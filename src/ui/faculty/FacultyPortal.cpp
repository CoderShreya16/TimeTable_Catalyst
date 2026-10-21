#include "FacultyPortal.h"
#include "../../db/DatabaseManager.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QComboBox>
#include <QDate>

FacultyPortal::FacultyPortal(QWidget *parent)
    : QWidget(parent),
      facultySelector(nullptr),
      daySelector(nullptr),
      classesTodayLabel(nullptr),
      nextClassLabel(nullptr),
      scheduleStatusLabel(nullptr),
      timetableWidget(nullptr),
      announcementText(nullptr)
{
    setWindowTitle("Catalyst - Faculty Portal");
    resize(1150, 760);

    // Main window colors
    QPalette pagePalette = palette();
    pagePalette.setColor(QPalette::Window, QColor("#F4F6FA"));
    pagePalette.setColor(QPalette::WindowText, QColor("#1E293B"));
    setPalette(pagePalette);
    setAutoFillBackground(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(16);

    // Header
    auto *header = new QFrame(this);
    header->setMinimumHeight(76);

    QPalette headerPalette = header->palette();
    headerPalette.setColor(QPalette::Window, QColor("#142B52"));
    header->setPalette(headerPalette);
    header->setAutoFillBackground(true);

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(22, 12, 22, 12);

    auto *brand = new QLabel("CATALYST | FACULTY PORTAL", header);
    QFont brandFont = brand->font();
    brandFont.setPointSize(16);
    brandFont.setBold(true);
    brand->setFont(brandFont);

    QPalette brandPalette = brand->palette();
    brandPalette.setColor(QPalette::WindowText, Qt::white);
    brand->setPalette(brandPalette);

    auto *facultySelectLabel = new QLabel("Select Faculty:", header);
    QPalette selLblPalette = facultySelectLabel->palette();
    selLblPalette.setColor(QPalette::WindowText, Qt::white);
    facultySelectLabel->setPalette(selLblPalette);

    facultySelector = new QComboBox(header);
    facultySelector->setMinimumWidth(180);
    facultySelector->setMinimumHeight(34);
    facultySelector->setStyleSheet(
        "QComboBox { background: #FFFFFF; color: #1E293B; border-radius: 4px; padding: 4px 8px; font-weight: bold; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #1E293B; selection-background-color: #2563EB; selection-color: #FFFFFF; }");

    connect(facultySelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FacultyPortal::onFacultyChanged);

    auto *backButton = new QPushButton("Back to Login", header);
    connect(backButton, &QPushButton::clicked, this, &FacultyPortal::backRequested);

    headerLayout->addWidget(brand);
    headerLayout->addStretch();
    headerLayout->addWidget(facultySelectLabel);
    headerLayout->addWidget(facultySelector);
    headerLayout->addSpacing(10);
    headerLayout->addWidget(backButton);
    root->addWidget(header);

    // Welcome heading
    auto *welcomeTitle = new QLabel("Faculty Dashboard", this);
    QFont welcomeFont = welcomeTitle->font();
    welcomeFont.setPointSize(23);
    welcomeFont.setBold(true);
    welcomeTitle->setFont(welcomeFont);
    root->addWidget(welcomeTitle);

    auto *welcomeText = new QLabel("Live teaching schedule and timetable updates connected to catalyst_db.", this);
    root->addWidget(welcomeText);

    // Summary cards
    auto *cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(16);

    auto createCard = [this, cardsLayout](const QString &title, QLabel *&valLabel, const QString &initialVal)
    {
        auto *card = new QFrame(this);
        card->setMinimumHeight(105);
        card->setFrameShape(QFrame::StyledPanel);

        QPalette cardPalette = card->palette();
        cardPalette.setColor(QPalette::Window, Qt::white);
        card->setPalette(cardPalette);
        card->setAutoFillBackground(true);

        auto *cardLayout = new QVBoxLayout(card);
        valLabel = new QLabel(initialVal, card);
        QFont valueFont = valLabel->font();
        valueFont.setPointSize(19);
        valueFont.setBold(true);
        valLabel->setFont(valueFont);

        auto *label = new QLabel(title, card);

        cardLayout->addWidget(valLabel);
        cardLayout->addWidget(label);
        cardLayout->addStretch();

        cardsLayout->addWidget(card);
    };

    createCard("Scheduled Classes", classesTodayLabel, "0");
    createCard("Next Class", nextClassLabel, "None scheduled");
    createCard("Database Status", scheduleStatusLabel, "catalyst_db connected");

    root->addLayout(cardsLayout);

    // Timetable Header & Day Filter Layout
    auto *tableHeaderLayout = new QHBoxLayout;

    auto *tableTitle = new QLabel("Timetable Schedule", this);
    QFont tableTitleFont = tableTitle->font();
    tableTitleFont.setPointSize(18);
    tableTitleFont.setBold(true);
    tableTitle->setFont(tableTitleFont);
    tableHeaderLayout->addWidget(tableTitle);

    tableHeaderLayout->addStretch();

    auto *dayLabel = new QLabel("Day:", this);
    QFont dayLabelFont = dayLabel->font();
    dayLabelFont.setBold(true);
    dayLabel->setFont(dayLabelFont);
    tableHeaderLayout->addWidget(dayLabel);

    daySelector = new QComboBox(this);
    daySelector->setMinimumWidth(160);
    daySelector->setMinimumHeight(32);
    daySelector->setStyleSheet(
        "QComboBox { background: #FFFFFF; color: #1E293B; border: 1px solid #CBD5E1; border-radius: 4px; padding: 4px 8px; font-weight: bold; }"
        "QComboBox QAbstractItemView { background: #FFFFFF; color: #1E293B; selection-background-color: #2563EB; selection-color: #FFFFFF; }");

    daySelector->addItem("Monday (Day 0)", 0);
    daySelector->addItem("Tuesday (Day 1)", 1);
    daySelector->addItem("Wednesday (Day 2)", 2);
    daySelector->addItem("Thursday (Day 3)", 3);
    daySelector->addItem("Friday (Day 4)", 4);
    daySelector->addItem("All Days", -1);

    connect(daySelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FacultyPortal::onDayChanged);

    tableHeaderLayout->addWidget(daySelector);
    root->addLayout(tableHeaderLayout);

    // Today's timetable table
    timetableWidget = new QTableWidget(0, 4, this);
    timetableWidget->setHorizontalHeaderLabels({"Time", "Subject", "Batch", "Room"});
    timetableWidget->setMinimumHeight(220);
    timetableWidget->setAlternatingRowColors(true);
    timetableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    timetableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    timetableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    timetableWidget->verticalHeader()->setVisible(false);
    timetableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    timetableWidget->horizontalHeader()->setMinimumHeight(36);
    timetableWidget->verticalHeader()->setDefaultSectionSize(35);
    timetableWidget->setStyleSheet("QTableWidget { background: #FFFFFF; gridline-color: #E2E8F0; }");

    root->addWidget(timetableWidget, 1);

    // Timetable update notice / Assignments
    auto *announcement = new QFrame(this);
    announcement->setFrameShape(QFrame::StyledPanel);

    QPalette announcementPalette = announcement->palette();
    announcementPalette.setColor(QPalette::Window, Qt::white);
    announcement->setPalette(announcementPalette);
    announcement->setAutoFillBackground(true);

    auto *announcementLayout = new QVBoxLayout(announcement);

    auto *announcementTitle = new QLabel("Course & Section Teaching Assignments", announcement);
    QFont announcementFont = announcementTitle->font();
    announcementFont.setPointSize(15);
    announcementFont.setBold(true);
    announcementTitle->setFont(announcementFont);

    announcementText = new QLabel("Loading assignments...", announcement);
    announcementText->setWordWrap(true);

    announcementLayout->addWidget(announcementTitle);
    announcementLayout->addWidget(announcementText);

    root->addWidget(announcement);

    // Populate data
    refreshData();
}

void FacultyPortal::refreshData()
{
    loadFacultyList();
}

void FacultyPortal::loadFacultyList()
{
    if (!facultySelector)
        return;

    QSignalBlocker blocker(facultySelector);
    facultySelector->clear();

    QList<DatabaseManager::Teacher> teachers = DatabaseManager::instance().getTeachers();
    for (const auto &t : teachers)
    {
        facultySelector->addItem(t.name, t.id);
    }

    if (facultySelector->count() > 0)
    {
        facultySelector->setCurrentIndex(0);
        int teacherId = facultySelector->currentData().toInt();
        int day = (daySelector) ? daySelector->currentData().toInt() : 0;
        loadFacultySchedule(teacherId, day);
    }
}

void FacultyPortal::onFacultyChanged(int index)
{
    if (index < 0 || !facultySelector)
        return;
    int teacherId = facultySelector->itemData(index).toInt();
    int day = (daySelector) ? daySelector->currentData().toInt() : 0;
    loadFacultySchedule(teacherId, day);
}

void FacultyPortal::onDayChanged(int index)
{
    if (index < 0 || !daySelector)
        return;
    int day = daySelector->itemData(index).toInt();
    int teacherId = (facultySelector && facultySelector->currentIndex() >= 0)
                        ? facultySelector->currentData().toInt()
                        : 1;
    loadFacultySchedule(teacherId, day);
}

void FacultyPortal::loadFacultySchedule(int teacherId, int day)
{
    static const QStringList dayNames = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

    QString selectedDayName;
    if (day >= 0 && day < dayNames.size())
    {
        selectedDayName = dayNames[day];
    }
    else
    {
        selectedDayName = "All Days";
    }

    QString teacherName = (facultySelector && facultySelector->currentIndex() >= 0)
                              ? facultySelector->currentText()
                              : "Faculty";

    QList<DatabaseManager::TimetableEntry> entries =
        DatabaseManager::instance().getFacultyTimetable(teacherId, day);

    int totalEntriesInDb = DatabaseManager::instance().getTimetableEntryCount();

    if (classesTodayLabel)
    {
        classesTodayLabel->setText(QString::number(entries.size()));
    }

    if (nextClassLabel)
    {
        if (!entries.isEmpty())
        {
            nextClassLabel->setText(entries.first().timeRange);
        }
        else
        {
            nextClassLabel->setText("None scheduled");
        }
    }

    if (scheduleStatusLabel)
    {
        scheduleStatusLabel->setText(QString("catalyst_db (%1 total in DB)").arg(totalEntriesInDb));
    }

    // Populate timetable table
    if (timetableWidget)
    {
        timetableWidget->clearContents();

        if (entries.isEmpty())
        {
            timetableWidget->setRowCount(1);
            QString emptyMsg;
            if (totalEntriesInDb == 0)
            {
                emptyMsg = "No timetable generated in database yet.";
            }
            else
            {
                emptyMsg = QString("No classes scheduled for %1 on %2.").arg(teacherName, selectedDayName);
            }
            timetableWidget->setItem(0, 0, new QTableWidgetItem("-"));
            timetableWidget->setItem(0, 1, new QTableWidgetItem(emptyMsg));
            timetableWidget->setItem(0, 2, new QTableWidgetItem("-"));
            timetableWidget->setItem(0, 3, new QTableWidgetItem("-"));
        }
        else
        {
            timetableWidget->setRowCount(entries.size());
            for (int i = 0; i < entries.size(); ++i)
            {
                const auto &e = entries[i];

                QString displayTime = e.timeRange;
                if (day < 0)
                {
                    QString dName = (e.day >= 0 && e.day < dayNames.size()) ? dayNames[e.day] : QString("Day %1").arg(e.day);
                    displayTime = QString("%1 %2").arg(dName, e.timeRange);
                }

                timetableWidget->setItem(i, 0, new QTableWidgetItem(displayTime));
                timetableWidget->setItem(i, 1, new QTableWidgetItem(e.subjectName));
                timetableWidget->setItem(i, 2, new QTableWidgetItem(QString("%1 - %2").arg(e.courseName, e.sectionName)));
                timetableWidget->setItem(i, 3, new QTableWidgetItem(e.roomName));
            }
        }
    }

    // Load teaching assignments from sectionsubjectteacher
    if (announcementText)
    {
        QList<DatabaseManager::TeacherAssignment> assigns =
            DatabaseManager::instance().getTeacherAssignments(teacherId);

        if (assigns.isEmpty())
        {
            announcementText->setText("No subject or section assignments mapped in catalyst_db for this faculty.");
        }
        else
        {
            QStringList lines;
            for (const auto &a : assigns)
            {
                lines.append(QString("• <b>%1</b> for <b>%2</b> (Section %3)")
                                 .arg(a.subjectName, a.courseName, a.sectionName));
            }
            announcementText->setText(QString("Mapped Teaching Assignments in catalyst_db:<br>%1")
                                          .arg(lines.join("<br>")));
        }
    }
}