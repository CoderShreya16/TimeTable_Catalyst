#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <QList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>


class DatabaseManager {
public:
  struct Config {
    QString driver;       // "auto", "QMYSQL", or "QODBC"
    QString host;         // default: "127.0.0.1"
    int port;             // default: 3306
    QString databaseName; // default: "catalyst_db"
    QString username;     // default: "root"
    QString password;     // default: ""
    QString odbcDriver;   // e.g. "MySQL ODBC 8.0 Unicode Driver"
  };

  struct Teacher {
    int id;
    QString name;
  };

  struct Course {
    int id;
    QString name;
  };

  struct Section {
    int id;
    QString name;
    int courseId;
    QString courseName;
    int studentCount;
  };

  struct Subject {
    int id;
    QString name;
    int lecturesPerWeek;
    QString type;
  };

  struct Room {
    int id;
    QString name;
    int capacity;
    QString type;
  };

  struct Timeslot {
    int id;
    int day;
    int slotIndex;
    QString startTime;
    QString endTime;
    bool isBreak;
  };

  struct TimetableEntry {
    int entryId;
    int sectionId;
    QString sectionName;
    QString courseName;
    int subjectId;
    QString subjectName;
    int teacherId;
    QString teacherName;
    int roomId;
    QString roomName;
    int day;
    int slotIndex;
    QString timeRange;
  };

  struct TeacherAssignment {
    QString subjectName;
    QString sectionName;
    QString courseName;
  };

  static DatabaseManager &instance();

  // Loads configuration from db_config.ini (or environment variables)
  Config loadConfig() const;

  // Connects to the database using the configured driver
  bool connect(QString *errorOut = nullptr);

  // Runs a diagnostic connection test and returns a clear summary report
  bool testConnection(QString *diagnosticOut = nullptr);

  // Checks if the database is open and valid
  bool isConnected() const;

  // Closes the database connection safely
  void close();

  // Access the underlying QSqlDatabase
  QSqlDatabase getDatabase() const;

  // Table Counts
  int getTeacherCount();
  int getSectionCount();
  int getRoomCount();
  int getSubjectCount();
  int getTimeslotCount();
  int getTimetableEntryCount();

  // Entity Queries
  QList<Teacher> getTeachers();
  QList<Course> getCourses();
  QList<Section> getSections();
  QList<Subject> getSubjects();
  QList<Room> getRooms();
  QList<Timeslot> getTimeslots(int day = -1);
  QList<TimetableEntry> getFacultyTimetable(int teacherId, int day = -1);
  QList<TeacherAssignment> getTeacherAssignments(int teacherId);

  // Connection name used by QSqlDatabase
  static const QString CONNECTION_NAME;

private:
  DatabaseManager();
  ~DatabaseManager();
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

  QString resolveOdbcDriver(const QString &configuredDriver) const;
  void ensureConnected();
};

#endif // DATABASE_MANAGER_H
