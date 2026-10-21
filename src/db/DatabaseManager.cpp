#include "DatabaseManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSqlRecord>


const QString DatabaseManager::CONNECTION_NAME = "CatalystDBConnection";

DatabaseManager &DatabaseManager::instance() {
  static DatabaseManager instance;
  return instance;
}

DatabaseManager::DatabaseManager() {}

DatabaseManager::~DatabaseManager() {
  // Only close and remove database if QCoreApplication is still alive
  if (QCoreApplication::instance()) {
    close();
  }
}

DatabaseManager::Config DatabaseManager::loadConfig() const {
  Config cfg;
  cfg.driver = "auto";
  cfg.host = "127.0.0.1";
  cfg.port = 3306;
  cfg.databaseName = "catalyst_db";
  cfg.username = "root";
  cfg.password = "";
  cfg.odbcDriver = "MySQL ODBC 8.0 Unicode Driver";

  QStringList candidatePaths = {
      QDir::current().filePath("db_config.ini"),
      QDir(QCoreApplication::applicationDirPath()).filePath("db_config.ini"),
      QDir(QCoreApplication::applicationDirPath()).filePath("../db_config.ini"),
      "D:/final caty/db_config.ini"};

  QString foundPath;
  for (const QString &path : candidatePaths) {
    if (QFile::exists(path)) {
      foundPath = path;
      break;
    }
  }

  if (!foundPath.isEmpty()) {
    QSettings settings(foundPath, QSettings::IniFormat);
    settings.beginGroup("database");
    cfg.driver = settings.value("driver", cfg.driver).toString().trimmed();
    cfg.host = settings.value("host", cfg.host).toString().trimmed();
    cfg.port = settings.value("port", cfg.port).toInt();
    cfg.databaseName =
        settings.value("database_name", cfg.databaseName).toString().trimmed();
    cfg.username =
        settings.value("username", cfg.username).toString().trimmed();
    cfg.password = settings.value("password", cfg.password).toString();
    cfg.odbcDriver =
        settings.value("odbc_driver", cfg.odbcDriver).toString().trimmed();
    settings.endGroup();
  }

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  if (env.contains("DB_DRIVER"))
    cfg.driver = env.value("DB_DRIVER").trimmed();
  if (env.contains("DB_HOST"))
    cfg.host = env.value("DB_HOST").trimmed();
  if (env.contains("DB_PORT")) {
    bool ok = false;
    int p = env.value("DB_PORT").toInt(&ok);
    if (ok && p > 0)
      cfg.port = p;
  }
  if (env.contains("DB_NAME"))
    cfg.databaseName = env.value("DB_NAME").trimmed();
  if (env.contains("DB_USER"))
    cfg.username = env.value("DB_USER").trimmed();
  if (env.contains("DB_PASS"))
    cfg.password = env.value("DB_PASS");
  if (env.contains("ODBC_DRIVER_NAME"))
    cfg.odbcDriver = env.value("ODBC_DRIVER_NAME").trimmed();

  return cfg;
}

QString
DatabaseManager::resolveOdbcDriver(const QString &configuredDriver) const {
  if (!configuredDriver.isEmpty()) {
    return configuredDriver;
  }
  return "MySQL ODBC 8.0 Unicode Driver";
}

bool DatabaseManager::connect(QString *errorOut) {
  if (!QCoreApplication::instance()) {
    if (errorOut)
      *errorOut = "Cannot connect: QCoreApplication is not initialized.";
    return false;
  }

  Config cfg = loadConfig();
  QStringList availableDrivers = QSqlDatabase::drivers();

  QString selectedDriver = cfg.driver;
  if (selectedDriver.compare("auto", Qt::CaseInsensitive) == 0) {
    if (availableDrivers.contains("QMYSQL", Qt::CaseInsensitive)) {
      selectedDriver = "QMYSQL";
    } else if (availableDrivers.contains("QODBC", Qt::CaseInsensitive)) {
      selectedDriver = "QODBC";
    } else {
      QString err =
          QString("No compatible SQL driver found. Qt available drivers: [%1]. "
                  "Neither QMYSQL nor QODBC is present.")
              .arg(availableDrivers.join(", "));
      if (errorOut)
        *errorOut = err;
      qWarning().noquote() << err;
      return false;
    }
  }

  QSqlDatabase db;
  if (QSqlDatabase::contains(CONNECTION_NAME)) {
    db = QSqlDatabase::database(CONNECTION_NAME);
    if (db.isOpen() &&
        db.driverName().compare(selectedDriver, Qt::CaseInsensitive) == 0) {
      return true;
    }
    db.close();
    QSqlDatabase::removeDatabase(CONNECTION_NAME);
  }

  db = QSqlDatabase::addDatabase(selectedDriver, CONNECTION_NAME);

  if (selectedDriver.compare("QMYSQL", Qt::CaseInsensitive) == 0) {
    db.setHostName(cfg.host);
    db.setPort(cfg.port);
    db.setDatabaseName(cfg.databaseName);
    db.setUserName(cfg.username);
    db.setPassword(cfg.password);
  } else if (selectedDriver.compare("QODBC", Qt::CaseInsensitive) == 0) {
    QString odbcDriver = resolveOdbcDriver(cfg.odbcDriver);
    QString connStr =
        QString(
            "Driver={%1};Server=%2;Port=%3;Database=%4;Uid=%5;Pwd=%6;Option=3;")
            .arg(odbcDriver, cfg.host, QString::number(cfg.port),
                 cfg.databaseName, cfg.username, cfg.password);
    db.setDatabaseName(connStr);
  }

  if (!db.open()) {
    QSqlError lastErr = db.lastError();
    QString errText = lastErr.text().trimmed();
    if (errText.isEmpty()) {
      errText = "Unknown error opening database.";
    }

    QString detailed =
        QString(
            "Failed to connect using driver '%1' to database '%2' at %3:%4.\n"
            "Error: %5")
            .arg(selectedDriver, cfg.databaseName, cfg.host,
                 QString::number(cfg.port), errText);

    if (selectedDriver.compare("QODBC", Qt::CaseInsensitive) == 0 &&
        errText.contains("Data source name not found", Qt::CaseInsensitive)) {
      detailed += QString("\n\nHint: The ODBC driver '%1' was not found in "
                          "Windows ODBC Data Sources. "
                          "Make sure MySQL Connector/ODBC (64-bit) is "
                          "installed on your system.")
                      .arg(resolveOdbcDriver(cfg.odbcDriver));
    }

    if (errorOut)
      *errorOut = detailed;
    qWarning().noquote() << detailed;
    return false;
  }

  return true;
}

bool DatabaseManager::isConnected() const {
  if (!QCoreApplication::instance())
    return false;
  if (!QSqlDatabase::contains(CONNECTION_NAME))
    return false;
  QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME);
  return db.isOpen() && db.isValid();
}

void DatabaseManager::ensureConnected() {
  if (!isConnected()) {
    connect();
  }
}

void DatabaseManager::close() {
  if (!QCoreApplication::instance())
    return;
  if (QSqlDatabase::contains(CONNECTION_NAME)) {
    {
      QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME);
      if (db.isOpen()) {
        db.close();
      }
    }
    QSqlDatabase::removeDatabase(CONNECTION_NAME);
  }
}

QSqlDatabase DatabaseManager::getDatabase() const {
  if (QCoreApplication::instance() && QSqlDatabase::contains(CONNECTION_NAME)) {
    return QSqlDatabase::database(CONNECTION_NAME);
  }
  return QSqlDatabase();
}

int DatabaseManager::getTeacherCount() {
  ensureConnected();
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT COUNT(*) FROM teacher") && q.next()) {
    return q.value(0).toInt();
  }
  return 0;
}

int DatabaseManager::getSectionCount() {
  ensureConnected();
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT COUNT(*) FROM section") && q.next()) {
    return q.value(0).toInt();
  }
  return 0;
}

int DatabaseManager::getRoomCount() {
  ensureConnected();
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT COUNT(*) FROM room") && q.next()) {
    return q.value(0).toInt();
  }
  return 0;
}

int DatabaseManager::getSubjectCount() {
  ensureConnected();
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT COUNT(*) FROM subject") && q.next()) {
    return q.value(0).toInt();
  }
  return 0;
}

int DatabaseManager::getTimeslotCount() {
  ensureConnected();
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT COUNT(*) FROM timeslot") && q.next()) {
    return q.value(0).toInt();
  }
  return 0;
}

int DatabaseManager::getTimetableEntryCount() {
  ensureConnected();
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT COUNT(*) FROM timetableentry") && q.next()) {
    return q.value(0).toInt();
  }
  return 0;
}

QList<DatabaseManager::Teacher> DatabaseManager::getTeachers() {
  ensureConnected();
  QList<Teacher> list;
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT teacher_id, name FROM teacher ORDER BY teacher_id ASC")) {
    while (q.next()) {
      Teacher t;
      t.id = q.value(0).toInt();
      t.name = q.value(1).toString();
      list.append(t);
    }
  }
  return list;
}

QList<DatabaseManager::Course> DatabaseManager::getCourses() {
  ensureConnected();
  QList<Course> list;
  QSqlQuery q(getDatabase());
  if (q.exec(
          "SELECT course_id, course_name FROM course ORDER BY course_id ASC")) {
    while (q.next()) {
      Course c;
      c.id = q.value(0).toInt();
      c.name = q.value(1).toString();
      list.append(c);
    }
  }
  return list;
}

QList<DatabaseManager::Section> DatabaseManager::getSections() {
  ensureConnected();
  QList<Section> list;
  QSqlQuery q(getDatabase());
  const QString sql = "SELECT s.section_id, s.section_name, s.course_id, "
                      "COALESCE(c.course_name, '-'), s.student_count "
                      "FROM section s "
                      "LEFT JOIN course c ON s.course_id = c.course_id "
                      "ORDER BY s.course_id ASC, s.section_name ASC";
  if (q.exec(sql)) {
    while (q.next()) {
      Section s;
      s.id = q.value(0).toInt();
      s.name = q.value(1).toString();
      s.courseId = q.value(2).toInt();
      s.courseName = q.value(3).toString();
      s.studentCount = q.value(4).toInt();
      list.append(s);
    }
  }
  return list;
}

QList<DatabaseManager::Subject> DatabaseManager::getSubjects() {
  ensureConnected();
  QList<Subject> list;
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT subject_id, subject_name, lectures_per_week, subject_type "
             "FROM subject ORDER BY subject_id ASC")) {
    while (q.next()) {
      Subject sub;
      sub.id = q.value(0).toInt();
      sub.name = q.value(1).toString();
      sub.lecturesPerWeek = q.value(2).toInt();
      sub.type = q.value(3).toString();
      list.append(sub);
    }
  }
  return list;
}

QList<DatabaseManager::Room> DatabaseManager::getRooms() {
  ensureConnected();
  QList<Room> list;
  QSqlQuery q(getDatabase());
  if (q.exec("SELECT room_id, room_name, capacity, room_type FROM room ORDER "
             "BY room_id ASC")) {
    while (q.next()) {
      Room r;
      r.id = q.value(0).toInt();
      r.name = q.value(1).toString();
      r.capacity = q.value(2).toInt();
      r.type = q.value(3).toString();
      list.append(r);
    }
  }
  return list;
}

QList<DatabaseManager::Timeslot> DatabaseManager::getTimeslots(int day) {
  ensureConnected();
  QList<Timeslot> list;
  QSqlQuery q(getDatabase());
  QString sql = "SELECT slot_id, day, slot_index, "
                "DATE_FORMAT(start_time, '%H:%i'), DATE_FORMAT(end_time, "
                "'%H:%i'), is_break "
                "FROM timeslot ";
  if (day >= 0) {
    sql += QString("WHERE day = %1 ").arg(day);
  }
  sql += "ORDER BY day ASC, slot_index ASC";

  if (q.exec(sql)) {
    while (q.next()) {
      Timeslot ts;
      ts.id = q.value(0).toInt();
      ts.day = q.value(1).toInt();
      ts.slotIndex = q.value(2).toInt();
      ts.startTime = q.value(3).toString();
      ts.endTime = q.value(4).toString();
      ts.isBreak = q.value(5).toBool();
      list.append(ts);
    }
  }
  return list;
}

QList<DatabaseManager::TimetableEntry>
DatabaseManager::getFacultyTimetable(int teacherId, int day) {
  ensureConnected();
  QList<TimetableEntry> list;
  QSqlQuery q(getDatabase());
  QString sql =
      "SELECT "
      "te.entry_id, "
      "te.section_id, sec.section_name, c.course_name, "
      "te.subject_id, sub.subject_name, "
      "te.teacher_id, t.name, "
      "te.room_id, r.room_name, "
      "te.day, te.slot_index, "
      "CONCAT(DATE_FORMAT(ts.start_time, '%H:%i'), ' - ', "
      "DATE_FORMAT(ts.end_time, '%H:%i')) AS time_range "
      "FROM timetableentry te "
      "JOIN timeslot ts ON te.day = ts.day AND te.slot_index = ts.slot_index "
      "JOIN subject sub ON te.subject_id = sub.subject_id "
      "JOIN section sec ON te.section_id = sec.section_id "
      "JOIN course c ON sec.course_id = c.course_id "
      "JOIN room r ON te.room_id = r.room_id "
      "JOIN teacher t ON te.teacher_id = t.teacher_id "
      "WHERE te.teacher_id = :teacher_id ";
  if (day >= 0) {
    sql += QString("AND te.day = %1 ").arg(day);
  }
  sql += "ORDER BY te.day ASC, te.slot_index ASC";

  q.prepare(sql);
  q.bindValue(":teacher_id", teacherId);

  if (!q.exec()) {
    qWarning() << "getFacultyTimetable SQL failed:" << q.lastError().text()
               << "Query:" << sql;
  } else {
    while (q.next()) {
      TimetableEntry entry;
      entry.entryId = q.value(0).toInt();
      entry.sectionId = q.value(1).toInt();
      entry.sectionName = q.value(2).toString();
      entry.courseName = q.value(3).toString();
      entry.subjectId = q.value(4).toInt();
      entry.subjectName = q.value(5).toString();
      entry.teacherId = q.value(6).toInt();
      entry.teacherName = q.value(7).toString();
      entry.roomId = q.value(8).toInt();
      entry.roomName = q.value(9).toString();
      entry.day = q.value(10).toInt();
      entry.slotIndex = q.value(11).toInt();
      entry.timeRange = q.value(12).toString();
      list.append(entry);
    }
  }
  return list;
}

QList<DatabaseManager::TeacherAssignment>
DatabaseManager::getTeacherAssignments(int teacherId) {
  ensureConnected();
  QList<TeacherAssignment> list;
  QSqlQuery q(getDatabase());
  const QString sql =
      "SELECT sub.subject_name, sec.section_name, c.course_name "
      "FROM sectionsubjectteacher sst "
      "JOIN subject sub ON sst.subject_id = sub.subject_id "
      "JOIN section sec ON sst.section_id = sec.section_id "
      "JOIN course c ON sec.course_id = c.course_id "
      "WHERE sst.teacher_id = :teacher_id "
      "ORDER BY sub.subject_name ASC, sec.section_name ASC";
  q.prepare(sql);
  q.bindValue(":teacher_id", teacherId);
  if (q.exec()) {
    while (q.next()) {
      TeacherAssignment ta;
      ta.subjectName = q.value(0).toString();
      ta.sectionName = q.value(1).toString();
      ta.courseName = q.value(2).toString();
      list.append(ta);
    }
  }
  return list;
}

bool DatabaseManager::testConnection(QString *diagnosticOut) {
  QString report;
  report += "========================================\n";
  report += "   Catalyst Database Diagnostic Test    \n";
  report += "========================================\n";

  QStringList drivers = QSqlDatabase::drivers();
  report += QString("Qt Available SQL Drivers: [%1]\n").arg(drivers.join(", "));

  Config cfg = loadConfig();
  report +=
      QString("Config Host: %1:%2\n").arg(cfg.host, QString::number(cfg.port));
  report += QString("Config Database: %1\n").arg(cfg.databaseName);
  report += QString("Config User: %1\n").arg(cfg.username);
  report += QString("Config Driver Mode: %1\n").arg(cfg.driver);
  if (cfg.driver.compare("QMYSQL", Qt::CaseInsensitive) != 0) {
    report += QString("Config ODBC Driver: %1\n").arg(cfg.odbcDriver);
  }
  report += "----------------------------------------\n";

  QString connectError;
  bool ok = connect(&connectError);

  if (!ok) {
    report += "STATUS: FAILED\n\n";
    report += connectError + "\n";
    report += "========================================\n";
    if (diagnosticOut)
      *diagnosticOut = report;
    qCritical().noquote() << report;
    return false;
  }

  QSqlDatabase db = getDatabase();
  report += QString("STATUS: SUCCESS!\n");
  report += QString("Active Driver: %1\n").arg(db.driverName());

  QSqlQuery q(db);
  if (q.exec("SELECT DATABASE(), VERSION()")) {
    if (q.next()) {
      report += QString("Connected Database: %1\n").arg(q.value(0).toString());
      report +=
          QString("MySQL Server Version: %1\n").arg(q.value(1).toString());
    }
  }

  if (q.exec("SHOW TABLES")) {
    QStringList tables;
    while (q.next()) {
      tables.append(q.value(0).toString());
    }
    report += QString("Found %1 tables: [%2]\n")
                  .arg(QString::number(tables.size()), tables.join(", "));
  }

  if (q.exec("SELECT COUNT(*) FROM timetableentry")) {
    if (q.next()) {
      report += QString("Row count in 'timetableentry': %1\n")
                    .arg(q.value(0).toString());
    }
  }

  // Diagnostic query tests
  QList<TimetableEntry> drRaoDay0 = getFacultyTimetable(1, 0);
  report +=
      QString("Dr. Rao (id=1, day=0): %1 entries\n").arg(drRaoDay0.size());
  for (const auto &e : drRaoDay0) {
    report += QString("   - %1 | %2 | %3 | %4\n")
                  .arg(e.timeRange, e.subjectName, e.sectionName, e.roomName);
  }

  QList<TimetableEntry> drRaoDay1 = getFacultyTimetable(1, 1);
  report +=
      QString("Dr. Rao (id=1, day=1): %1 entries\n").arg(drRaoDay1.size());

  QList<TimetableEntry> msVermaDay0 = getFacultyTimetable(3, 0);
  report +=
      QString("Ms. Verma (id=3, day=0): %1 entries\n").arg(msVermaDay0.size());
  for (const auto &e : msVermaDay0) {
    report += QString("   - %1 | %2 | %3 | %4\n")
                  .arg(e.timeRange, e.subjectName, e.sectionName, e.roomName);
  }

  report += "========================================\n";

  if (diagnosticOut)
    *diagnosticOut = report;
  qInfo().noquote() << report;
  return true;
}
