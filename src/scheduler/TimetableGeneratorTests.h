#ifndef TIMETABLE_GENERATOR_TESTS_H
#define TIMETABLE_GENERATOR_TESTS_H

#include "TimetableGenerator.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace catalyst {

inline bool runTimetableGeneratorTests(std::string *outputReport = nullptr) {
    std::ostringstream out;
    out << "========================================\n";
    out << "   TimetableGenerator CSP Unit Tests    \n";
    out << "========================================\n";

    int totalTests = 0;
    int passedTests = 0;

    auto assertTest = [&](bool condition, const std::string &testName) {
        totalTests++;
        if (condition) {
            passedTests++;
            out << "[PASS] " << testName << "\n";
        } else {
            out << "[FAIL] " << testName << "\n";
        }
    };

    // Helper to build standard 5-day, 7-slot schedule (slot 3 is break)
    auto createStandardTimeslots = []() -> std::vector<TimeSlot> {
        std::vector<TimeSlot> slotList;
        int id = 1;
        for (int day = 0; day < 5; ++day) {
            for (int slotIdx = 0; slotIdx < 7; ++slotIdx) {
                TimeSlot ts;
                ts.id = id++;
                ts.day = day;
                ts.slotIndex = slotIdx;
                ts.isBreak = (slotIdx == 3);
                slotList.push_back(ts);
            }
        }
        return slotList;
    };

    // Test 1: Small Satisfiable Problem
    {
        ConflictValidator validator;
        validator.setTimeSlots(createStandardTimeslots());
        validator.addRoom({1, "CS-101", 60, RoomType::Classroom});
        validator.addRoom({2, "CS-102", 60, RoomType::Classroom});

        LectureRequirement req1{1, 1, "Section A", 1, 50, 1, "Data Structures", SubjectType::Theory, 2, 1, "Dr. Rao"};
        LectureRequirement req2{2, 2, "Section B", 1, 55, 2, "OOP", SubjectType::Theory, 2, 2, "Ms. Verma"};

        TimetableGenerator generator(validator);
        SchedulingState state;
        GeneratorStats stats = generator.generate({req1, req2}, state);

        assertTest(stats.status == GeneratorStatus::Success, "Small satisfiable problem returns Success");
        assertTest(state.allocations.size() == 4, "Allocated all 4 required instances (2 + 2)");
        assertTest(stats.allocatedInstances == 4, "Stats record 4 allocated instances");

        std::string err;
        auto allInstances = TimetableGenerator::createInstancesFromRequirements({req1, req2});
        bool valid = TimetableGenerator::validateCompleteSchedule(allInstances, state, validator, &err);
        assertTest(valid, "Independent schedule validation passes for small problem");
    }

    // Test 2: Unsatisfiable Problem (Overconstrained slots)
    {
        ConflictValidator validator;
        // Only 1 day with 2 teaching slots and 1 break
        std::vector<TimeSlot> tightSlots = {
            {1, 0, 0, "09:00", "10:00", false},
            {2, 0, 1, "10:00", "11:00", false},
            {3, 0, 2, "11:00", "12:00", true} // break
        };
        validator.setTimeSlots(tightSlots);
        validator.addRoom({1, "CS-101", 60, RoomType::Classroom});

        // 3 lectures for the same section with only 2 non-break slots!
        LectureRequirement req{1, 1, "Section A", 1, 50, 1, "Data Structures", SubjectType::Theory, 3, 1, "Dr. Rao"};

        TimetableGenerator generator(validator);
        SchedulingState state;
        GeneratorStats stats = generator.generate({req}, state);

        assertTest(stats.status == GeneratorStatus::NoSolution, "Overconstrained problem correctly returns NoSolution");
        assertTest(state.allocations.empty(), "State allocations cleanly emptied after NoSolution");
        assertTest(validator.teacherOccupancyCount() == 0, "Validator teacher occupancy clean after NoSolution");
        assertTest(validator.sectionOccupancyCount() == 0, "Validator section occupancy clean after NoSolution");
        assertTest(validator.roomOccupancyCount() == 0, "Validator room occupancy clean after NoSolution");
    }

    // Test 3: Two-Slot Lab Allocation and Verification
    {
        ConflictValidator validator;
        validator.setTimeSlots(createStandardTimeslots());
        validator.addRoom({1, "CS-101", 60, RoomType::Classroom});
        validator.addRoom({2, "Lab-1", 60, RoomType::Lab});

        LectureRequirement labReq{1, 1, "Section A", 1, 50, 6, "DS Lab", SubjectType::Lab, 1, 5, "Mr. Negi"};
        LectureRequirement theoryReq{2, 1, "Section A", 1, 50, 1, "DS Theory", SubjectType::Theory, 1, 1, "Dr. Rao"};

        TimetableGenerator generator(validator);
        SchedulingState state;
        GeneratorStats stats = generator.generate({labReq, theoryReq}, state);

        assertTest(stats.status == GeneratorStatus::Success, "Two-slot lab schedule solved successfully");
        assertTest(state.allocations.size() == 2, "Allocated 1 lab and 1 theory instance");

        auto labAlloc = state.getAllocation(1); // lab is instance 1
        assertTest(labAlloc.has_value(), "Lab instance allocated");
        if (labAlloc) {
            assertTest(labAlloc->durationSlots == 2, "Lab allocation recorded with 2 duration slots");
            assertTest(labAlloc->roomId == 2, "Lab assigned to Lab-1 (RoomType::Lab)");
            assertTest(labAlloc->startSlotIndex != 2 && labAlloc->startSlotIndex != 3,
                       "Lab does not overlap break (starts away from slot 2 and slot 3)");
        }
    }

    // Test 4: Multiple Instances of the Same Requirement
    {
        ConflictValidator validator;
        validator.setTimeSlots(createStandardTimeslots());
        validator.addRoom({1, "CS-101", 60, RoomType::Classroom});

        // 4 sessions of DBMS theory per week
        LectureRequirement req{1, 1, "Section A", 1, 50, 3, "DBMS", SubjectType::Theory, 4, 3, "Mr. Iyer"};

        TimetableGenerator generator(validator);
        SchedulingState state;
        GeneratorStats stats = generator.generate({req}, state);

        assertTest(stats.status == GeneratorStatus::Success, "4 sessions of same requirement scheduled");
        assertTest(state.allocations.size() == 4, "4 distinct allocations created");

        // Verify distinct timeslots
        std::vector<std::pair<int, int>> scheduledSlots;
        for (const auto &[instId, alloc] : state.allocations) {
            scheduledSlots.push_back({alloc.day, alloc.startSlotIndex});
        }
        bool allDistinct = true;
        for (size_t i = 0; i < scheduledSlots.size(); ++i) {
            for (size_t j = i + 1; j < scheduledSlots.size(); ++j) {
                if (scheduledSlots[i] == scheduledSlots[j]) allDistinct = false;
            }
        }
        assertTest(allDistinct, "All 4 sessions scheduled at distinct (day, slot) times");
    }

    // Test 5: Backtracking Scenario (Resource Contention)
    {
        ConflictValidator validator;
        // Exactly 2 non-break slots on day 0
        std::vector<TimeSlot> slotList2 = {
            {1, 0, 0, "09:00", "10:00", false},
            {2, 0, 1, "10:00", "11:00", false}
        };
        validator.setTimeSlots(slotList2);
        // Room 1 (Classroom, 60), Room 2 (Classroom, 70)
        validator.addRoom({1, "CS-Small", 60, RoomType::Classroom});
        validator.addRoom({2, "CS-Large", 70, RoomType::Classroom});

        // Teacher 1 teaches Section A (size 65, can ONLY fit in CS-Large)
        LectureRequirement largeReq{1, 1, "Section A", 1, 65, 1, "Theory A", SubjectType::Theory, 1, 1, "Dr. Rao"};
        // Teacher 2 teaches Section B (size 50, fits in CS-Small or CS-Large)
        LectureRequirement smallReq{2, 2, "Section B", 1, 50, 2, "Theory B", SubjectType::Theory, 1, 2, "Ms. Verma"};

        TimetableGenerator generator(validator);
        SchedulingState state;
        GeneratorStats stats = generator.generate({smallReq, largeReq}, state);

        assertTest(stats.status == GeneratorStatus::Success, "Backtracking resource contention solved");
        // smallReq is instance 1, largeReq is instance 2
        auto largeAlloc = state.getAllocation(2);
        assertTest(largeAlloc.has_value(), "Large section allocation found");
        if (largeAlloc) {
            assertTest(largeAlloc->roomId == 2, "Large section correctly routed to CS-Large (Room 2)");
        }
    }

    // Test 6: Empty Candidate Domain (Immediate fail-first)
    {
        ConflictValidator validator;
        validator.setTimeSlots(createStandardTimeslots());
        // Only Classroom rooms, NO Lab room
        validator.addRoom({1, "CS-101", 60, RoomType::Classroom});

        // Requirement is a Lab, but no Lab room exists
        LectureRequirement labReq{1, 1, "Section A", 1, 50, 6, "DS Lab", SubjectType::Lab, 1, 5, "Mr. Negi"};

        TimetableGenerator generator(validator);
        SchedulingState state;
        GeneratorStats stats = generator.generate({labReq}, state);

        assertTest(stats.status == GeneratorStatus::NoSolution, "Empty candidate domain immediately returns NoSolution");
        assertTest(stats.nodesExplored <= 1, "MRV fail-first stopped without expanding doomed nodes");
    }

    // Test 7: Search Node Limit Reached
    {
        ConflictValidator validator;
        // 2 slots
        std::vector<TimeSlot> slotList2 = {
            {1, 0, 0, "09:00", "10:00", false},
            {2, 0, 1, "10:00", "11:00", false}
        };
        validator.setTimeSlots(slotList2);
        validator.addRoom({1, "CS-101", 60, RoomType::Classroom});

        // 10 instances for 2 slots -> huge conflict tree
        LectureRequirement req{1, 1, "Section A", 1, 50, 1, "DBMS", SubjectType::Theory, 10, 1, "Dr. Rao"};

        GeneratorConfig cfg;
        cfg.maxNodes = 1; // Strict node limit of 1 node ensures early termination

        TimetableGenerator generator(validator, cfg);
        SchedulingState state;
        GeneratorStats stats = generator.generate({req}, state);

        assertTest(stats.status == GeneratorStatus::SearchLimitReached, "Terminates with SearchLimitReached when node limit exceeded");
        assertTest(state.allocations.empty(), "State allocations empty after search limit reached");
        assertTest(validator.teacherOccupancyCount() == 0, "No dangling locks after search limit reached");
    }

    // Test 8: Complete Schedule Validation
    {
        ConflictValidator validator;
        validator.setTimeSlots(createStandardTimeslots());
        validator.addRoom({1, "CS-101", 60, RoomType::Classroom});
        validator.addRoom({2, "CS-102", 60, RoomType::Classroom});
        validator.addRoom({3, "Lab-1", 60, RoomType::Lab});

        // Multi-subject schedule with 2 sections, 1 lab, multiple theory
        std::vector<LectureRequirement> requirements = {
            {1, 1, "Section A", 1, 50, 1, "Data Structures", SubjectType::Theory, 3, 1, "Dr. Rao"},
            {2, 1, "Section A", 1, 50, 2, "OOP", SubjectType::Theory, 2, 2, "Ms. Verma"},
            {3, 1, "Section A", 1, 50, 6, "DS Lab", SubjectType::Lab, 1, 5, "Mr. Negi"},
            {4, 2, "Section B", 1, 55, 1, "Data Structures", SubjectType::Theory, 3, 1, "Dr. Rao"}, // shared teacher
            {5, 2, "Section B", 1, 55, 3, "DBMS", SubjectType::Theory, 2, 3, "Mr. Iyer"}
        };

        TimetableGenerator generator(validator);
        SchedulingState state;
        GeneratorStats stats = generator.generate(requirements, state);

        assertTest(stats.status == GeneratorStatus::Success, "Multi-section mixed theory/lab problem solved");
        assertTest(state.allocations.size() == 11, "All 11 instances successfully scheduled");

        std::string valReport;
        auto allInst = TimetableGenerator::createInstancesFromRequirements(requirements);
        bool allValid = TimetableGenerator::validateCompleteSchedule(allInst, state, validator, &valReport);
        assertTest(allValid, "Full independent pairwise schedule verification passed");
    }

    out << "----------------------------------------\n";
    out << "SUMMARY: " << passedTests << " / " << totalTests << " generator tests PASSED.\n";
    out << "========================================\n";

    if (outputReport) {
        *outputReport = out.str();
    }

    return (passedTests == totalTests);
}

} // namespace catalyst

#endif // TIMETABLE_GENERATOR_TESTS_H
