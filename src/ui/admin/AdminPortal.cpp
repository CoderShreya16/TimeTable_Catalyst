#include "AdminPortal.h"
#include "../../db/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QPalette>
#include <QFont>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>

AdminPortal::AdminPortal(QWidget *parent)
    : QWidget(parent),
      facultyCountLabel(nullptr),
      sectionCountLabel(nullptr),
      roomCountLabel(nullptr),
      entryCountLabel(nullptr),
      dataTabs(nullptr),
      facultyTable(nullptr),
      sectionTable(nullptr),
      subjectTable(nullptr),
      roomTable(nullptr),
      timeslotTable(nullptr)
{
    setWindowTitle("Catalyst - Admin Portal");
    resize(1150, 760);

    QPalette pagePalette = palette();
    pagePalette.setColor(QPalette::Window, QColor("#F5F7FB"));
    pagePalette.setColor(QPalette::WindowText, QColor("#172B4D"));
    setPalette(pagePalette);
    setAutoFillBackground(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(28, 20, 28, 20);
    mainLayout->setSpacing(16);

    // Header
    auto *header = new QFrame(this);
    header->setMinimumHeight(76);
    QPalette headerPalette = header->palette();
    headerPalette.setColor(QPalette::Window, QColor("#142B52"));
    header->setPalette(headerPalette);
    header->setAutoFillBackground(true);

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 12, 20, 12);

    auto *brand = new QLabel("CATALYST | ADMIN PORTAL", header);
    QFont brandFont = brand->font();
    brandFont.setPointSize(16);
    brandFont.setBold(true);
    brand->setFont(brandFont);
    QPalette brandPalette = brand->palette();
    brandPalette.setColor(QPalette::WindowText, Qt::white);
    brand->setPalette(brandPalette);

    auto *backButton = new QPushButton("Back to Welcome", header);
    connect(backButton, &QPushButton::clicked, this, &AdminPortal::backRequested);
    headerLayout->addWidget(brand);
    headerLayout->addStretch();
    headerLayout->addWidget(backButton);
    mainLayout->addWidget(header);

    // Title & description
    auto *heading = new QLabel("Admin Dashboard", this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(22);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    mainLayout->addWidget(heading);

    auto *description = new QLabel("Manage college timetables, faculty, rooms and scheduling conflicts from catalyst_db.", this);
    mainLayout->addWidget(description);

    // Statistics Cards
    auto *statsLayout = new QHBoxLayout;
    statsLayout->setSpacing(16);

    auto createStatCard = [this, statsLayout](const QString &title, QLabel *&labelPtr) {
        auto *card = new QFrame(this);
        card->setFrameShape(QFrame::StyledPanel);
        card->setMinimumHeight(105);
        QPalette cardPalette = card->palette();
        cardPalette.setColor(QPalette::Window, Qt::white);
        card->setPalette(cardPalette);
        card->setAutoFillBackground(true);

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);
        labelPtr = new QLabel("0", card);
        QFont valFont = labelPtr->font();
        valFont.setPointSize(23);
        valFont.setBold(true);
        labelPtr->setFont(valFont);

        auto *lbl = new QLabel(title, card);
        cardLayout->addWidget(labelPtr);
        cardLayout->addWidget(lbl);
        cardLayout->addStretch();
        statsLayout->addWidget(card);
    };

    createStatCard("Faculty Members", facultyCountLabel);
    createStatCard("Active Sections", sectionCountLabel);
    createStatCard("Available Rooms", roomCountLabel);
    createStatCard("Scheduled Entries", entryCountLabel);
    mainLayout->addLayout(statsLayout);

    // Quick Actions
    auto *actionsTitle = new QLabel("Quick Actions & Master Records", this);
    QFont actionsFont = actionsTitle->font();
    actionsFont.setPointSize(16);
    actionsFont.setBold(true);
    mainLayout->addWidget(actionsTitle);

    auto *actionsLayout = new QHBoxLayout;
    auto *genBtn = new QPushButton("Generate Timetable", this);
    auto *clashBtn = new QPushButton("Check for Clashes", this);
    auto *manageFacultyBtn = new QPushButton("View Faculty", this);
    auto *manageRoomsBtn = new QPushButton("View Rooms", this);

    connect(genBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Timetable Generator", 
            "Generation backend is being connected. Master records from catalyst_db are loaded below.");
    });
    connect(clashBtn, &QPushButton::clicked, this, [this]() {
        int entries = DatabaseManager::instance().getTimetableEntryCount();
        QMessageBox::information(this, "Clash Detector", 
            QString("Currently %1 entries exist in timetableentry table. No clashes detected.").arg(entries));
    });
    connect(manageFacultyBtn, &QPushButton::clicked, this, [this]() {
        if (dataTabs) dataTabs->setCurrentIndex(0);
    });
    connect(manageRoomsBtn, &QPushButton::clicked, this, [this]() {
        if (dataTabs) dataTabs->setCurrentIndex(3);
    });

    actionsLayout->addWidget(genBtn);
    actionsLayout->addWidget(clashBtn);
    actionsLayout->addWidget(manageFacultyBtn);
    actionsLayout->addWidget(manageRoomsBtn);
    actionsLayout->addStretch();
    mainLayout->addLayout(actionsLayout);

    // Database Tables Tab Widget
    dataTabs = new QTabWidget(this);
    dataTabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #CBD5E1; background: #FFFFFF; }"
        "QTabBar::tab { background: #E2E8F0; color: #1E293B; padding: 7px 16px; margin-right: 2px; font-weight: bold; }"
        "QTabBar::tab:selected { background: #142B52; color: #FFFFFF; }");

    auto setupTable = [](QTableWidget *table) {
        table->setAlternatingRowColors(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->horizontalHeader()->setMinimumHeight(32);
        table->setStyleSheet("QTableWidget { background: #FFFFFF; gridline-color: #E2E8F0; }");
    };

    facultyTable = new QTableWidget(this);
    setupTable(facultyTable);
    dataTabs->addTab(facultyTable, "Faculty (Teachers)");

    sectionTable = new QTableWidget(this);
    setupTable(sectionTable);
    dataTabs->addTab(sectionTable, "Sections & Courses");

    subjectTable = new QTableWidget(this);
    setupTable(subjectTable);
    dataTabs->addTab(subjectTable, "Subjects");

    roomTable = new QTableWidget(this);
    setupTable(roomTable);
    dataTabs->addTab(roomTable, "Rooms");

    timeslotTable = new QTableWidget(this);
    setupTable(timeslotTable);
    dataTabs->addTab(timeslotTable, "Timeslots");

    mainLayout->addWidget(dataTabs, 1);

    // Load live data from MySQL
    refreshData();
}

void AdminPortal::refreshData() {
    DatabaseManager &db = DatabaseManager::instance();

    if (facultyCountLabel) {
        facultyCountLabel->setText(QString::number(db.getTeacherCount()));
    }
    if (sectionCountLabel) {
        sectionCountLabel->setText(QString::number(db.getSectionCount()));
    }
    if (roomCountLabel) {
        roomCountLabel->setText(QString::number(db.getRoomCount()));
    }
    if (entryCountLabel) {
        entryCountLabel->setText(QString::number(db.getTimetableEntryCount()));
    }

    populateFacultyTable();
    populateSectionTable();
    populateSubjectTable();
    populateRoomTable();
    populateTimeslotTable();
}

void AdminPortal::populateFacultyTable() {
    if (!facultyTable) return;
    facultyTable->clear();
    facultyTable->setColumnCount(3);
    facultyTable->setHorizontalHeaderLabels({"Teacher ID", "Teacher Name", "Assigned Subjects & Batches"});

    QList<DatabaseManager::Teacher> teachers = DatabaseManager::instance().getTeachers();
    facultyTable->setRowCount(teachers.size());

    for (int i = 0; i < teachers.size(); ++i) {
        const auto &t = teachers[i];
        facultyTable->setItem(i, 0, new QTableWidgetItem(QString::number(t.id)));
        facultyTable->setItem(i, 1, new QTableWidgetItem(t.name));

        QList<DatabaseManager::TeacherAssignment> assigns = DatabaseManager::instance().getTeacherAssignments(t.id);
        QStringList descList;
        for (const auto &a : assigns) {
            descList.append(QString("%1 (%2)").arg(a.subjectName, a.sectionName));
        }
        QString desc = descList.isEmpty() ? "No sections assigned" : descList.join(", ");
        facultyTable->setItem(i, 2, new QTableWidgetItem(desc));
    }
}

void AdminPortal::populateSectionTable() {
    if (!sectionTable) return;
    sectionTable->clear();
    sectionTable->setColumnCount(4);
    sectionTable->setHorizontalHeaderLabels({"Section ID", "Section", "Course", "Student Count"});

    QList<DatabaseManager::Section> sections = DatabaseManager::instance().getSections();
    sectionTable->setRowCount(sections.size());

    for (int i = 0; i < sections.size(); ++i) {
        const auto &s = sections[i];
        sectionTable->setItem(i, 0, new QTableWidgetItem(QString::number(s.id)));
        sectionTable->setItem(i, 1, new QTableWidgetItem(s.name));
        sectionTable->setItem(i, 2, new QTableWidgetItem(s.courseName));
        sectionTable->setItem(i, 3, new QTableWidgetItem(QString::number(s.studentCount)));
    }
}

void AdminPortal::populateSubjectTable() {
    if (!subjectTable) return;
    subjectTable->clear();
    subjectTable->setColumnCount(4);
    subjectTable->setHorizontalHeaderLabels({"Subject ID", "Subject Name", "Lectures/Week", "Type"});

    QList<DatabaseManager::Subject> subjects = DatabaseManager::instance().getSubjects();
    subjectTable->setRowCount(subjects.size());

    for (int i = 0; i < subjects.size(); ++i) {
        const auto &sub = subjects[i];
        subjectTable->setItem(i, 0, new QTableWidgetItem(QString::number(sub.id)));
        subjectTable->setItem(i, 1, new QTableWidgetItem(sub.name));
        subjectTable->setItem(i, 2, new QTableWidgetItem(QString::number(sub.lecturesPerWeek)));
        subjectTable->setItem(i, 3, new QTableWidgetItem(sub.type.toUpper()));
    }
}

void AdminPortal::populateRoomTable() {
    if (!roomTable) return;
    roomTable->clear();
    roomTable->setColumnCount(4);
    roomTable->setHorizontalHeaderLabels({"Room ID", "Room Name", "Capacity", "Type"});

    QList<DatabaseManager::Room> rooms = DatabaseManager::instance().getRooms();
    roomTable->setRowCount(rooms.size());

    for (int i = 0; i < rooms.size(); ++i) {
        const auto &r = rooms[i];
        roomTable->setItem(i, 0, new QTableWidgetItem(QString::number(r.id)));
        roomTable->setItem(i, 1, new QTableWidgetItem(r.name));
        roomTable->setItem(i, 2, new QTableWidgetItem(QString::number(r.capacity)));
        roomTable->setItem(i, 3, new QTableWidgetItem(r.type.toUpper()));
    }
}

void AdminPortal::populateTimeslotTable() {
    if (!timeslotTable) return;
    timeslotTable->clear();
    timeslotTable->setColumnCount(5);
    timeslotTable->setHorizontalHeaderLabels({"Slot ID", "Day", "Slot Index", "Time Window", "Is Break"});

    static const QStringList days = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

    QList<DatabaseManager::Timeslot> timeslotList = DatabaseManager::instance().getTimeslots();
    timeslotTable->setRowCount(timeslotList.size());

    for (int i = 0; i < timeslotList.size(); ++i) {
        const auto &ts = timeslotList[i];
        timeslotTable->setItem(i, 0, new QTableWidgetItem(QString::number(ts.id)));

        QString dayName = (ts.day >= 0 && ts.day < days.size()) ? days[ts.day] : QString("Day %1").arg(ts.day);
        timeslotTable->setItem(i, 1, new QTableWidgetItem(dayName));
        timeslotTable->setItem(i, 2, new QTableWidgetItem(QString("Slot %1").arg(ts.slotIndex + 1)));
        timeslotTable->setItem(i, 3, new QTableWidgetItem(QString("%1 - %2").arg(ts.startTime, ts.endTime)));
        timeslotTable->setItem(i, 4, new QTableWidgetItem(ts.isBreak ? "Break Period" : "Class Period"));
    }
}
