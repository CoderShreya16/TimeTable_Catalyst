# Catalyst — Intelligent Timetable Generation and Synchronization Platform

A desktop application that **automatically generates clash-free college timetables** and keeps them **in sync** when a change is requested, so a modification never forces a full manual recheck.

Built in **C++ (Qt)** with a **MySQL** database as a Project-Based Learning (PBL) project at the Department of Computer Science & Engineering, Graphic Era (Deemed to be University), Dehradun. Team ID: **T084**.

---

## Table of contents

1. [The problem](#the-problem)
2. [What Catalyst does](#what-catalyst-does)
3. [How it works](#how-it-works)
4. [Tech stack](#tech-stack)
5. [Repository structure](#repository-structure)
6. [Database](#database)
7. [Backend](#backend)
8. [Frontend](#frontend)
9. [Getting started](#getting-started)
10. [Project status](#project-status)
11. [Team](#team)

---

## The problem

Colleges usually build timetables by hand. This does not scale as sections, subjects and teachers grow:

- Every change forces someone to recheck the whole timetable manually.
- Clashes slip through (a teacher or room booked twice at the same time, a class with two lectures at once).
- Faculty are often not told when their schedule changes.

Our own timetable was changed once and several classes clashed. Catalyst is built to prevent that.

## What Catalyst does

- **Generates** a timetable for all sections together, with no teacher, room or section double-booked.
- **Respects real constraints**: room capacity, lab vs classroom rooms, 2-hour labs, lunch break, fixed lectures per week.
- **Validates changes instantly**: a requested change is checked against current bookings and either applied or rejected with a reason.
- **Keeps views in sync**: affected faculty and student views update after a change.
- **Role-based access**: Admin, Faculty and Student each see what is relevant to them.

## How it works

```
Input data in MySQL
(teachers, rooms, subjects, courses, sections, assignments)
            |
            v
Backend loads it and builds a list of "class requirements"
(e.g. Section A needs Data Structures, 4 times a week, taught by Dr. Rao)
            |
            v
Generation engine places every required lecture into a free slot
(backtracking search, most-constrained class first)
            |
            v
Finished timetable is saved to MySQL (TimetableEntry)
            |
            v
Qt interface displays it per role; later change requests are
validated against the same booking data and applied or rejected
```

---

## Tech stack

| Layer | Technology |
|---|---|
| Language | C++ (OOP, STL), C++17 |
| GUI | Qt (Qt Widgets) |
| Database | MySQL 8.0 |
| DB access from C++ | Qt SQL module (`QSqlDatabase`, `QSqlQuery`, `QMYSQL` driver) |
| Build system | CMake |QMake
| Version control | Git / GitHub |

The whole project is written in C++. There is no web frontend or Node.js backend.

---

## Repository structure

> Update this tree to match the actual folders in the repo.

```
catalyst/
├── README.md
├── database/
│   ├── schema.sql          # creates all tables (and the 35 TimeSlot rows)
│   ├── seed_data.sql       # sample input data (12 teachers, 8 sections, ...)
│   └── er_diagram.png      # ER diagram of the database
├── backend/
│   ├── models/             # Teacher, Room, Subject, Section, ...
│   ├── data/               # database access layer (MySQL via Qt SQL)
│   ├── engine/             # conflict checker, constraints, generator, sync
│   └── main.cpp            # console test harness (no UI needed)
├── frontend/               # Qt Widgets windows and dialogs
└── docs/
```

---

## Database

The database is **designed and built by the team**. It stores the *input* the algorithm needs and the *output* it produces. The timetable itself is **not** hand-entered: it is produced by the backend and written into `TimetableEntry`.

### Tables

| Table | Purpose |
|---|---|
| `Teacher` | Teacher names |
| `Room` | Room name, capacity, type (`classroom` or `lab`) |
| `Subject` | Subject name, lectures per week, type (`lecture` or `lab`) |
| `Course` | Courses, e.g. B.Tech CSE |
| `Section` | Section name, student count, and the course it belongs to |
| `TeacherSubject` | Which teachers **can** teach which subjects (many-to-many) |
| `CourseSubject` | Which subjects belong to which course (many-to-many) |
| `SectionSubjectTeacher` | Which teacher **will** teach which subject to which section |
| `TimeSlot` | The weekly grid: Monday to Friday, 7 slots a day, slot 3 (12:00 to 13:00) is the lunch break |
| `TimetableEntry` | **Output.** One row per occupied slot, filled by the backend |

### Relationships

- One **Course** has many **Sections** (`Section.course_id`).
- **Teacher ↔ Subject** and **Course ↔ Subject** are many-to-many, so each has its own link table.
- `SectionSubjectTeacher` turns "who *can* teach" into "who *does* teach" for each section. Its foreign key on `(teacher_id, subject_id)` only allows a teacher who is listed in `TeacherSubject` for that subject.
- `TimetableEntry` references `SectionSubjectTeacher`, `Room` and `TimeSlot`.

The ER diagram is in `database/er_diagram.png`.

### Design decisions worth knowing

- **Duration is not stored.** A `lecture` subject takes 1 slot and a `lab` subject takes 2. The code derives this from `subject_type`.
- **`TimetableEntry` has one row per occupied slot**, so a 2-hour lab becomes two rows. This lets three `UNIQUE` rules cover every double-booking:
  - `UNIQUE (teacher_id, day, slot_index)`
  - `UNIQUE (room_id, day, slot_index)`
  - `UNIQUE (section_id, day, slot_index)`
- **The database is a safety net, not the whole defence.** Rules MySQL cannot express stay in the C++ code: skipping the lunch slot, room capacity and type matching, keeping a lab's two slots consecutive, and even distribution.
- **Data is validated at the database level** with `NOT NULL`, `CHECK`, `ENUM`, `UNIQUE` and foreign keys, so bad rows are rejected.

### Sample dataset

`seed_data.sql` loads 12 teachers, 12 rooms (8 classrooms, 4 labs), 7 subjects, 2 courses, 8 sections (A to F, ML1, ML2) and 54 teacher assignments. It was sized so a valid timetable exists: every section needs 18 to 20 of the 30 weekly slots and no teacher exceeds 15 hours.


- A change-request history and a sync log to drive the activity feed
- Teacher availability (days or slots a teacher cannot teach)
- Multi-user sync over the network (Qt sockets)

---
