#ifndef SCHEDULING_MODELS_H
#define SCHEDULING_MODELS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <algorithm>

namespace catalyst {

// Corresponds to MySQL subject.subject_type enum('lecture', 'lab')
enum class SubjectType {
    Theory,
    Lab
};

// Corresponds to MySQL room.room_type enum('classroom', 'lab')
enum class RoomType {
    Classroom,
    Lab
};

// Represents a defined timeslot in the college schedule.
// Does NOT assume database slot_ids are consecutive. Uses (day, slotIndex).
struct TimeSlot {
    int id{0};               // Database slot_id
    int day{0};              // 0 = Monday ... 4 = Friday
    int slotIndex{0};        // 0-based index within the day (0 to 6)
    std::string startTime;   // e.g. "09:00"
    std::string endTime;     // e.g. "10:00"
    bool isBreak{false};     // true if slot is break (e.g. slot 3: 12:00 - 13:00)

    bool operator==(const TimeSlot &other) const noexcept {
        return id == other.id &&
               day == other.day &&
               slotIndex == other.slotIndex &&
               isBreak == other.isBreak;
    }
};

// Represents a subject teaching requirement assigned to a section.
// Derived from sectionsubjectteacher + subject + section.
struct LectureRequirement {
    int requirementId{0};       // Unique identifier
    int sectionId{0};           // Target section ID
    std::string sectionName;
    int courseId{0};
    int studentCount{0};        // Enrolled student count in section
    int subjectId{0};           // Target subject ID
    std::string subjectName;
    SubjectType subjectType{SubjectType::Theory};
    int lecturesPerWeek{0};     // Total sessions required per week
    int teacherId{0};           // Assigned faculty ID
    std::string teacherName;

    // Returns required timeslot duration (1 for Theory, 2 for Lab)
    int slotsPerInstance() const noexcept {
        return (subjectType == SubjectType::Lab) ? 2 : 1;
    }
};

// Represents an individual schedulable session of a LectureRequirement.
// Each instance is scheduled independently.
struct LectureInstance {
    int instanceId{0};          // Globally unique instance ID
    int requirementId{0};       // Refers to parent LectureRequirement
    int sectionId{0};
    int subjectId{0};
    int teacherId{0};
    SubjectType subjectType{SubjectType::Theory};
    int studentCount{0};        // Required student capacity
    int durationSlots{1};       // 1 for Theory, 2 for Lab
    int instanceIndex{0};       // 0-based session index (0 ... lecturesPerWeek-1)
    std::string subjectName;
    std::string sectionName;
    std::string teacherName;

    bool operator==(const LectureInstance &other) const noexcept {
        return instanceId == other.instanceId;
    }
};

// Represents a concrete assignment of a LectureInstance to a room and starting timeslot.
struct Allocation {
    int instanceId{0};
    int roomId{0};
    int startSlotId{0};         // Timeslot ID of starting slot
    int day{0};
    int startSlotIndex{0};
    int durationSlots{1};       // 1 for Theory, 2 for Lab

    bool operator==(const Allocation &other) const noexcept {
        return instanceId == other.instanceId &&
               roomId == other.roomId &&
               startSlotId == other.startSlotId &&
               day == other.day &&
               startSlotIndex == other.startSlotIndex &&
               durationSlots == other.durationSlots;
    }
};

// Snapshot of the current scheduling state during search/backtracking.
struct SchedulingState {
    // Maps instanceId -> Allocation
    std::unordered_map<int, Allocation> allocations;
    std::vector<int> unassignedInstanceIds;

    bool isComplete() const noexcept {
        return unassignedInstanceIds.empty() && !allocations.empty();
    }

    bool isAllocated(int instanceId) const noexcept {
        return allocations.find(instanceId) != allocations.end();
    }

    std::optional<Allocation> getAllocation(int instanceId) const {
        auto it = allocations.find(instanceId);
        if (it != allocations.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// Room specification used for conflict checking and suitability.
struct RoomInfo {
    int id{0};
    std::string name;
    int capacity{0};
    RoomType type{RoomType::Classroom};

    bool operator==(const RoomInfo &other) const noexcept {
        return id == other.id;
    }
};

} // namespace catalyst

#endif // SCHEDULING_MODELS_H

