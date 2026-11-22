#ifndef TIMETABLE_SERVICE_TESTS_H
#define TIMETABLE_SERVICE_TESTS_H

#include "TimetableService.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace catalyst {

inline bool runTimetableServiceTests(std::string *outputReport = nullptr) {
    std::ostringstream out;
    out << "========================================\n";
    out << "   TimetableService DB Integration Tests\n";
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

    DatabaseManager &db = DatabaseManager::instance();
    if (!db.isConnected()) {
        db.connect();
    }

    if (!db.isConnected()) {
        out << "[SKIP] Database is not connected on localhost:3306. Cannot run live DB integration tests.\n";
        if (outputReport) *outputReport = out.str();
        return false;
    }

    // Backup existing timetableentry records to ensure zero data destruction
    QList<DatabaseManager::TimetableEntry> backupEntries = db.getAllTimetableEntries();
    out << "[INFO] Initial timetableentry count: " << backupEntries.size() << "\n";

    // Test 1: Loading Real Database Data
    {
        std::vector<TimeSlot> timeSlots = TimetableService::loadTimeSlots(db);
        assertTest(!timeSlots.empty() && timeSlots.size() == 35, "Loaded 35 timeslots from catalyst_db");

        std::vector<RoomInfo> rooms = TimetableService::loadRooms(db);
        assertTest(!rooms.empty() && rooms.size() == 12, "Loaded 12 rooms from catalyst_db (8 classrooms, 4 labs)");

        std::vector<LectureRequirement> reqs = TimetableService::loadRequirements(db);
        assertTest(!reqs.empty() && reqs.size() == 54, "Loaded 54 teaching requirements across all sections");
    }

    // Test 2: Constructing Lecture Instances
    {
        std::vector<LectureRequirement> reqs = TimetableService::loadRequirements(db);
        auto instances = TimetableGenerator::createInstancesFromRequirements(reqs);

        int expectedLectureCount = 0;
        for (const auto &r : reqs) {
            expectedLectureCount += r.lecturesPerWeek;
        }

        assertTest(instances.size() == static_cast<size_t>(expectedLectureCount),
                   "Expanded 54 requirements into correct total lecture instances (" +
                   std::to_string(instances.size()) + ")");

        bool durationsCorrect = true;
        for (const auto &inst : instances) {
            if (inst.subjectType == SubjectType::Lab && inst.durationSlots != 2) durationsCorrect = false;
            if (inst.subjectType == SubjectType::Theory && inst.durationSlots != 1) durationsCorrect = false;
        }
        assertTest(durationsCorrect, "All lecture instances have correct durations (Theory=1, Lab=2)");
    }

    // Test 3: Two-Record Lab Persistence Mapping
    {
        LectureInstance theoryInst;
        theoryInst.instanceId = 1;
        theoryInst.sectionId = 1;
        theoryInst.subjectId = 1;
        theoryInst.teacherId = 1;
        theoryInst.durationSlots = 1;

        LectureInstance labInst;
        labInst.instanceId = 2;
        labInst.sectionId = 1;
        labInst.subjectId = 6;
        labInst.teacherId = 11;
        labInst.durationSlots = 2;

        SchedulingState state;
        Allocation theoryAlloc;
        theoryAlloc.instanceId = 1;
        theoryAlloc.roomId = 1;
        theoryAlloc.day = 0;
        theoryAlloc.startSlotIndex = 0;
        theoryAlloc.durationSlots = 1;
        state.allocations[1] = theoryAlloc;

        Allocation labAlloc;
        labAlloc.instanceId = 2;
        labAlloc.roomId = 9;
        labAlloc.day = 1;
        labAlloc.startSlotIndex = 4;
        labAlloc.durationSlots = 2;
        state.allocations[2] = labAlloc;

        auto dbRecords = TimetableService::convertAllocationsToDbEntries(state, {theoryInst, labInst});
        assertTest(dbRecords.size() == 3, "Allocations mapped to 3 database records (1 theory + 2 for lab)");

        // Verify the 2 lab records have identical day/room/section/teacher and consecutive slots
        int labRecordsFound = 0;
        int slotA = -1;
        int slotB = -1;
        for (const auto &rec : dbRecords) {
            if (rec.subjectId == 6) {
                labRecordsFound++;
                if (slotA == -1) slotA = rec.slotIndex;
                else slotB = rec.slotIndex;
                assertTest(rec.day == 1 && rec.roomId == 9 && rec.teacherId == 11, "Lab record preserves day, room, teacher");
            }
        }
        assertTest(labRecordsFound == 2, "Lab produced exactly 2 database records");
        assertTest(slotB == slotA + 1, "Lab database records have consecutive slot indices (" +
                   std::to_string(slotA) + " and " + std::to_string(slotB) + ")");
    }

    // Test 4: Preventing Accidental Overwrite
    {
        // Table has entries from seed data. Calling with replaceExisting = false should fail safely.
        QList<DatabaseManager::ScheduledDbEntry> dummyEntries;
        DatabaseManager::ScheduledDbEntry d;
        d.sectionId = 1; d.subjectId = 1; d.teacherId = 1; d.roomId = 1; d.day = 0; d.slotIndex = 0;
        dummyEntries.append(d);

        QString err;
        bool overwriteBlocked = !db.saveTimetableEntries(dummyEntries, false, &err);
        assertTest(overwriteBlocked, "Prevented accidental timetable overwrite when replaceExisting=false");
        assertTest(!err.isEmpty(), "Error message accurately explains existing timetable was found");
    }

    // Test 5: Transaction Rollback on Failure
    {
        int countBefore = db.getTimetableEntryCount();

        // Attempt insertion of an invalid entry (non-existent room_id = 999999 violates FK room_id -> room)
        QList<DatabaseManager::ScheduledDbEntry> invalidList;
        DatabaseManager::ScheduledDbEntry inv;
        inv.sectionId = 1; inv.subjectId = 1; inv.teacherId = 1; inv.roomId = 999999; inv.day = 0; inv.slotIndex = 0;
        invalidList.append(inv);

        QString err;
        bool failedAsExpected = !db.saveTimetableEntries(invalidList, true, &err);
        assertTest(failedAsExpected, "Transaction rejected invalid foreign key record");

        int countAfter = db.getTimetableEntryCount();
        assertTest(countAfter == countBefore, "Database transaction rolled back cleanly; row count unchanged (" +
                   std::to_string(countAfter) + ")");
    }

    // Test 6: Safe End-to-End Persistence, Retrieval & Non-Destructive Data Restoration
    {
        TimetableService service(db);
        GeneratorConfig cfg;
        cfg.maxNodes = 200000;
        cfg.timeLimitMs = 15000;

        // Perform full end-to-end generation and safe transactional persistence
        TimetableServiceResult result = service.generateAndSaveTimetable(true /* replaceExisting */, cfg);
        assertTest(result.success, "TimetableService generated and committed conflict-free timetable to catalyst_db");
        assertTest(result.dbEntriesSaved > 0, "Persisted " + std::to_string(result.dbEntriesSaved) + " rows in timetableentry");

        // Retrieve saved entries and verify database integrity
        QList<DatabaseManager::TimetableEntry> savedEntries = db.getAllTimetableEntries();
        assertTest(savedEntries.size() == result.dbEntriesSaved,
                   "Retrieved exact count of saved timetable entries from catalyst_db (" +
                   std::to_string(savedEntries.size()) + ")");

        // RESTORATION: Restore original seed data to preserve project database state
        QList<DatabaseManager::ScheduledDbEntry> restoreList;
        for (const auto &b : backupEntries) {
            DatabaseManager::ScheduledDbEntry e;
            e.sectionId = b.sectionId;
            e.subjectId = b.subjectId;
            e.teacherId = b.teacherId;
            e.roomId = b.roomId;
            e.day = b.day;
            e.slotIndex = b.slotIndex;
            restoreList.append(e);
        }

        QString restoreErr;
        bool restored = db.saveTimetableEntries(restoreList, true, &restoreErr);
        assertTest(restored, "Original seed database records restored successfully");

        int finalCount = db.getTimetableEntryCount();
        assertTest(finalCount == backupEntries.size(), "Project database state completely restored to " +
                   std::to_string(finalCount) + " original rows");
    }

    out << "----------------------------------------\n";
    out << "SUMMARY: " << passedTests << " / " << totalTests << " service tests PASSED.\n";
    out << "========================================\n";

    if (outputReport) {
        *outputReport = out.str();
    }

    return (passedTests == totalTests);
}

} // namespace catalyst

#endif // TIMETABLE_SERVICE_TESTS_H
