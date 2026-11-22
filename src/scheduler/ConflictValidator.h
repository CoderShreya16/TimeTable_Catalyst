#ifndef CONFLICT_VALIDATOR_H
#define CONFLICT_VALIDATOR_H

#include "SchedulingModels.h"
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstddef>

namespace catalyst {

// Hashable key representing an entity (teacher, section, or room) occupied at a specific day and slot index.
struct OccupancyKey {
    int entityId{0};
    int day{0};
    int slotIndex{0};

    bool operator==(const OccupancyKey &other) const noexcept {
        return entityId == other.entityId && day == other.day && slotIndex == other.slotIndex;
    }
};

struct OccupancyKeyHash {
    std::size_t operator()(const OccupancyKey &k) const noexcept {
        // Standard hash combine
        std::size_t h = std::hash<int>{}(k.entityId);
        h ^= std::hash<int>{}(k.day) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.slotIndex) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

class ConflictValidator {
public:
    ConflictValidator() = default;

    // --- Configuration & Slot / Room Topology ---
    void addTimeSlot(const TimeSlot &slot);
    void setTimeSlots(const std::vector<TimeSlot> &slots);
    const TimeSlot* findTimeSlot(int day, int slotIndex) const;
    const TimeSlot* findTimeSlotById(int slotId) const;
    const TimeSlot* getNextConsecutiveSlot(const TimeSlot &slot) const;

    void addRoom(const RoomInfo &room);
    void setRooms(const std::vector<RoomInfo> &rooms);
    const RoomInfo* findRoom(int roomId) const;

    // --- Occupancy Queries ---
    bool isTeacherOccupied(int teacherId, int day, int slotIndex) const;
    bool isSectionOccupied(int sectionId, int day, int slotIndex) const;
    bool isRoomOccupied(int roomId, int day, int slotIndex) const;

    // --- Room Suitability ---
    // Checks capacity (room.capacity >= instance.studentCount) and room_type matching.
    bool isRoomSuitable(const RoomInfo &room, const LectureInstance &instance, std::string *reason = nullptr) const;

    // --- Allocation Validation ---
    // Validates whether allocating the instance starting at startSlot in room is legal.
    // Handles 1-slot theory and 2-slot labs across consecutive non-break slots.
    bool isValidAllocation(const LectureInstance &instance,
                           const TimeSlot &startSlot,
                           const RoomInfo &room,
                           std::string *reason = nullptr) const;

    // --- Allocation & Backtracking Deallocation ---
    // Commits an allocation into the occupancy tracker. Returns false if invalid.
    bool allocate(const LectureInstance &instance,
                  const TimeSlot &startSlot,
                  const RoomInfo &room,
                  Allocation *outAllocation = nullptr);

    // Reverses an allocation during backtracking search.
    bool deallocate(const LectureInstance &instance,
                    const Allocation &allocation);

    // --- State Management ---
    void clear();
    std::size_t teacherOccupancyCount() const noexcept { return m_teacherOccupancy.size(); }
    std::size_t sectionOccupancyCount() const noexcept { return m_sectionOccupancy.size(); }
    std::size_t roomOccupancyCount() const noexcept { return m_roomOccupancy.size(); }

    const std::vector<TimeSlot>& getAllTimeSlots() const noexcept { return m_timeSlots; }
    const std::unordered_map<int, RoomInfo>& getAllRooms() const noexcept { return m_rooms; }

private:
    // Timeslot storage: lookup by (day, slotIndex) and by slotId
    std::vector<TimeSlot> m_timeSlots;
    // Fast lookup: key is (day << 16) | slotIndex
    std::unordered_map<int, const TimeSlot*> m_slotByDayAndIndex;
    std::unordered_map<int, const TimeSlot*> m_slotById;

    // Room storage: lookup by roomId
    std::unordered_map<int, RoomInfo> m_rooms;

    // Occupancy sets
    std::unordered_set<OccupancyKey, OccupancyKeyHash> m_teacherOccupancy;
    std::unordered_set<OccupancyKey, OccupancyKeyHash> m_sectionOccupancy;
    std::unordered_set<OccupancyKey, OccupancyKeyHash> m_roomOccupancy;

    static int makeDaySlotKey(int day, int slotIndex) noexcept {
        return (day << 16) | (slotIndex & 0xFFFF);
    }
};

} // namespace catalyst

#endif // CONFLICT_VALIDATOR_H

