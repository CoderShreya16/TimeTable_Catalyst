#ifndef TIMETABLE_SERVICE_H
#define TIMETABLE_SERVICE_H

#include "../db/DatabaseManager.h"
#include "SchedulingModels.h"
#include "ConflictValidator.h"
#include "TimetableGenerator.h"

#include <vector>
#include <string>

namespace catalyst {

struct TimetableServiceResult {
    bool success{false};
    std::string message;
    GeneratorStats generatorStats;
    SchedulingState state;
    std::vector<LectureInstance> instances;
    int dbEntriesSaved{0};
};

class TimetableService {
public:
    explicit TimetableService(DatabaseManager &db = DatabaseManager::instance());

    // End-to-end generation and persistence pipeline:
    // Fetches live DB data, builds models, generates conflict-free schedule,
    // independently validates, and commits to timetableentry using an atomic transaction.
    TimetableServiceResult generateAndSaveTimetable(bool replaceExisting = false,
                                                   const GeneratorConfig &config = GeneratorConfig{});

    // In-memory generation only (does not write to database)
    TimetableServiceResult generateTimetableOnly(const GeneratorConfig &config = GeneratorConfig{});

    // Data-loading & model-mapping helpers
    static std::vector<TimeSlot> loadTimeSlots(DatabaseManager &db);
    static std::vector<RoomInfo> loadRooms(DatabaseManager &db);
    static std::vector<LectureRequirement> loadRequirements(DatabaseManager &db);

    // Converts in-memory allocations into DatabaseManager::ScheduledDbEntry records.
    // Expands 2-hour labs into 2 consecutive timetableentry records on the same day.
    static QList<DatabaseManager::ScheduledDbEntry> convertAllocationsToDbEntries(
        const SchedulingState &state,
        const std::vector<LectureInstance> &instances);

private:
    DatabaseManager &m_db;
};

} // namespace catalyst

#endif // TIMETABLE_SERVICE_H

