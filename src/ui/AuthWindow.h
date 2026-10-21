
#ifndef AUTH_WINDOW_H
#define AUTH_WINDOW_H

#include <QWidget>
#include <QString>

class QLineEdit;
class QComboBox;
class QStackedWidget;
class QLabel;

class AuthWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AuthWindow(QWidget *parent = nullptr);

    // These methods must be public so main.cpp can call them.
    void openLogin();
    void openSignUp();

signals:
    void backRequested();
    void authenticationRequested(const QString &role,
                                 bool isSignUp);

private:
    void setupUi();
    void showLogin();
    void showSignUp();
    void submitForm(bool isSignUp);

    QStackedWidget *pages;

    QLineEdit *loginEmail;
    QLineEdit *loginPassword;

    QLineEdit *signupName;
    QLineEdit *signupEmail;
    QLineEdit *signupPassword;
    QLineEdit *confirmPassword;

    QComboBox *loginRole;
    QComboBox *signupRole;

    QLabel *messageLabel;
};

#endif

