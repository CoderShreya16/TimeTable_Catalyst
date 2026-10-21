
#ifndef WELCOME_PAGE_H
#define WELCOME_PAGE_H

#include <QWidget>

class WelcomePage : public QWidget {
  Q_OBJECT

public:
  explicit WelcomePage(QWidget *parent = nullptr);

signals:
signals:
  void loginRequested();
  void signUpRequested();
  void exploreRequested();

private:
  void setupUi();
};

#endif
