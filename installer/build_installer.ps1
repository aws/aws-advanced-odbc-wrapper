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

$CONFIGURATION = $args[0] # Debug/Release

$CURRENT_DIR = (Get-Location).Path

# Build Driver
Set-Location ${PSScriptRoot}/.. # Project Root
Write-Host "Building Driver"
cmake -S . -B build -DBUILD_ANSI=ON -DBUILD_UNICODE=ON -DBUILD_UNIT_TEST=OFF
cmake --build build --config $CONFIGURATION
Write-Host "Built Driver"

# Build Installer
Write-Host "Building Installer"
Set-Location ${PSScriptRoot}
wix build .\Package.wxs .\Package.en-us.wxl -d "CONFIG=$CONFIGURATION" -o aws-advanced-odbc-wrapper.msi -arch x64 -ext WixToolset.UI.wixext

Set-Location ${CURRENT_DIR}
Write-Host "Finished"
