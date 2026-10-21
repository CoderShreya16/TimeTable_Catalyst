#ifndef FACULTY_PORTAL_H
#define FACULTY_PORTAL_H

#include <QWidget>

class QComboBox;
class QLabel;
class QTableWidget;

class FacultyPortal : public QWidget
{
    Q_OBJECT

public:
    explicit FacultyPortal(QWidget *parent = nullptr);

    // Refresh data from the database
    void refreshData();

signals:
    void backRequested();

private slots:
    void onFacultyChanged(int index);
    void onDayChanged(int index);

private:
    QComboBox *facultySelector;
    QComboBox *daySelector;
    QLabel *classesTodayLabel;
    QLabel *nextClassLabel;
    QLabel *scheduleStatusLabel;
    QTableWidget *timetableWidget;
    QLabel *announcementText;

    void loadFacultyList();
    void loadFacultySchedule(int teacherId, int day);
};

#endif // FACULTY_PORTAL_H