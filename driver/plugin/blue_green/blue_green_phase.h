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

#ifndef BLUE_GREEN_PHASE_H_
#define BLUE_GREEN_PHASE_H_

#include <map>
#include <string>

class BlueGreenPhase {
public:
    typedef enum {
        UNKNOWN,
        NOT_CREATED,
        CREATED,
        PREPARATION,
        IN_PROGRESS,
        POST,
        COMPLETED
    } Phase;

    static inline std::map<Phase, bool> const PHASE_STATE_MAPPING = {
        {Phase::NOT_CREATED,    false},
        {Phase::CREATED,        false},
        {Phase::PREPARATION,    true},
        {Phase::IN_PROGRESS,    true},
        {Phase::POST,           true},
        {Phase::COMPLETED,      true},
        {Phase::UNKNOWN,        false}
    };

    static inline std::map<Phase, std::string> const PHASE_TO_STR_MAPPING = {
        {Phase::NOT_CREATED,   "NOT_CREATED"},
        {Phase::CREATED,       "CREATED"},
        {Phase::PREPARATION,   "PREPARATION"},
        {Phase::IN_PROGRESS,   "IN_PROGRESS"},
        {Phase::POST,          "POST"},
        {Phase::COMPLETED,     "COMPLETED"},
        {Phase::UNKNOWN,       "UNKNOWN"}
    };

    static inline std::map<std::string, Phase> const STR_TO_PHASE_MAPPING = {
        {"AVAILABLE",                       Phase::CREATED},
        {"SWITCHOVER_INITIATED",            Phase::PREPARATION},
        {"SWITCHOVER_IN_PROGRESS",          Phase::IN_PROGRESS},
        {"SWITCHOVER_IN_POST_PROCESSING",   Phase::POST},
        {"SWITCHOVER_COMPLETED",            Phase::COMPLETED}
    };
    BlueGreenPhase();
    BlueGreenPhase(Phase phase);
    BlueGreenPhase(Phase phase, bool switchover_or_completed);

    Phase GetPhase() const;
    bool IsSwitchoverOrCompleted() const;

    static BlueGreenPhase ParsePhase(std::string value, std::string version);

    std::strong_ordering operator<=>(const BlueGreenPhase& other) const {
        return phase_ <=> other.phase_;
    }

    bool operator==(const BlueGreenPhase& other) const {
        return phase_ == other.phase_;
    }

    std::string ToString();

private:
    Phase phase_ = UNKNOWN;
    bool switchover_or_completed_ = false;
};

#endif // BLUE_GREEN_PHASE_H_
