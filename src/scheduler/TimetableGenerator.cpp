#include "TimetableGenerator.h"
#include <algorithm>
#include <limits>
#include <sstream>

namespace catalyst {

TimetableGenerator::TimetableGenerator(ConflictValidator &validator, GeneratorConfig config)
    : m_validator(validator), m_config(config) {}

std::vector<LectureInstance> TimetableGenerator::createInstancesFromRequirements(
    const std::vector<LectureRequirement> &requirements) {
    std::vector<LectureInstance> instances;
    int nextId = 1;

    for (const auto &req : requirements) {
        int duration = req.slotsPerInstance();
        for (int k = 0; k < req.lecturesPerWeek; ++k) {
            LectureInstance inst;
            inst.instanceId = nextId++;
            inst.requirementId = req.requirementId;
            inst.sectionId = req.sectionId;
            inst.subjectId = req.subjectId;
            inst.teacherId = req.teacherId;
            inst.subjectType = req.subjectType;
            inst.studentCount = req.studentCount;
            inst.durationSlots = duration;
            inst.instanceIndex = k;
            inst.subjectName = req.subjectName;
            inst.sectionName = req.sectionName;
            inst.teacherName = req.teacherName;
            instances.push_back(inst);
        }
    }

    return instances;
}

GeneratorStats TimetableGenerator::generate(const std::vector<LectureRequirement> &requirements,
                                           SchedulingState &outState) {
    std::vector<LectureInstance> instances = createInstancesFromRequirements(requirements);
    return generateFromInstances(instances, outState);
}

GeneratorStats TimetableGenerator::generateFromInstances(const std::vector<LectureInstance> &instances,
                                                         SchedulingState &outState) {
    m_startTime = std::chrono::steady_clock::now();
    m_stats = GeneratorStats{};
    m_stats.totalInstances = static_cast<int>(instances.size());
    outState.allocations.clear();
    outState.unassignedInstanceIds.clear();
    m_validator.clear();

    if (instances.empty()) {
        m_stats.status = GeneratorStatus::InvalidInput;
        m_stats.message = "No lecture instances provided to schedule.";
        return m_stats;
    }

    std::unordered_map<int, LectureInstance> instanceMap;
    std::vector<int> unassignedIds;
    unassignedIds.reserve(instances.size());

    for (const auto &inst : instances) {
        instanceMap[inst.instanceId] = inst;
        unassignedIds.push_back(inst.instanceId);
    }

    outState.unassignedInstanceIds = unassignedIds;

    resetTracking();
    SchedulingState bestState;
    int bestPenalty = std::numeric_limits<int>::max();

    GeneratorStatus status = solve(outState, unassignedIds, instanceMap, instances, bestState, bestPenalty);

    if (bestPenalty < std::numeric_limits<int>::max()) {
        outState = bestState;
        m_stats.qualityPenaltyScore = bestPenalty;
        status = GeneratorStatus::Success;
    }

    auto endTime = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - m_startTime;
    m_stats.durationMs = elapsed.count();
    m_stats.status = status;
    m_stats.allocatedInstances = static_cast<int>(outState.allocations.size());

    if (status == GeneratorStatus::Success) {
        std::string valError;
        bool valid = validateCompleteSchedule(instances, outState, m_validator, &valError);
        if (!valid) {
            m_stats.status = GeneratorStatus::NoSolution;
            m_stats.message = "Independent schedule verification failed: " + valError;
            outState.allocations.clear();
            m_validator.clear();
        } else {
            m_stats.message = "Schedule successfully generated and verified.";
        }
    } else if (status == GeneratorStatus::SearchLimitReached) {
        m_stats.message = "Search limit reached (node or time limit exceeded).";
        outState.allocations.clear();
        m_validator.clear();
    } else {
        m_stats.message = "No conflict-free schedule exists for the given inputs and constraints.";
        outState.allocations.clear();
        m_validator.clear();
    }

    return m_stats;
}

std::vector<Candidate> TimetableGenerator::getValidCandidates(const LectureInstance &instance) const {
    std::vector<Candidate> candidates;

    for (const auto &slot : m_validator.getAllTimeSlots()) {
        if (slot.isBreak) continue;

        if (instance.durationSlots == 2) {
            const TimeSlot *nextSlot = m_validator.findTimeSlot(slot.day, slot.slotIndex + 1);
            if (!nextSlot || nextSlot->isBreak) continue;
        }

        for (const auto &[roomId, room] : m_validator.getAllRooms()) {
            if (!m_validator.isRoomSuitable(room, instance)) continue;
            if (m_validator.isValidAllocation(instance, slot, room)) {
                candidates.push_back({&slot, &room, 0});
            }
        }
    }

    return candidates;
}

int TimetableGenerator::calculateDegree(const LectureInstance &var,
                                        const std::vector<int> &unassignedIds,
                                        const std::unordered_map<int, LectureInstance> &instanceMap) const {
    int degree = 0;
    for (int otherId : unassignedIds) {
        if (otherId == var.instanceId) continue;
        const auto &other = instanceMap.at(otherId);
        if (other.teacherId == var.teacherId ||
            other.sectionId == var.sectionId ||
            other.subjectType == var.subjectType) {
            degree++;
        }
    }
    return degree;
}

int TimetableGenerator::selectNextVariable(const std::vector<int> &unassignedIds,
                                           const std::unordered_map<int, LectureInstance> &instanceMap,
                                           std::vector<Candidate> &outCandidates) const {
    int bestIdx = -1;
    std::size_t minDomainSize = std::numeric_limits<std::size_t>::max();
    int maxDegree = -1;
    std::vector<Candidate> bestCandidates;

    for (std::size_t i = 0; i < unassignedIds.size(); ++i) {
        const auto &inst = instanceMap.at(unassignedIds[i]);
        auto cands = getValidCandidates(inst);

        // Immediate domain wipeout -> fail-first detection
        if (cands.empty()) {
            outCandidates.clear();
            return static_cast<int>(i);
        }

        int deg = calculateDegree(inst, unassignedIds, instanceMap);

        if (cands.size() < minDomainSize || (cands.size() == minDomainSize && deg > maxDegree)) {
            minDomainSize = cands.size();
            maxDegree = deg;
            bestIdx = static_cast<int>(i);
            bestCandidates = std::move(cands);
        }
    }

    outCandidates = std::move(bestCandidates);
    return bestIdx;
}

void TimetableGenerator::resetTracking() {
    m_sectionDaySubjectCount.clear();
    m_sectionDaySlotCount.clear();
    m_teacherDaySlotCount.clear();
    m_sectionSlotRoomTeacher.clear();
}

void TimetableGenerator::applyTracking(const LectureInstance &inst, const Allocation &alloc) {
    int sec = inst.sectionId;
    int day = alloc.day;
    int sub = inst.subjectId;
    int tchr = inst.teacherId;

    m_sectionDaySubjectCount[sec][day][sub]++;
    m_sectionDaySlotCount[sec][day] += alloc.durationSlots;
    m_teacherDaySlotCount[tchr][day] += alloc.durationSlots;

    for (int k = 0; k < alloc.durationSlots; ++k) {
        m_sectionSlotRoomTeacher[sec][day][alloc.startSlotIndex + k] = {alloc.roomId, tchr};
    }
}

void TimetableGenerator::revertTracking(const LectureInstance &inst, const Allocation &alloc) {
    int sec = inst.sectionId;
    int day = alloc.day;
    int sub = inst.subjectId;
    int tchr = inst.teacherId;

    auto secIt = m_sectionDaySubjectCount.find(sec);
    if (secIt != m_sectionDaySubjectCount.end()) {
        auto dayIt = secIt->second.find(day);
        if (dayIt != secIt->second.end()) {
            dayIt->second[sub]--;
            if (dayIt->second[sub] <= 0) {
                dayIt->second.erase(sub);
            }
        }
    }

    m_sectionDaySlotCount[sec][day] -= alloc.durationSlots;
    if (m_sectionDaySlotCount[sec][day] <= 0) {
        m_sectionDaySlotCount[sec].erase(day);
    }

    m_teacherDaySlotCount[tchr][day] -= alloc.durationSlots;
    if (m_teacherDaySlotCount[tchr][day] <= 0) {
        m_teacherDaySlotCount[tchr].erase(day);
    }

    for (int k = 0; k < alloc.durationSlots; ++k) {
        m_sectionSlotRoomTeacher[sec][day].erase(alloc.startSlotIndex + k);
    }
}

int TimetableGenerator::calculateCandidatePenalty(
    const LectureInstance &inst,
    const TimeSlot &slot,
    const RoomInfo &room,
    const std::vector<int> &unassignedIds,
    const std::unordered_map<int, LectureInstance> &instanceMap) const {

    if (!m_config.enableOptimization) {
        return 0;
    }

    int penalty = 0;
    int day = slot.day;
    int start = slot.slotIndex;
    int dur = inst.durationSlots;
    int sec = inst.sectionId;
    int tchr = inst.teacherId;
    int sub = inst.subjectId;

    // 1. Avoid multiple lectures of same subject on same day for this section
    auto secIt = m_sectionDaySubjectCount.find(sec);
    if (secIt != m_sectionDaySubjectCount.end()) {
        auto dayIt = secIt->second.find(day);
        if (dayIt != secIt->second.end()) {
            auto subIt = dayIt->second.find(sub);
            if (subIt != dayIt->second.end() && subIt->second > 0) {
                penalty += subIt->second * 2000;
            }
        }
    }

    // 2. Avoid assigning a teacher more than two consecutive occupied teaching slots
    bool tchrPrev1 = m_validator.isTeacherOccupied(tchr, day, start - 1);
    bool tchrPrev2 = (start >= 2) ? m_validator.isTeacherOccupied(tchr, day, start - 2) : false;
    bool tchrNext1 = m_validator.isTeacherOccupied(tchr, day, start + dur);
    bool tchrNext2 = m_validator.isTeacherOccupied(tchr, day, start + dur + 1);

    if (dur == 1) { // Single 1-hour period
        if (tchrPrev1 && tchrPrev2) penalty += 2500;
        if (tchrPrev1 && tchrNext1) penalty += 2500;
        if (tchrNext1 && tchrNext2) penalty += 2500;

        if (tchrPrev1 && !tchrPrev2) penalty += 20;
        if (tchrNext1 && !tchrNext2) penalty += 20;
    } else { // 2-hour lab session (durationSlots == 2)
        if (tchrPrev1) penalty += 2500;
        if (tchrNext1) penalty += 2500;
    }

    // 3. Teacher weekly workload distribution across weekdays
    auto tDayIt = m_teacherDaySlotCount.find(tchr);
    if (tDayIt != m_teacherDaySlotCount.end()) {
        auto dIt = tDayIt->second.find(day);
        int tSlotsToday = (dIt != tDayIt->second.end()) ? dIt->second : 0;
        if (tSlotsToday >= 3) {
            penalty += (tSlotsToday - 2) * 250;
        } else {
            penalty += tSlotsToday * 25;
        }
    }

    // 4. Section daily workload distribution across available days
    auto sDayIt = m_sectionDaySlotCount.find(sec);
    int sSlotsToday = 0;
    if (sDayIt != m_sectionDaySlotCount.end()) {
        auto dIt = sDayIt->second.find(day);
        sSlotsToday = (dIt != sDayIt->second.end()) ? dIt->second : 0;
    }

    if (sSlotsToday == 0) {
        penalty -= 120; // Strong bonus for spreading into empty days (e.g. Monday)!
    } else if (sSlotsToday >= 4) {
        penalty += (sSlotsToday - 3) * 300;
    } else {
        penalty += sSlotsToday * 20;
    }

    // 5. Classroom consistency for consecutive periods with same teacher
    auto sRoomIt = m_sectionSlotRoomTeacher.find(sec);
    if (sRoomIt != m_sectionSlotRoomTeacher.end()) {
        auto dIt = sRoomIt->second.find(day);
        if (dIt != sRoomIt->second.end()) {
            auto prevSlotIt = dIt->second.find(start - 1);
            if (prevSlotIt != dIt->second.end()) {
                int prevRoom = prevSlotIt->second.first;
                int prevTchr = prevSlotIt->second.second;
                if (prevTchr == tchr) {
                    if (room.id == prevRoom) penalty -= 90; // Keep same room!
                    else penalty += 80;                     // Unnecessary classroom change
                }
            }

            auto nextSlotIt = dIt->second.find(start + dur);
            if (nextSlotIt != dIt->second.end()) {
                int nextRoom = nextSlotIt->second.first;
                int nextTchr = nextSlotIt->second.second;
                if (nextTchr == tchr) {
                    if (room.id == nextRoom) penalty -= 90; // Keep same room!
                    else penalty += 80;                     // Unnecessary classroom change
                }
            }
        }
    }

    // 6. Section schedule continuity
    if (sSlotsToday > 0) {
        bool hasAdjacent = false;
        if (sRoomIt != m_sectionSlotRoomTeacher.end()) {
            auto dIt = sRoomIt->second.find(day);
            if (dIt != sRoomIt->second.end()) {
                if (dIt->second.find(start - 1) != dIt->second.end() ||
                    dIt->second.find(start + dur) != dIt->second.end()) {
                    hasAdjacent = true;
                }
            }
        }
        if (hasAdjacent) {
            penalty -= 35;
        }
    }

    // 7. LCV Tie-Breaker
    if (m_config.enableLcv) {
        int lcvScore = 0;
        for (int otherId : unassignedIds) {
            if (otherId == inst.instanceId) continue;
            auto oIt = instanceMap.find(otherId);
            if (oIt == instanceMap.end()) continue;
            const auto &other = oIt->second;

            if (other.teacherId == tchr) lcvScore += 2;
            if (other.sectionId == sec) lcvScore += 2;
            if (m_validator.isRoomSuitable(room, other)) lcvScore += 1;
        }
        penalty += lcvScore;
    }

    return penalty;
}

int TimetableGenerator::evaluateSchedulePenalty(
    const SchedulingState &state,
    const std::vector<LectureInstance> &instances) const {

    int totalPenalty = 0;

    std::unordered_map<int, LectureInstance> instMap;
    for (const auto &inst : instances) {
        instMap[inst.instanceId] = inst;
    }

    std::unordered_map<int, std::unordered_map<int, std::vector<int>>> teacherOccupiedSlots;
    struct SlotDetail { int slotIndex; int subjectId; int teacherId; int roomId; SubjectType type; };
    std::unordered_map<int, std::unordered_map<int, std::vector<SlotDetail>>> sectionOccupiedSlots;

    for (const auto &[instId, alloc] : state.allocations) {
        auto it = instMap.find(instId);
        if (it == instMap.end()) continue;
        const auto &inst = it->second;

        for (int k = 0; k < alloc.durationSlots; ++k) {
            int slot = alloc.startSlotIndex + k;
            teacherOccupiedSlots[inst.teacherId][alloc.day].push_back(slot);
            sectionOccupiedSlots[inst.sectionId][alloc.day].push_back(
                {slot, inst.subjectId, inst.teacherId, alloc.roomId, inst.subjectType});
        }
    }

    // 1. Teacher consecutive teaching periods (> 2 in a row)
    for (auto &[tchr, dayMap] : teacherOccupiedSlots) {
        for (auto &[day, slots] : dayMap) {
            std::sort(slots.begin(), slots.end());
            int run = 1;
            for (size_t i = 1; i < slots.size(); ++i) {
                if (slots[i] == slots[i - 1] + 1) {
                    run++;
                    if (run >= 3) {
                        totalPenalty += 3000;
                    }
                } else {
                    run = 1;
                }
            }
        }
    }

    // 2. Section daily subject distribution & room consistency
    for (auto &[sec, dayMap] : sectionOccupiedSlots) {
        int daysUsed = 0;
        for (int d = 0; d < 5; ++d) {
            auto dIt = dayMap.find(d);
            int count = (dIt != dayMap.end()) ? static_cast<int>(dIt->second.size()) : 0;
            if (count > 0) daysUsed++;

            int diff = count - 4;
            totalPenalty += diff * diff * 60;
        }

        if (daysUsed < 5) {
            totalPenalty += (5 - daysUsed) * 800;
        }

        for (auto &[day, slotList] : dayMap) {
            std::unordered_map<int, int> subjectCounts;
            for (const auto &sd : slotList) {
                if (sd.type == SubjectType::Theory) {
                    subjectCounts[sd.subjectId]++;
                }
            }
            for (auto &[subId, cnt] : subjectCounts) {
                if (cnt > 1) {
                    totalPenalty += (cnt - 1) * 2000;
                }
            }

            std::sort(slotList.begin(), slotList.end(), [](const SlotDetail &a, const SlotDetail &b) {
                return a.slotIndex < b.slotIndex;
            });
            for (size_t i = 1; i < slotList.size(); ++i) {
                if (slotList[i].slotIndex == slotList[i - 1].slotIndex + 1) {
                    if (slotList[i].teacherId == slotList[i - 1].teacherId &&
                        slotList[i].roomId != slotList[i - 1].roomId) {
                        totalPenalty += 150;
                    }
                }
            }
        }
    }

    return totalPenalty;
}

void TimetableGenerator::orderCandidatesLcv(const LectureInstance &instance,
                                            std::vector<Candidate> &candidates,
                                            const std::vector<int> &unassignedIds,
                                            const std::unordered_map<int, LectureInstance> &instanceMap) const {
    if (candidates.size() <= 1) {
        return;
    }

    for (auto &cand : candidates) {
        cand.lcvScore = calculateCandidatePenalty(instance, *cand.startSlot, *cand.room, unassignedIds, instanceMap);
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.lcvScore < b.lcvScore;
    });
}

bool TimetableGenerator::forwardCheck(const std::vector<int> &unassignedIds,
                                      const std::unordered_map<int, LectureInstance> &instanceMap) const {
    for (int otherId : unassignedIds) {
        const auto &other = instanceMap.at(otherId);
        bool hasAtLeastOne = false;

        for (const auto &slot : m_validator.getAllTimeSlots()) {
            if (slot.isBreak) continue;
            if (other.durationSlots == 2) {
                const TimeSlot *nextSlot = m_validator.findTimeSlot(slot.day, slot.slotIndex + 1);
                if (!nextSlot || nextSlot->isBreak) continue;
            }

            for (const auto &[roomId, room] : m_validator.getAllRooms()) {
                if (m_validator.isRoomSuitable(room, other) &&
                    m_validator.isValidAllocation(other, slot, room)) {
                    hasAtLeastOne = true;
                    break;
                }
            }
            if (hasAtLeastOne) break;
        }

        if (!hasAtLeastOne) {
            return false; // Domain wipeout detected
        }
    }

    return true;
}

GeneratorStatus TimetableGenerator::solve(SchedulingState &state,
                                         std::vector<int> &unassignedIds,
                                         const std::unordered_map<int, LectureInstance> &instanceMap,
                                         const std::vector<LectureInstance> &allInstances,
                                         SchedulingState &bestState,
                                         int &bestPenalty) {
    if (unassignedIds.empty()) {
        int currentPenalty = evaluateSchedulePenalty(state, allInstances);
        m_stats.solutionsFound++;

        if (currentPenalty < bestPenalty) {
            bestPenalty = currentPenalty;
            bestState = state;
        }

        if (bestPenalty <= m_config.targetQualityPenalty ||
            m_stats.solutionsFound >= m_config.maxSolutionsToExplore) {
            return GeneratorStatus::Success;
        }
        return GeneratorStatus::NoSolution; // Continue search to find better solution
    }

    m_stats.nodesExplored++;
    if (m_stats.nodesExplored >= m_config.maxNodes) {
        return GeneratorStatus::SearchLimitReached;
    }
    if (isTimeLimitExceeded()) {
        return GeneratorStatus::SearchLimitReached;
    }

    std::vector<Candidate> candidates;
    int chosenIdx = selectNextVariable(unassignedIds, instanceMap, candidates);
    if (chosenIdx < 0 || candidates.empty()) {
        return GeneratorStatus::NoSolution;
    }

    int chosenId = unassignedIds[chosenIdx];
    const auto &instance = instanceMap.at(chosenId);

    // Remove chosen variable from unassigned list
    unassignedIds.erase(unassignedIds.begin() + chosenIdx);

    // Order candidates using composite quality penalty + LCV
    orderCandidatesLcv(instance, candidates, unassignedIds, instanceMap);

    for (const auto &cand : candidates) {
        if (m_stats.nodesExplored >= m_config.maxNodes || isTimeLimitExceeded()) {
            unassignedIds.insert(unassignedIds.begin() + chosenIdx, chosenId);
            return GeneratorStatus::SearchLimitReached;
        }

        Allocation alloc;
        if (!m_validator.allocate(instance, *cand.startSlot, *cand.room, &alloc)) {
            continue;
        }

        state.allocations[chosenId] = alloc;
        applyTracking(instance, alloc);

        bool fcPassed = true;
        if (m_config.enableForwardChecking) {
            fcPassed = forwardCheck(unassignedIds, instanceMap);
        }

        if (fcPassed) {
            GeneratorStatus subStatus = solve(state, unassignedIds, instanceMap, allInstances, bestState, bestPenalty);
            if (subStatus == GeneratorStatus::Success) {
                return GeneratorStatus::Success;
            }
            if (subStatus == GeneratorStatus::SearchLimitReached) {
                revertTracking(instance, alloc);
                m_validator.deallocate(instance, alloc);
                state.allocations.erase(chosenId);
                unassignedIds.insert(unassignedIds.begin() + chosenIdx, chosenId);
                return GeneratorStatus::SearchLimitReached;
            }
        }

        // Clean rollback during backtracking
        revertTracking(instance, alloc);
        m_validator.deallocate(instance, alloc);
        state.allocations.erase(chosenId);
        m_stats.backtracks++;
    }

    // Restore unassigned list
    unassignedIds.insert(unassignedIds.begin() + chosenIdx, chosenId);
    return GeneratorStatus::NoSolution;
}

bool TimetableGenerator::isTimeLimitExceeded() const {
    if (m_config.timeLimitMs <= 0) return false;
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed = now - m_startTime;
    return elapsed.count() >= m_config.timeLimitMs;
}

bool TimetableGenerator::validateCompleteSchedule(const std::vector<LectureInstance> &instances,
                                                 const SchedulingState &state,
                                                 const ConflictValidator &validator,
                                                 std::string *errorOut) {
    if (state.allocations.size() != instances.size()) {
        if (errorOut) {
            std::ostringstream ss;
            ss << "Allocation count mismatch: expected " << instances.size()
               << ", actual " << state.allocations.size();
            *errorOut = ss.str();
        }
        return false;
    }

    std::unordered_map<int, LectureInstance> instMap;
    for (const auto &inst : instances) {
        instMap[inst.instanceId] = inst;
    }

    // Verify every instance has an allocation with valid room and slots
    for (const auto &inst : instances) {
        auto optAlloc = state.getAllocation(inst.instanceId);
        if (!optAlloc) {
            if (errorOut) *errorOut = "Instance ID " + std::to_string(inst.instanceId) + " is unallocated.";
            return false;
        }

        const auto &alloc = *optAlloc;
        if (alloc.durationSlots != inst.durationSlots) {
            if (errorOut) *errorOut = "Instance duration mismatch for ID " + std::to_string(inst.instanceId);
            return false;
        }

        const RoomInfo *room = validator.findRoom(alloc.roomId);
        if (!room) {
            if (errorOut) *errorOut = "Invalid room ID " + std::to_string(alloc.roomId);
            return false;
        }

        if (!validator.isRoomSuitable(*room, inst, errorOut)) {
            return false;
        }

        for (int k = 0; k < alloc.durationSlots; ++k) {
            const TimeSlot *slot = validator.findTimeSlot(alloc.day, alloc.startSlotIndex + k);
            if (!slot) {
                if (errorOut) *errorOut = "Slot does not exist at day " + std::to_string(alloc.day) + " index " + std::to_string(alloc.startSlotIndex + k);
                return false;
            }
            if (slot->isBreak) {
                if (errorOut) *errorOut = "Allocation overlaps break at day " + std::to_string(alloc.day) + " index " + std::to_string(slot->slotIndex);
                return false;
            }
        }
    }

    // Pairwise overlap check across all allocations
    const auto &allocs = state.allocations;
    for (auto it1 = allocs.begin(); it1 != allocs.end(); ++it1) {
        const auto &a1 = it1->second;
        const auto &inst1 = instMap.at(a1.instanceId);

        auto it2 = it1;
        ++it2;
        for (; it2 != allocs.end(); ++it2) {
            const auto &a2 = it2->second;
            const auto &inst2 = instMap.at(a2.instanceId);

            if (a1.day != a2.day) continue;

            int a1Start = a1.startSlotIndex;
            int a1End = a1.startSlotIndex + a1.durationSlots - 1;
            int a2Start = a2.startSlotIndex;
            int a2End = a2.startSlotIndex + a2.durationSlots - 1;

            bool overlaps = !(a1End < a2Start || a2End < a1Start);
            if (!overlaps) continue;

            if (inst1.teacherId == inst2.teacherId) {
                if (errorOut) {
                    std::ostringstream ss;
                    ss << "Teacher clash on day " << a1.day << " between instance "
                       << inst1.instanceId << " and " << inst2.instanceId
                       << " (Teacher ID " << inst1.teacherId << ")";
                    *errorOut = ss.str();
                }
                return false;
            }

            if (inst1.sectionId == inst2.sectionId) {
                if (errorOut) {
                    std::ostringstream ss;
                    ss << "Section clash on day " << a1.day << " between instance "
                       << inst1.instanceId << " and " << inst2.instanceId
                       << " (Section ID " << inst1.sectionId << ")";
                    *errorOut = ss.str();
                }
                return false;
            }

            if (a1.roomId == a2.roomId) {
                if (errorOut) {
                    std::ostringstream ss;
                    ss << "Room clash on day " << a1.day << " between instance "
                       << inst1.instanceId << " and " << inst2.instanceId
                       << " (Room ID " << a1.roomId << ")";
                    *errorOut = ss.str();
                }
                return false;
            }
        }
    }

    return true;
}

} // namespace catalyst

