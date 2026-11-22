#ifndef TIMETABLE_GENERATOR_H
#define TIMETABLE_GENERATOR_H

#include "SchedulingModels.h"
#include "ConflictValidator.h"
#include <vector>
#include <unordered_map>
#include <chrono>
#include <string>

namespace catalyst {

enum class GeneratorStatus {
    NotStarted,
    Success,
    NoSolution,
    SearchLimitReached,
    InvalidInput
};

struct GeneratorConfig {
    int maxNodes{100000};             // Maximum search nodes explored
    int timeLimitMs{10000};           // Time limit in ms (0 = unlimited)
    bool enableLcv{true};             // Enable Least Constraining Value ordering
    bool enableForwardChecking{true}; // Enable forward checking pruning
    bool enableOptimization{true};    // Enable quality scoring and workload distribution
    int targetQualityPenalty{0};      // Stop early if penalty score <= this threshold
    int maxSolutionsToExplore{3};     // Maximum feasible solutions to evaluate before returning best
};

struct GeneratorStats {
    GeneratorStatus status{GeneratorStatus::NotStarted};
    int nodesExplored{0};
    int backtracks{0};
    int totalInstances{0};
    int allocatedInstances{0};
    double durationMs{0.0};
    int solutionsFound{0};
    int qualityPenaltyScore{0};       // Total soft penalty score of selected timetable (lower is better)
    std::string message;
};

// Represents a candidate assignment (start timeslot + room) for an instance
struct Candidate {
    const TimeSlot *startSlot{nullptr};
    const RoomInfo *room{nullptr};
    int lcvScore{0}; // Score under Least Constraining Value & quality heuristic (lower = better)
};

class TimetableGenerator {
public:
    explicit TimetableGenerator(ConflictValidator &validator, GeneratorConfig config = GeneratorConfig{});

    // Generate timetable from LectureRequirements
    GeneratorStats generate(const std::vector<LectureRequirement> &requirements,
                            SchedulingState &outState);

    // Generate timetable from pre-created LectureInstances
    GeneratorStats generateFromInstances(const std::vector<LectureInstance> &instances,
                                         SchedulingState &outState);

    // Independent validation of a completed schedule
    static bool validateCompleteSchedule(const std::vector<LectureInstance> &instances,
                                         const SchedulingState &state,
                                         const ConflictValidator &validator,
                                         std::string *errorOut = nullptr);

    // Helper to generate instances from requirements
    static std::vector<LectureInstance> createInstancesFromRequirements(
        const std::vector<LectureRequirement> &requirements);

    const GeneratorStats& getStats() const noexcept { return m_stats; }
    const GeneratorConfig& getConfig() const noexcept { return m_config; }
    void setConfig(const GeneratorConfig &config) noexcept { m_config = config; }

private:
    ConflictValidator &m_validator;
    GeneratorConfig m_config;
    GeneratorStats m_stats;
    std::chrono::steady_clock::time_point m_startTime;

    // Fast candidate generation for an instance under current occupancy
    std::vector<Candidate> getValidCandidates(const LectureInstance &instance) const;

    // Minimum Remaining Values (MRV) selection with Degree Heuristic tie-breaking
    int selectNextVariable(const std::vector<int> &unassignedIds,
                           const std::unordered_map<int, LectureInstance> &instanceMap,
                           std::vector<Candidate> &outCandidates) const;

    // Calculate degree (number of unassigned variables constrained by this variable)
    int calculateDegree(const LectureInstance &var,
                        const std::vector<int> &unassignedIds,
                        const std::unordered_map<int, LectureInstance> &instanceMap) const;

    // Order candidates using Least Constraining Value (LCV)
    void orderCandidatesLcv(const LectureInstance &instance,
                            std::vector<Candidate> &candidates,
                            const std::vector<int> &unassignedIds,
                            const std::unordered_map<int, LectureInstance> &instanceMap) const;

    // Forward checking: check if any remaining unassigned variable has an empty domain
    bool forwardCheck(const std::vector<int> &unassignedIds,
                      const std::unordered_map<int, LectureInstance> &instanceMap) const;

    // Composite candidate scoring: soft constraints + workload distribution + LCV
    int calculateCandidatePenalty(const LectureInstance &instance,
                                  const TimeSlot &slot,
                                  const RoomInfo &room,
                                  const std::vector<int> &unassignedIds,
                                  const std::unordered_map<int, LectureInstance> &instanceMap) const;

    // Full schedule quality evaluation function
    int evaluateSchedulePenalty(const SchedulingState &state,
                                const std::vector<LectureInstance> &instances) const;

    // Incremental tracking for soft constraint evaluation
    void applyTracking(const LectureInstance &inst, const Allocation &alloc);
    void revertTracking(const LectureInstance &inst, const Allocation &alloc);
    void resetTracking();

    // Tracking structures
    std::unordered_map<int, std::unordered_map<int, std::unordered_map<int, int>>> m_sectionDaySubjectCount;
    std::unordered_map<int, std::unordered_map<int, int>> m_sectionDaySlotCount;
    std::unordered_map<int, std::unordered_map<int, int>> m_teacherDaySlotCount;
    std::unordered_map<int, std::unordered_map<int, std::unordered_map<int, std::pair<int, int>>>> m_sectionSlotRoomTeacher;

    // Core recursive backtracking solver
    GeneratorStatus solve(SchedulingState &state,
                          std::vector<int> &unassignedIds,
                          const std::unordered_map<int, LectureInstance> &instanceMap,
                          const std::vector<LectureInstance> &allInstances,
                          SchedulingState &bestState,
                          int &bestPenalty);

    // Timeout check
    bool isTimeLimitExceeded() const;
};

} // namespace catalyst

#endif // TIMETABLE_GENERATOR_H

