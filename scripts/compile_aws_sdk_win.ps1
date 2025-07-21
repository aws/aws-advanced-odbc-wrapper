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

$CONFIGURATION = $args[0]   # Debug/Release

$CURRENT_DIR = (Get-Location).Path

$AWS_SDK_CPP_TAG = "1.11.594"

$SRC_DIR = "${PSScriptRoot}\..\aws_sdk\aws_sdk_cpp"
$BUILD_DIR = "${SRC_DIR}\..\build"
$INSTALL_DIR = "${BUILD_DIR}\..\install"

Write-Host $args

New-Item -Path $SRC_DIR -ItemType Directory -Force | Out-Null
git clone --recurse-submodules -b "$AWS_SDK_CPP_TAG" "https://github.com/aws/aws-sdk-cpp.git" $SRC_DIR

New-Item -Path $BUILD_DIR -ItemType Directory -Force | Out-Null
Set-Location $BUILD_DIR

cmake $SRC_DIR `
    -D CMAKE_BUILD_TYPE=$CONFIGURATION `
    -A "x64" `
    -D TARGET_ARCH="WINDOWS" `
    -D CMAKE_INSTALL_PREFIX=$INSTALL_DIR `
    -D BUILD_ONLY="rds;secretsmanager;sts" `
    -D ENABLE_TESTING="OFF" `
    -D CPP_STANDARD="20" `
    -D BUILD_SHARED_LIBS="OFF" `
    -D FORCE_SHARED_CRT="ON"

cmake --build . --config $CONFIGURATION
cmake --install . --config $CONFIGURATION

Set-Location $CURRENT_DIR
