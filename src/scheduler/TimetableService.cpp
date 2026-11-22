#include "TimetableService.h"
#include <sstream>

namespace catalyst {

TimetableService::TimetableService(DatabaseManager &db)
    : m_db(db) {}

std::vector<TimeSlot> TimetableService::loadTimeSlots(DatabaseManager &db) {
    std::vector<TimeSlot> result;
    QList<DatabaseManager::Timeslot> dbSlots = db.getTimeslots();

    result.reserve(dbSlots.size());
    for (const auto &s : dbSlots) {
        TimeSlot ts;
        ts.id = s.id;
        ts.day = s.day;
        ts.slotIndex = s.slotIndex;
        ts.startTime = s.startTime.toStdString();
        ts.endTime = s.endTime.toStdString();
        ts.isBreak = s.isBreak;
        result.push_back(ts);
    }

    return result;
}

std::vector<RoomInfo> TimetableService::loadRooms(DatabaseManager &db) {
    std::vector<RoomInfo> result;
    QList<DatabaseManager::Room> dbRooms = db.getRooms();

    result.reserve(dbRooms.size());
    for (const auto &r : dbRooms) {
        RoomInfo room;
        room.id = r.id;
        room.name = r.name.toStdString();
        room.capacity = r.capacity;
        room.type = (r.type.compare("lab", Qt::CaseInsensitive) == 0)
                        ? RoomType::Lab
                        : RoomType::Classroom;
        result.push_back(room);
    }

    return result;
}

std::vector<LectureRequirement> TimetableService::loadRequirements(DatabaseManager &db) {
    std::vector<LectureRequirement> result;
    QList<DatabaseManager::TeachingRequirementRecord> dbReqs = db.getAllTeachingRequirements();

    result.reserve(dbReqs.size());
    int reqIdCounter = 1;
    for (const auto &r : dbReqs) {
        LectureRequirement req;
        req.requirementId = reqIdCounter++;
        req.sectionId = r.sectionId;
        req.sectionName = r.sectionName.toStdString();
        req.courseId = r.courseId;
        req.studentCount = r.studentCount;
        req.subjectId = r.subjectId;
        req.subjectName = r.subjectName.toStdString();
        req.subjectType = (r.subjectType.compare("lab", Qt::CaseInsensitive) == 0)
                              ? SubjectType::Lab
                              : SubjectType::Theory;
        req.lecturesPerWeek = r.lecturesPerWeek;
        req.teacherId = r.teacherId;
        req.teacherName = r.teacherName.toStdString();
        result.push_back(req);
    }

    return result;
}

QList<DatabaseManager::ScheduledDbEntry>
TimetableService::convertAllocationsToDbEntries(const SchedulingState &state,
                                              const std::vector<LectureInstance> &instances) {
    QList<DatabaseManager::ScheduledDbEntry> entries;

    std::unordered_map<int, LectureInstance> instMap;
    for (const auto &inst : instances) {
        instMap[inst.instanceId] = inst;
    }

    for (const auto &[instId, alloc] : state.allocations) {
        auto it = instMap.find(instId);
        if (it == instMap.end()) continue;
        const auto &inst = it->second;

        // Theory (1 slot) -> 1 record.
        // Lab (2 consecutive slots) -> 2 records on the same day.
        for (int k = 0; k < alloc.durationSlots; ++k) {
            DatabaseManager::ScheduledDbEntry entry;
            entry.sectionId = inst.sectionId;
            entry.subjectId = inst.subjectId;
            entry.teacherId = inst.teacherId;
            entry.roomId = alloc.roomId;
            entry.day = alloc.day;
            entry.slotIndex = alloc.startSlotIndex + k;
            entries.append(entry);
        }
    }

    return entries;
}

TimetableServiceResult TimetableService::generateTimetableOnly(const GeneratorConfig &config) {
    TimetableServiceResult res;

    std::vector<TimeSlot> timeSlots = loadTimeSlots(m_db);
    if (timeSlots.empty()) {
        res.success = false;
        res.message = "Failed to load timeslots from database.";
        return res;
    }

    std::vector<RoomInfo> rooms = loadRooms(m_db);
    if (rooms.empty()) {
        res.success = false;
        res.message = "Failed to load rooms from database.";
        return res;
    }

    std::vector<LectureRequirement> requirements = loadRequirements(m_db);
    if (requirements.empty()) {
        res.success = false;
        res.message = "Failed to load teaching requirements from database.";
        return res;
    }

    res.instances = TimetableGenerator::createInstancesFromRequirements(requirements);

    ConflictValidator validator;
    validator.setTimeSlots(timeSlots);
    validator.setRooms(rooms);

    TimetableGenerator generator(validator, config);
    res.generatorStats = generator.generateFromInstances(res.instances, res.state);

    if (res.generatorStats.status != GeneratorStatus::Success) {
        res.success = false;
        res.message = "Timetable generation failed: " + res.generatorStats.message;
        return res;
    }

    std::string valError;
    bool valid = TimetableGenerator::validateCompleteSchedule(res.instances, res.state, validator, &valError);
    if (!valid) {
        res.success = false;
        res.message = "Independent timetable verification failed: " + valError;
        res.state.allocations.clear();
        return res;
    }

    res.success = true;
    res.message = "Conflict-free timetable generated and verified successfully.";
    return res;
}

TimetableServiceResult TimetableService::generateAndSaveTimetable(bool replaceExisting,
                                                                 const GeneratorConfig &config) {
    // 1. Generate and independently validate in memory first
    TimetableServiceResult result = generateTimetableOnly(config);
    if (!result.success) {
        return result; // Never modify the database if generation was unsuccessful
    }

    // 2. Convert allocations to database records (expanding 2-slot labs)
    QList<DatabaseManager::ScheduledDbEntry> dbEntries =
        convertAllocationsToDbEntries(result.state, result.instances);

    // 3. Atomically persist to database
    QString dbError;
    bool saved = m_db.saveTimetableEntries(dbEntries, replaceExisting, &dbError);
    if (!saved) {
        result.success = false;
        result.message = "Generation succeeded, but database persistence failed: " + dbError.toStdString();
        return result;
    }

    result.dbEntriesSaved = static_cast<int>(dbEntries.size());
    result.message = "Successfully generated and committed " +
                     std::to_string(dbEntries.size()) +
                     " timetable entries to catalyst_db.";
    return result;
}

} // namespace catalyst
