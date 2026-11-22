#ifndef CONFLICT_VALIDATOR_TESTS_H
#define CONFLICT_VALIDATOR_TESTS_H

#include "ConflictValidator.h"
#include <iostream>
#include <sstream>
#include <cassert>
#include <string>
#include <vector>

namespace catalyst {

inline bool runConflictValidatorTests(std::string *outputReport = nullptr) {
    std::ostringstream out;
    out << "========================================\n";
    out << "   ConflictValidator Unit & Edge Tests   \n";
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

    // Setup mock timeslots (5 days, 7 slots per day, slot 3 is break)
    ConflictValidator validator;
    std::vector<TimeSlot> timeSlotsList;
    int slotIdCounter = 1;
    for (int day = 0; day < 5; ++day) {
        for (int slotIdx = 0; slotIdx < 7; ++slotIdx) {
            TimeSlot ts;
            ts.id = slotIdCounter++;
            ts.day = day;
            ts.slotIndex = slotIdx;
            ts.isBreak = (slotIdx == 3);
            timeSlotsList.push_back(ts);
        }
    }
    validator.setTimeSlots(timeSlotsList);

    // Setup rooms: Room 1 (Classroom, 60), Room 2 (Classroom, 70), Room 3 (Lab, 70)
    RoomInfo cr60{1, "CS-101", 60, RoomType::Classroom};
    RoomInfo cr70{2, "CS-201", 70, RoomType::Classroom};
    RoomInfo lab70{3, "Lab-1", 70, RoomType::Lab};
    validator.addRoom(cr60);
    validator.addRoom(cr70);
    validator.addRoom(lab70);

    // Test 1: Single-slot Theory allocation and room suitability
    {
        LectureInstance theory1;
        theory1.instanceId = 101;
        theory1.requirementId = 1;
        theory1.sectionId = 1;      // Section A
        theory1.subjectId = 1;      // Data Structures
        theory1.teacherId = 1;      // Dr. Rao
        theory1.subjectType = SubjectType::Theory;
        theory1.studentCount = 58;
        theory1.durationSlots = 1;
        theory1.instanceIndex = 0;

        const TimeSlot *slot0 = validator.findTimeSlot(0, 0);
        assertTest(slot0 != nullptr, "TimeSlot lookup day 0 slot 0");

        std::string reason;
        bool valid = validator.isValidAllocation(theory1, *slot0, cr60, &reason);
        assertTest(valid, "Valid theory allocation in CS-101 (cap 60 >= 58)");

        Allocation alloc;
        bool allocated = validator.allocate(theory1, *slot0, cr60, &alloc);
        assertTest(allocated, "Allocate theory1 succeeded");
        assertTest(alloc.startSlotIndex == 0 && alloc.durationSlots == 1, "Allocation structure verified");

        // Test 1b: Teacher double-booking conflict
        LectureInstance theoryTeacherConflict = theory1;
        theoryTeacherConflict.instanceId = 102;
        theoryTeacherConflict.sectionId = 2; // Different section (B)
        bool clashT = validator.isValidAllocation(theoryTeacherConflict, *slot0, cr70, &reason);
        assertTest(!clashT, "Blocked teacher double-booking on same slot");

        // Test 1c: Section double-booking conflict
        LectureInstance theorySectionConflict = theory1;
        theorySectionConflict.instanceId = 103;
        theorySectionConflict.teacherId = 2; // Different teacher
        bool clashS = validator.isValidAllocation(theorySectionConflict, *slot0, cr70, &reason);
        assertTest(!clashS, "Blocked section double-booking on same slot");

        // Test 1d: Room double-booking conflict
        LectureInstance theoryRoomConflict = theory1;
        theoryRoomConflict.instanceId = 104;
        theoryRoomConflict.teacherId = 2;
        theoryRoomConflict.sectionId = 2;
        bool clashR = validator.isValidAllocation(theoryRoomConflict, *slot0, cr60, &reason);
        assertTest(!clashR, "Blocked room double-booking on same slot");
    }

    // Test 2: Two-slot Lab Allocation, Span Occupancy, and Deallocation
    {
        LectureInstance lab1;
        lab1.instanceId = 201;
        lab1.requirementId = 2;
        lab1.sectionId = 2;      // Section B
        lab1.subjectId = 6;      // DS Lab
        lab1.teacherId = 5;      // Mr. Negi
        lab1.subjectType = SubjectType::Lab;
        lab1.studentCount = 60;
        lab1.durationSlots = 2;
        lab1.instanceIndex = 0;

        const TimeSlot *startSlot = validator.findTimeSlot(1, 0); // Day 1 (Tuesday), slot 0 (09:00 - 11:00)
        assertTest(startSlot != nullptr, "Lab start slot day 1 slot 0 exists");

        Allocation labAlloc;
        bool labAllocOk = validator.allocate(lab1, *startSlot, lab70, &labAlloc);
        assertTest(labAllocOk, "Allocated 2-slot lab across slots 0 and 1");
        assertTest(labAlloc.durationSlots == 2, "Lab allocation duration recorded as 2 slots");

        // Verify occupancy across BOTH slots (slot 0 and slot 1)
        assertTest(validator.isTeacherOccupied(5, 1, 0), "Teacher 5 occupied at Day 1 Slot 0");
        assertTest(validator.isTeacherOccupied(5, 1, 1), "Teacher 5 occupied at Day 1 Slot 1");
        assertTest(validator.isRoomOccupied(3, 1, 0), "Lab Room 3 occupied at Day 1 Slot 0");
        assertTest(validator.isRoomOccupied(3, 1, 1), "Lab Room 3 occupied at Day 1 Slot 1");

        // Attempting to schedule something during slot 1 should conflict with the lab
        LectureInstance conflictSlot1;
        conflictSlot1.instanceId = 202;
        conflictSlot1.sectionId = 2; // Section B
        conflictSlot1.teacherId = 9;
        conflictSlot1.durationSlots = 1;
        conflictSlot1.studentCount = 60;
        conflictSlot1.subjectType = SubjectType::Theory;
        const TimeSlot *slot1 = validator.findTimeSlot(1, 1);
        std::string clashReason;
        bool clashLabSlot1 = validator.isValidAllocation(conflictSlot1, *slot1, cr70, &clashReason);
        assertTest(!clashLabSlot1, "Blocked allocation during second slot of 2-slot lab");

        // Deallocate lab and verify both slots are completely released
        bool deallocOk = validator.deallocate(lab1, labAlloc);
        assertTest(deallocOk, "Deallocated 2-slot lab");
        assertTest(!validator.isTeacherOccupied(5, 1, 0), "Teacher 5 freed at Day 1 Slot 0");
        assertTest(!validator.isTeacherOccupied(5, 1, 1), "Teacher 5 freed at Day 1 Slot 1");
        assertTest(!validator.isRoomOccupied(3, 1, 0), "Lab Room 3 freed at Day 1 Slot 0");
        assertTest(!validator.isRoomOccupied(3, 1, 1), "Lab Room 3 freed at Day 1 Slot 1");
    }

    // Test 3: Break period collision & crossing
    {
        LectureInstance theoryBreak;
        theoryBreak.instanceId = 301;
        theoryBreak.sectionId = 3;
        theoryBreak.teacherId = 7;
        theoryBreak.durationSlots = 1;
        theoryBreak.studentCount = 50;
        theoryBreak.subjectType = SubjectType::Theory;

        const TimeSlot *breakSlot = validator.findTimeSlot(0, 3); // slot 3 is break
        assertTest(breakSlot && breakSlot->isBreak, "Slot 3 correctly marked as isBreak");
        bool breakAlloc = validator.isValidAllocation(theoryBreak, *breakSlot, cr60);
        assertTest(!breakAlloc, "Rejected theory allocation starting during lunch break");

        // 2-slot lab starting at slot 2 (11:00) crossing into slot 3 (break 12:00-13:00)
        LectureInstance labCrossBreak;
        labCrossBreak.instanceId = 302;
        labCrossBreak.sectionId = 3;
        labCrossBreak.teacherId = 7;
        labCrossBreak.durationSlots = 2;
        labCrossBreak.studentCount = 50;
        labCrossBreak.subjectType = SubjectType::Lab;

        const TimeSlot *slotBeforeBreak = validator.findTimeSlot(0, 2);
        std::string breakCrossReason;
        bool crossBreak = validator.isValidAllocation(labCrossBreak, *slotBeforeBreak, lab70, &breakCrossReason);
        assertTest(!crossBreak, "Rejected 2-slot lab crossing into lunch break");
    }

    // Test 4: Day boundary collision
    {
        LectureInstance labAtDayEnd;
        labAtDayEnd.instanceId = 401;
        labAtDayEnd.sectionId = 4;
        labAtDayEnd.teacherId = 8;
        labAtDayEnd.durationSlots = 2;
        labAtDayEnd.studentCount = 45;
        labAtDayEnd.subjectType = SubjectType::Lab;

        const TimeSlot *lastSlot = validator.findTimeSlot(2, 6); // slot 6 is the 7th and last slot
        std::string endDayReason;
        bool endDayAlloc = validator.isValidAllocation(labAtDayEnd, *lastSlot, lab70, &endDayReason);
        assertTest(!endDayAlloc, "Rejected 2-slot lab starting at final slot of day (no slot 7)");
    }

    // Test 5: Room suitability (capacity & type)
    {
        LectureInstance largeSection; // Section D has 64 students
        largeSection.instanceId = 501;
        largeSection.sectionId = 4;
        largeSection.teacherId = 3;
        largeSection.studentCount = 64;
        largeSection.durationSlots = 1;
        largeSection.subjectType = SubjectType::Theory;

        const TimeSlot *slotDay3 = validator.findTimeSlot(3, 1);
        std::string capReason;
        bool smallRoom = validator.isValidAllocation(largeSection, *slotDay3, cr60, &capReason);
        assertTest(!smallRoom, "Rejected section size 64 into room capacity 60");

        bool bigRoom = validator.isValidAllocation(largeSection, *slotDay3, cr70, &capReason);
        assertTest(bigRoom, "Accepted section size 64 into room capacity 70");

        // Lab in classroom rejected
        LectureInstance labInstance;
        labInstance.instanceId = 502;
        labInstance.sectionId = 4;
        labInstance.teacherId = 3;
        labInstance.studentCount = 50;
        labInstance.durationSlots = 1;
        labInstance.subjectType = SubjectType::Lab;

        std::string typeReason;
        bool labInClassroom = validator.isValidAllocation(labInstance, *slotDay3, cr70, &typeReason);
        assertTest(!labInClassroom, "Rejected lab session assigned to classroom");

        // Theory in lab rejected
        bool theoryInLab = validator.isValidAllocation(largeSection, *slotDay3, lab70, &typeReason);
        assertTest(!theoryInLab, "Rejected theory session assigned to lab");
    }

    // Test 6: Independent Schedulability of Multiple Instances
    {
        LectureRequirement req;
        req.requirementId = 10;
        req.sectionId = 1;
        req.subjectId = 1;
        req.teacherId = 1;
        req.subjectType = SubjectType::Theory;
        req.lecturesPerWeek = 4;
        req.studentCount = 58;

        std::vector<LectureInstance> instances;
        for (int i = 0; i < req.lecturesPerWeek; ++i) {
            LectureInstance inst;
            inst.instanceId = 1000 + i;
            inst.requirementId = req.requirementId;
            inst.sectionId = req.sectionId;
            inst.subjectId = req.subjectId;
            inst.teacherId = req.teacherId;
            inst.subjectType = req.subjectType;
            inst.studentCount = req.studentCount;
            inst.durationSlots = req.slotsPerInstance();
            inst.instanceIndex = i;
            instances.push_back(inst);
        }

        assertTest(instances.size() == 4, "Requirement generated 4 independent lecture instances");

        // Schedule instance 0 at Day 2 Slot 0
        const TimeSlot *s0 = validator.findTimeSlot(2, 0);
        Allocation a0;
        bool ok0 = validator.allocate(instances[0], *s0, cr60, &a0);

        // Schedule instance 1 at Day 3 Slot 0
        const TimeSlot *s1 = validator.findTimeSlot(3, 0);
        Allocation a1;
        bool ok1 = validator.allocate(instances[1], *s1, cr60, &a1);

        assertTest(ok0 && ok1, "Multiple instances of same requirement allocated to different days independently");
    }

    out << "----------------------------------------\n";
    out << "SUMMARY: " << passedTests << " / " << totalTests << " tests PASSED.\n";
    out << "========================================\n";

    if (outputReport) {
        *outputReport = out.str();
    }

    return (passedTests == totalTests);
}

} // namespace catalyst

#endif // CONFLICT_VALIDATOR_TESTS_H
