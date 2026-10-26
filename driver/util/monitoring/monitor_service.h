// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MONITOR_SERVICE_H_
#define MONITOR_SERVICE_H_

// Owns the background monitors shared across connections. Owned by CoreService and never destroyed.
// TODO - Empty until the monitors move here
class MonitorService {
public:
    MonitorService() = default;

    // Stops and joins every monitor, then frees the ENVs they used. Called by CoreService when the last ENV of the
    // app or Driver Manager is freed, so:
    // - Runs under CoreService's lock. Monitors must use ENVs that are not counted, see RDS_AllocInternalEnv,
    //   or this would deadlock
    // - Nothing may keep running afterwards, the DLL may be unloaded
    // - Must leave the service usable, the next connect creates monitors again
    // codechecker_suppress [readability-convert-member-functions-to-static]
    void ReleaseResources() {}
};

#endif // MONITOR_SERVICE_H_
