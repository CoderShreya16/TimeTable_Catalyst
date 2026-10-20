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
### Setting up the database

1. Install MySQL Server 8.0 and MySQL Workbench.
2. In Workbench, create the database and load the files in order:

```sql
CREATE DATABASE catalyst_db;
USE catalyst_db;
```

3. Run `database/schema.sql`, then `database/seed_data.sql`.
4. Run the check queries at the bottom of `seed_data.sql` and compare with the expected results written there.

Or from a terminal:

```bash
mysql -u root -p -e "CREATE DATABASE catalyst_db;"
mysql -u root -p catalyst_db < database/schema.sql
mysql -u root -p catalyst_db < database/seed_data.sql
```

> `seed_data.sql` deletes existing rows from the data tables (not the tables) before loading, so it is safe to re-run.

---

## Backend

The backend is everything that is not UI. It is kept **independent of Qt widgets**, so it can be built and tested from a console program before any window exists.

### Modules

| Module | Responsibility |
|---|---|
| **Models** | Plain classes for Teacher, Room, Subject, Section and TimetableEntry |
| **Database layer** | Connects to MySQL, loads input data, saves the generated timetable |
| **Conflict checker** | Fast lookup of "is this teacher / room / section free at this day and slot?" |
| **Constraint engine** | `Constraint` base class with `HardConstraint` and `SoftConstraint` subclasses |
| **Generation engine** | Orders the work and runs the backtracking search |
| **Sync engine** | Validates and applies a single change request |

### How a timetable is generated

1. **Build the requirements.** A SQL join over `SectionSubjectTeacher`, `Subject` and `Section` gives one row per (section, subject). Each becomes a requirement such as: *Section A, Data Structures, Dr. Rao, 4 lectures, 1 hour each, 58 students.*
2. **Model it as a constraint satisfaction problem.** Each required lecture is something to place; each (day, slot, room) is a possible position.
3. **Choose the order (MRV).** The class with the **fewest remaining valid options** is placed first, because it is the most likely to get stuck later.
4. **Place with backtracking.** Try a candidate slot, check the constraints, and place it if valid. If a class has no valid slot left, undo the previous placement and try a different one. This continues until every lecture is placed.
5. **Save** the result to `TimetableEntry`.

### The booking registers

The conflict checker keeps three in-memory hash sets (`unordered_set`), keyed like `teacherId_day_slot`, for teachers, rooms and sections. Before placing a class, the code checks all three; if every key is free, it places the class and adds the keys. On backtracking the keys are removed again.

Every class for a given teacher checks the **same** teacher entry, so a teacher who teaches several sections can never be double-booked, with no special-case code. Checks take constant time.

At the start of a run every slot is assumed free. "Free" simply means nothing has claimed that slot yet in this run.

### Constraints

**Hard constraints** (must never be broken):
- A teacher, room or section cannot have two classes at the same time.
- Room capacity must be at least the section's student count.
- Lab subjects need a `lab` room; lecture subjects need a `classroom`.
- A lab occupies **two consecutive slots** on the same day and cannot span the lunch break.
- No class is placed in a break slot.
- Each subject gets exactly its required number of sessions per week.

**Soft constraints** (preferences, used to rank valid options):
- Spread a section's and a teacher's classes evenly across the week.
- Avoid long idle gaps.
- Prefer the smallest room that fits, so large rooms stay free for large sections.

### Handling a change request

A requested move is checked only against the **one affected slot** using the same registers. If it is free, the entry is updated and affected views are notified; if not, it is rejected with a specific reason. The whole timetable is **not** regenerated, which is faster and does not disturb unrelated classes.

### Limitations

Backtracking has exponential worst-case time. At this project's scale (8 sections, about 140 sessions) the most-constrained-first ordering keeps it fast. Very large institutions would need extra techniques such as constraint propagation or local search; this is future scope.

---

## Frontend

The interface is built with **Qt Widgets** in C++. It only calls backend functions and never contains SQL or scheduling logic.

### Windows

| Window | Access | Contents |
|---|---|---|
| **Login dialog** | Everyone | Choose role and identity |
| **Admin window** | Full | Manage data, generate the timetable, request changes, view all sections |
| **Faculty window** | Read-only | The teacher's own timetable, notifications |
| **Student window** | Read-only | The student's own section timetable |

### Main components

- **Timetable grid** (`QTableWidget`): days across, slots down, with lunch shown as a break.
- **Request-change dialog** (`QDialog`): pick a new day, slot and room.
- **Conflict dialog**: shows why a change was rejected.
- **Reminder** (`QTimer`): a popup shortly before a class starts.

Long-running generation is intended to run on a worker thread (`QThread`) so the window stays responsive.

---

## Getting started

### Requirements

- A C++17 compiler (MinGW or MSVC on Windows, GCC/Clang elsewhere)
- CMake/QMake
- Qt (with the **Qt SQL** module and the **`QMYSQL` driver**)
- MySQL Server 8.0 running locally

> The `QMYSQL` driver is not always bundled with Qt installers. Check that it is available early. If `QSqlDatabase::drivers()` does not list `QMYSQL`, the driver has to be built or installed separately.

### Steps

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd catalyst
   ```
2. Set up the database as described in [Setting up the database](#setting-up-the-database).
3. Add your MySQL credentials in a **local config file that is not committed**. Never push passwords to GitHub.
4. Build with CMake or open the project in Qt Creator:
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```
5. Run the executable.

> Fill in exact folder names, Qt version and run instructions once the project layout is final.

---

## Project status

- [x] Problem definition, requirements and architecture
- [x] Database designed and built (10 tables, constraints tested)
- [x] Sample dataset loaded and validated
- [ ] Backend: domain models
- [ ] Backend: database access layer
- [ ] Backend: conflict checker and constraints
- [ ] Backend: generation engine (MRV + backtracking)
- [ ] Backend: saving the generated timetable
- [ ] Backend: change-request handling
- [ ] Frontend: login and role windows
- [ ] Frontend: timetable grid
- [ ] Frontend: change requests, notifications and reminders
- [ ] Integration and testing

> Tick the boxes as work is completed.

### Planned

- A login table for real role-based access
- A change-request history and a sync log to drive the activity feed
- Teacher availability (days or slots a teacher cannot teach)
- Multi-user sync over the network (Qt sockets)

---
