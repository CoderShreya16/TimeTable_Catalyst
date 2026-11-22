#include "ConflictValidator.h"
#include <sstream>

namespace catalyst {

void ConflictValidator::addTimeSlot(const TimeSlot &slot) {
    m_timeSlots.push_back(slot);
    int key = makeDaySlotKey(slot.day, slot.slotIndex);
    m_slotByDayAndIndex[key] = &m_timeSlots.back();
    m_slotById[slot.id] = &m_timeSlots.back();
}

void ConflictValidator::setTimeSlots(const std::vector<TimeSlot> &slots) {
    m_timeSlots = slots;
    m_slotByDayAndIndex.clear();
    m_slotById.clear();
    for (const auto &slot : m_timeSlots) {
        int key = makeDaySlotKey(slot.day, slot.slotIndex);
        m_slotByDayAndIndex[key] = &slot;
        m_slotById[slot.id] = &slot;
    }
}

const TimeSlot* ConflictValidator::findTimeSlot(int day, int slotIndex) const {
    int key = makeDaySlotKey(day, slotIndex);
    auto it = m_slotByDayAndIndex.find(key);
    if (it != m_slotByDayAndIndex.end()) {
        return it->second;
    }
    return nullptr;
}

const TimeSlot* ConflictValidator::findTimeSlotById(int slotId) const {
    auto it = m_slotById.find(slotId);
    if (it != m_slotById.end()) {
        return it->second;
    }
    return nullptr;
}

const TimeSlot* ConflictValidator::getNextConsecutiveSlot(const TimeSlot &slot) const {
    const TimeSlot *next = findTimeSlot(slot.day, slot.slotIndex + 1);
    if (next && !next->isBreak) {
        return next;
    }
    return nullptr;
}

void ConflictValidator::addRoom(const RoomInfo &room) {
    m_rooms[room.id] = room;
}

void ConflictValidator::setRooms(const std::vector<RoomInfo> &rooms) {
    m_rooms.clear();
    for (const auto &r : rooms) {
        m_rooms[r.id] = r;
    }
}

const RoomInfo* ConflictValidator::findRoom(int roomId) const {
    auto it = m_rooms.find(roomId);
    if (it != m_rooms.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ConflictValidator::isTeacherOccupied(int teacherId, int day, int slotIndex) const {
    return m_teacherOccupancy.find({teacherId, day, slotIndex}) != m_teacherOccupancy.end();
}

bool ConflictValidator::isSectionOccupied(int sectionId, int day, int slotIndex) const {
    return m_sectionOccupancy.find({sectionId, day, slotIndex}) != m_sectionOccupancy.end();
}

bool ConflictValidator::isRoomOccupied(int roomId, int day, int slotIndex) const {
    return m_roomOccupancy.find({roomId, day, slotIndex}) != m_roomOccupancy.end();
}

bool ConflictValidator::isRoomSuitable(const RoomInfo &room, const LectureInstance &instance, std::string *reason) const {
    if (room.capacity < instance.studentCount) {
        if (reason) {
            std::ostringstream ss;
            ss << "Insufficient room capacity: room '" << room.name << "' has capacity "
               << room.capacity << ", but section needs " << instance.studentCount << ".";
            *reason = ss.str();
        }
        return false;
    }

    if (instance.subjectType == SubjectType::Lab && room.type != RoomType::Lab) {
        if (reason) {
            *reason = "Lab session requires a laboratory room, but '" + room.name + "' is a classroom.";
        }
        return false;
    }

    if (instance.subjectType == SubjectType::Theory && room.type != RoomType::Classroom) {
        if (reason) {
            *reason = "Theory lecture requires a standard classroom, but '" + room.name + "' is a laboratory.";
        }
        return false;
    }

    return true;
}

bool ConflictValidator::isValidAllocation(const LectureInstance &instance,
                                         const TimeSlot &startSlot,
                                         const RoomInfo &room,
                                         std::string *reason) const {
    if (instance.instanceId <= 0 || instance.durationSlots <= 0) {
        if (reason) *reason = "Invalid instance parameters (instanceId or durationSlots <= 0).";
        return false;
    }

    if (room.id <= 0) {
        if (reason) *reason = "Invalid room ID.";
        return false;
    }

    if (startSlot.isBreak) {
        if (reason) *reason = "Cannot schedule lecture starting during a break timeslot.";
        return false;
    }

    // Room capacity and type suitability check
    if (!isRoomSuitable(room, instance, reason)) {
        return false;
    }

    // Check all required consecutive slots
    for (int k = 0; k < instance.durationSlots; ++k) {
        int currentSlotIndex = startSlot.slotIndex + k;
        const TimeSlot *currentSlot = findTimeSlot(startSlot.day, currentSlotIndex);

        if (!currentSlot) {
            if (reason) {
                std::ostringstream ss;
                ss << "Required slot index " << currentSlotIndex << " does not exist on day " << startSlot.day << ".";
                *reason = ss.str();
            }
            return false;
        }

        if (currentSlot->isBreak) {
            if (reason) {
                std::ostringstream ss;
                ss << "Required slot index " << currentSlotIndex << " on day " << startSlot.day << " is a break period.";
                *reason = ss.str();
            }
            return false;
        }

        if (isTeacherOccupied(instance.teacherId, startSlot.day, currentSlotIndex)) {
            if (reason) {
                std::ostringstream ss;
                ss << "Teacher conflict: Teacher ID " << instance.teacherId
                   << " is already occupied on day " << startSlot.day << " at slot " << currentSlotIndex << ".";
                *reason = ss.str();
            }
            return false;
        }

        if (isSectionOccupied(instance.sectionId, startSlot.day, currentSlotIndex)) {
            if (reason) {
                std::ostringstream ss;
                ss << "Section conflict: Section ID " << instance.sectionId
                   << " is already occupied on day " << startSlot.day << " at slot " << currentSlotIndex << ".";
                *reason = ss.str();
            }
            return false;
        }

        if (isRoomOccupied(room.id, startSlot.day, currentSlotIndex)) {
            if (reason) {
                std::ostringstream ss;
                ss << "Room conflict: Room '" << room.name << "' (ID " << room.id
                   << ") is already occupied on day " << startSlot.day << " at slot " << currentSlotIndex << ".";
                *reason = ss.str();
            }
            return false;
        }
    }

    return true;
}

bool ConflictValidator::allocate(const LectureInstance &instance,
                                 const TimeSlot &startSlot,
                                 const RoomInfo &room,
                                 Allocation *outAllocation) {
    if (!isValidAllocation(instance, startSlot, room)) {
        return false;
    }

    for (int k = 0; k < instance.durationSlots; ++k) {
        int currentSlotIndex = startSlot.slotIndex + k;
        m_teacherOccupancy.insert({instance.teacherId, startSlot.day, currentSlotIndex});
        m_sectionOccupancy.insert({instance.sectionId, startSlot.day, currentSlotIndex});
        m_roomOccupancy.insert({room.id, startSlot.day, currentSlotIndex});
    }

    if (outAllocation) {
        outAllocation->instanceId = instance.instanceId;
        outAllocation->roomId = room.id;
        outAllocation->startSlotId = startSlot.id;
        outAllocation->day = startSlot.day;
        outAllocation->startSlotIndex = startSlot.slotIndex;
        outAllocation->durationSlots = instance.durationSlots;
    }

    return true;
}

bool ConflictValidator::deallocate(const LectureInstance &instance,
                                   const Allocation &allocation) {
    if (allocation.durationSlots <= 0) {
        return false;
    }

    for (int k = 0; k < allocation.durationSlots; ++k) {
        int currentSlotIndex = allocation.startSlotIndex + k;
        m_teacherOccupancy.erase({instance.teacherId, allocation.day, currentSlotIndex});
        m_sectionOccupancy.erase({instance.sectionId, allocation.day, currentSlotIndex});
        m_roomOccupancy.erase({allocation.roomId, allocation.day, currentSlotIndex});
    }

    return true;
}

void ConflictValidator::clear() {
    m_teacherOccupancy.clear();
    m_sectionOccupancy.clear();
    m_roomOccupancy.clear();
}

} // namespace catalyst

