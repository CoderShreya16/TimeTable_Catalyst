
#include "db/DatabaseManager.h"
#include "ui/AuthWindow.h"
#include "ui/WelcomePage.h"
#include "ui/admin/AdminPortal.h"
#include "ui/faculty/FacultyPortal.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QPalette>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QTextStream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[]) {
  // If invoked with --test-db, run connection test, print results, and exit
  bool isTest = false;
  for (int i = 1; i < argc; ++i) {
    if (QString::fromLocal8Bit(argv[i]) == "--test-db") {
      isTest = true;
      break;
    }
  }

  if (isTest) {
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
      freopen("CONOUT$", "w", stdout);
      freopen("CONOUT$", "w", stderr);
    }
#endif
    QCoreApplication testApp(argc, argv);
    QString report;
    bool ok = DatabaseManager::instance().testConnection(&report);

    // Print to standard streams and flush
    fprintf(stdout, "\n%s\n", report.toUtf8().constData());
    fflush(stdout);
    fprintf(stderr, "\n%s\n", report.toUtf8().constData());
    fflush(stderr);

    // Write report to both app directory and workspace directory
    QStringList outputPaths = {QDir(QCoreApplication::applicationDirPath())
                                   .filePath("db_test_result.txt"),
                               QDir(QCoreApplication::applicationDirPath())
                                   .filePath("../db_test_result.txt"),
                               "D:/final caty/db_test_result.txt"};
    for (const QString &path : outputPaths) {
      QFile outFile(path);
      if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&outFile);
        ts << report;
        outFile.close();
      }
    }

    DatabaseManager::instance().close();
    return ok ? 0 : 1;
  }

  QApplication app(argc, argv);

  // Attempt database connection on startup (non-blocking for UI)
  DatabaseManager::instance().connect();

  // Force Fusion style so Windows dark mode doesn't override our colors
  app.setStyle(QStyleFactory::create("Fusion"));

  // Set a light base palette
  QPalette lightPalette;
  lightPalette.setColor(QPalette::Window, QColor("#F4F6FA"));
  lightPalette.setColor(QPalette::WindowText, QColor("#1E293B"));
  lightPalette.setColor(QPalette::Base, QColor("#FFFFFF"));
  lightPalette.setColor(QPalette::AlternateBase, QColor("#EEF2F7"));
  lightPalette.setColor(QPalette::Text, QColor("#1E293B"));
  lightPalette.setColor(QPalette::Button, QColor("#E2E8F0"));
  lightPalette.setColor(QPalette::ButtonText, QColor("#1E293B"));
  lightPalette.setColor(QPalette::Highlight, QColor("#2563EB"));
  lightPalette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
  app.setPalette(lightPalette);

  QStackedWidget stack;
  stack.setWindowTitle("Catalyst - Smart Timetable Management");
  stack.resize(1200, 800);
  stack.setMinimumSize(1000, 680);

  auto *welcome = new WelcomePage;
  auto *auth = new AuthWindow;
  auto *admin = new AdminPortal;
  auto *faculty = new FacultyPortal;

  stack.addWidget(welcome);
  stack.addWidget(auth);
  stack.addWidget(admin);
  stack.addWidget(faculty);

  // Welcome page -> Login
  QObject::connect(welcome, &WelcomePage::loginRequested, &stack,
                   [&stack, auth]() {
                     auth->openLogin();
                     stack.setCurrentWidget(auth);
                   });

  // Welcome page -> Sign Up
  QObject::connect(welcome, &WelcomePage::signUpRequested, &stack,
                   [&stack, auth]() {
                     auth->openSignUp();
                     stack.setCurrentWidget(auth);
                   });

  // Login page -> Welcome page
  QObject::connect(auth, &AuthWindow::backRequested, &stack,
                   [&stack, welcome]() { stack.setCurrentWidget(welcome); });

  // Validated form -> Navigate to the appropriate portal
  QObject::connect(
      auth, &AuthWindow::authenticationRequested, &stack,
      [&stack, admin, faculty](const QString &role, bool isSignUp) {
        if (role.contains("Administrator", Qt::CaseInsensitive)) {
          stack.setCurrentWidget(admin);
        } else if (role.contains("Faculty", Qt::CaseInsensitive)) {
          stack.setCurrentWidget(faculty);
        }
      });

  // Admin portal -> Login
  QObject::connect(admin, &AdminPortal::backRequested, &stack,
                   [&stack, auth]() {
                     auth->openLogin();
                     stack.setCurrentWidget(auth);
                   });

  // Faculty portal -> Login
  QObject::connect(faculty, &FacultyPortal::backRequested, &stack,
                   [&stack, auth]() {
                     auth->openLogin();
                     stack.setCurrentWidget(auth);
                   });

  // Welcome page -> Explore More
  QObject::connect(welcome, &WelcomePage::exploreRequested, &stack, [&stack]() {
    QMessageBox::information(
        &stack, "Explore Catalyst",
        "Catalyst is designed to manage college timetables, "
        "detect scheduling conflicts, and coordinate updates.");
  });

  stack.setCurrentWidget(welcome);
  stack.show();

  int exitCode = app.exec();
  DatabaseManager::instance().close();
  return exitCode;
}
