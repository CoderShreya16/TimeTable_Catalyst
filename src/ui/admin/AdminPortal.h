#ifndef ADMIN_PORTAL_H
#define ADMIN_PORTAL_H

#include <QWidget>

class QLabel;
class QTabWidget;
class QTableWidget;

class AdminPortal : public QWidget
{
    Q_OBJECT

public:
    explicit AdminPortal(QWidget *parent = nullptr);

    // Refresh data from the database
    void refreshData();

signals:
    void backRequested();

private:
    QLabel *facultyCountLabel;
    QLabel *sectionCountLabel;
    QLabel *roomCountLabel;
    QLabel *entryCountLabel;

    QTabWidget *dataTabs;
    QTableWidget *facultyTable;
    QTableWidget *sectionTable;
    QTableWidget *subjectTable;
    QTableWidget *roomTable;
    QTableWidget *timeslotTable;

    void populateFacultyTable();
    void populateSectionTable();
    void populateSubjectTable();
    void populateRoomTable();
    void populateTimeslotTable();
};

#endif // ADMIN_PORTAL_H
