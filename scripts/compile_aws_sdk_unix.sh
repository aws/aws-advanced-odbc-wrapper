#!/usr/bin/env bash
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Fail the script (and therefore the CI step) as soon as any command fails,
# otherwise a broken SDK build is only reported much later as a missing SDK.
set -e

CONFIGURATION=$1    # Debug/Release

export ROOT_REPO_PATH=$(cd "$(dirname "$0")/.."; pwd -P)

export AWS_SDK_CPP_TAG="1.11.835"
# Commit the tag pointed to when it was vetted; git tags are mutable, so verify
# the clone landed on this exact commit before building.
export AWS_SDK_CPP_COMMIT="94e71dee7a4dcb21c31df2b9c8f3d49b337e0a83"

export AWS_SDK_PATH="${ROOT_REPO_PATH}/aws_sdk"
export SRC_DIR="${AWS_SDK_PATH}/aws_sdk_cpp"
export BUILD_DIR="${AWS_SDK_PATH}/build"
export INSTALL_DIR="${AWS_SDK_PATH}/install"

mkdir -p ${SRC_DIR} ${BUILD_DIR} ${INSTALL_DIR}
pushd $BUILD_DIR

if [ ! -d "${SRC_DIR}/.git" ]; then
    git clone --recurse-submodules -b "$AWS_SDK_CPP_TAG" "https://github.com/aws/aws-sdk-cpp.git" ${SRC_DIR}
fi

ACTUAL_COMMIT=$(git -C ${SRC_DIR} rev-parse HEAD)
if [ "$ACTUAL_COMMIT" != "$AWS_SDK_CPP_COMMIT" ]; then
    echo "ERROR: aws-sdk-cpp tag $AWS_SDK_CPP_TAG resolved to unexpected commit." >&2
    echo "  expected: $AWS_SDK_CPP_COMMIT" >&2
    echo "  actual:   $ACTUAL_COMMIT" >&2
    echo "The upstream tag may have been moved. Verify before updating AWS_SDK_CPP_COMMIT." >&2
    exit 1
fi

# Homebrew's default OpenSSL is now 4.x, which the s2n revision pinned by this SDK tag cannot compile against
# openssl@4 is not keg-only, so it lands in ${BREW}/{lib,include} and wins the libcrypto lookup.
# Pin the build to openssl@3's keg.
EXTRA_CMAKE_ARGS=()
if [ "$(uname -s)" = "Darwin" ]; then
    # Resolve the keg from the filesystem rather than via `brew --prefix openssl@3`
    # CodeBuild macOS fleets the Homebrew prefix is chowned to the buildspec's pre_build user
    # while the GitHub Actions step runs as root, and `brew` there returns nothing even when the formula is installed.
    OPENSSL_PREFIX=""
    for candidate in /opt/homebrew/opt/openssl@3 /usr/local/opt/openssl@3; do
        if [ -d "${candidate}/include/openssl" ]; then
            OPENSSL_PREFIX="${candidate}"
            break
        fi
    done

    # Fall back to asking brew, and surface its error if that fails too.
    if [ -z "${OPENSSL_PREFIX}" ]; then
        BREW_OPENSSL_PREFIX=$(brew --prefix openssl@3 2>&1) || true
        if [ -d "${BREW_OPENSSL_PREFIX}/include/openssl" ]; then
            OPENSSL_PREFIX="${BREW_OPENSSL_PREFIX}"
        else
            echo "brew --prefix openssl@3 did not yield a usable keg: ${BREW_OPENSSL_PREFIX}" >&2
        fi
    fi

    if [ -z "${OPENSSL_PREFIX}" ]; then
        echo "ERROR: openssl@3 is required to build the AWS SDK on macOS but was not found." >&2
        echo "The pinned s2n revision cannot compile against OpenSSL 4, which is what" >&2
        echo "\${BREW}/include provides by default. Install it with: brew install openssl@3" >&2
        exit 1
    fi

    echo "Pinning AWS SDK build to OpenSSL at ${OPENSSL_PREFIX}"
    EXTRA_CMAKE_ARGS+=(-D "OPENSSL_ROOT_DIR=${OPENSSL_PREFIX}")
    # crypto_ROOT is what s2n's cmake/modules/Findcrypto.cmake honours via
    # CMP0074; CMAKE_PREFIX_PATH is the HINTS list that same module searches.
    EXTRA_CMAKE_ARGS+=(-D "crypto_ROOT=${OPENSSL_PREFIX}")
    EXTRA_CMAKE_ARGS+=(-D "CMAKE_PREFIX_PATH=${OPENSSL_PREFIX}")
fi

cmake -S ${SRC_DIR} \
    -B $BUILD_DIR \
    "${EXTRA_CMAKE_ARGS[@]}" \
    -D CMAKE_BUILD_TYPE="${CONFIGURATION}" \
    -D CMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -D BUILD_ONLY="rds;secretsmanager;sts;sso;sso-oidc" \
    -D ENABLE_TESTING="OFF" \
    -D CPP_STANDARD="20" \
    -D BUILD_SHARED_LIBS="ON" \
    -D ENABLE_UNITY_BUILD="ON"

cmake --build . --config=${CONFIGURATION}
cmake --install . --config=${CONFIGURATION}

popd
