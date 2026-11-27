# Understanding the CLUSTER_ID Parameter

## Overview

The `CLUSTER_ID` parameter identifies the database cluster a connection belongs to. The AWS Advanced ODBC Wrapper uses it as the key for state that is shared by every connection to the same cluster within a process. It must be set when **one application process connects to more than one database cluster**.

## What is a Cluster?

A cluster is a group of database instances that share one topology and one failover domain: one writer instance and zero or more reader instances. The wrapper tracks the topology of the cluster and reconnects to one of its instances when a failover is detected.

Examples of clusters:

- An Aurora DB cluster (one writer and multiple readers)
- An RDS Multi-AZ DB cluster (one writer and two readers)
- An Aurora Limitless Database

> **Rule of thumb:** If the wrapper should track topology separately and fail over independently, use different `CLUSTER_ID` values. If instances share the same topology and failover domain, use the same `CLUSTER_ID`.

## How CLUSTER_ID is Used

Connections with the same `CLUSTER_ID` share:

- The topology cache, the list of instances in the cluster and their roles
- The topology monitor, which keeps the topology cache up to date and detects failover
- The Limitless router monitor, when the [Limitless Plugin](./plugins/limitless-plugin.md) is enabled

Sharing avoids redundant topology queries and monitoring threads. Custom endpoint member lists are not keyed by `CLUSTER_ID`: they belong to the custom endpoint a connection was opened with, so connections to the same cluster through different custom endpoints can share a `CLUSTER_ID` and each still uses only the instances of its own endpoint.

## Why the Wrapper Does Not Derive the Cluster from the Host

A connection can reach the same cluster through many kinds of hosts:

- A cluster or reader cluster endpoint: `my-cluster.cluster-abc123.us-east-1.rds.amazonaws.com`
- An instance endpoint: `instance-1.abc123.us-east-1.rds.amazonaws.com`
- A custom endpoint: `my-custom-endpoint.cluster-custom-abc123.us-east-1.rds.amazonaws.com`
- An RDS Proxy: `my-proxy.proxy-abc123.us-east-1.rds.amazonaws.com`
- An IP address or a custom domain name: `10.0.1.50`, `db.mycompany.com`

All of these can point at the same cluster, and two clusters in different regions or accounts can have the same name. The wrapper cannot reliably identify the cluster from the host, so **it is up to the application to provide `CLUSTER_ID`**. When it is not set, the wrapper uses the default value `1`.

## When to Set CLUSTER_ID

### Required: Multiple Clusters in One Process

Set a distinct `CLUSTER_ID` for every cluster when the application connects to more than one cluster:

```
# Source cluster
DRIVER={AWS Advanced ODBC Wrapper Ansi};SERVER=source-db.cluster-abc123.us-east-1.rds.amazonaws.com;PORT=5432;DATABASE=mydb;UID=admin;PWD=***;CLUSTER_ID=source-cluster;ENABLE_CLUSTER_FAILOVER=1;...

# Destination cluster, a different CLUSTER_ID
DRIVER={AWS Advanced ODBC Wrapper Ansi};SERVER=dest-db.cluster-def456.us-west-2.rds.amazonaws.com;PORT=5432;DATABASE=mydb;UID=admin;PWD=***;CLUSTER_ID=destination-cluster;ENABLE_CLUSTER_FAILOVER=1;...

# The source cluster again, through an IP address, the same CLUSTER_ID as the first connection
DRIVER={AWS Advanced ODBC Wrapper Ansi};SERVER=10.0.0.1;PORT=5432;DATABASE=mydb;UID=admin;PWD=***;CLUSTER_ID=source-cluster;HOST_PATTERN=?.abc123.us-east-1.rds.amazonaws.com;ENABLE_CLUSTER_FAILOVER=1;...
```

The same applies to DSNs. A single process, for example a reporting or BI tool, often has several DSNs open at once. If those DSNs point at different clusters, give each one its own Cluster ID, in the DSN setup dialog (Failover tab) or in the `odbc.ini` entry.

### Optional: Single Cluster Applications

An application that connects to only one cluster can omit `CLUSTER_ID`. Every connection then uses the default `1` and shares one topology cache, whichever host it connects through:

```
# Both connections use CLUSTER_ID=1
DRIVER={AWS Advanced ODBC Wrapper Ansi};SERVER=my-cluster.cluster-abc123.us-east-1.rds.amazonaws.com;...
DRIVER={AWS Advanced ODBC Wrapper Ansi};SERVER=my-cluster.cluster-ro-abc123.us-east-1.rds.amazonaws.com;...
```

## Critical Warnings

### Never Share a CLUSTER_ID Between Different Clusters

This includes leaving `CLUSTER_ID` unset on connections to different clusters, which gives them all the default `1`. Connections to different clusters with the same `CLUSTER_ID`:

- Overwrite each other's topology
- Can fail over to an instance of the wrong cluster
- Share one topology monitor, which then monitors only one of the clusters

### Use the Same CLUSTER_ID for the Same Cluster

Different `CLUSTER_ID` values for the same cluster are safe but wasteful: each value has its own topology cache and its own monitoring threads.

### A Shared CLUSTER_ID Also Shares the Monitor's Connection Settings

One topology monitor exists per `CLUSTER_ID`. It is created by whichever connection needs topology first, and it keeps that connection's attributes, including `UID`, `PWD` and `DATABASE`, for as long as it runs. Later connections with the same `CLUSTER_ID` reuse the monitor and cannot change those settings.

This does not matter when every connection sharing a `CLUSTER_ID` uses the same user and database. It does matter when they differ, for example when one process connects to several databases of one cluster, each with its own database user. If the user or database captured by the monitor is later removed or loses access, the monitor can no longer connect, and every connection sharing the `CLUSTER_ID` waits for a topology refresh that cannot complete.

If connections to the same cluster use different users or databases, and the user or database of one of them may be removed or lose access, give those connections different `CLUSTER_ID` values. Each value then has its own topology monitor that uses the settings of its own connections, at the cost of duplicate topology caches and monitoring threads.

## Upgrading from Earlier Versions

Earlier versions derived a cluster ID from the host when `CLUSTER_ID` was not set: the first label of a cluster, reader, custom, proxy or shard group endpoint, and a unique value per connection for any other host. Applications that connect to a single cluster need no changes. Applications that connect to more than one cluster must set a distinct `CLUSTER_ID` for each cluster.
