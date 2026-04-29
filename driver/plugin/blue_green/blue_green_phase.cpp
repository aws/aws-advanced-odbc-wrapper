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

#include "blue_green_phase.h"

#include <algorithm>
#include <map>
#include <string>

BlueGreenPhase::BlueGreenPhase() :BlueGreenPhase(BlueGreenPhase::UNKNOWN) {}

BlueGreenPhase::BlueGreenPhase(Phase phase) : BlueGreenPhase(phase, BlueGreenPhase::PHASE_STATE_MAPPING.at(phase)) {}

BlueGreenPhase::BlueGreenPhase(Phase phase, bool switchoverOrCompleted) :
    phase_{ phase },
    switchover_or_completed_{ switchoverOrCompleted } {}

BlueGreenPhase::Phase BlueGreenPhase::GetPhase() const {
    return this->phase_;
}

bool BlueGreenPhase::IsSwitchoverOrCompleted() const {
    return this->switchover_or_completed_;
}

BlueGreenPhase BlueGreenPhase::ParsePhase(std::string value, std::string version) {
    BlueGreenPhase::Phase phase = BlueGreenPhase::Phase::UNKNOWN;

    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::toupper(c); });
    auto itr = BlueGreenPhase::STR_TO_PHASE_MAPPING.find(value);
    if (itr != BlueGreenPhase::STR_TO_PHASE_MAPPING.end()) {
        phase = itr->second;
    }

    return {
        phase,
        BlueGreenPhase::PHASE_STATE_MAPPING.at(phase)
    };
}

std::string BlueGreenPhase::ToString() {
    auto itr = BlueGreenPhase::PHASE_TO_STR_MAPPING.find(this->phase_);
    if (itr != BlueGreenPhase::PHASE_TO_STR_MAPPING.end()) {
        return itr->second;
    }
    return {};
}
