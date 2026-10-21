
#include "AuthWindow.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFont>
#include <QPalette>

namespace
{
    const QString navy = "#142B4A";
    const QString blue = "#2563EB";
    const QString light = "#F5F7FB";
    const QString muted = "#64748B";

    QLabel *makeText(const QString &text, int size, bool bold = false)
    {
        auto *label = new QLabel(text);

        QFont font;
        font.setPointSize(size);
        font.setBold(bold);
        label->setFont(font);

        return label;
    }

    QPushButton *makeAction(const QString &text, bool primary = true)
    {
        auto *button = new QPushButton(text);
        button->setMinimumHeight(42);
        button->setCursor(Qt::PointingHandCursor);

        QPalette palette = button->palette();
        palette.setColor(QPalette::Button,
                         QColor(primary ? blue : light));
        palette.setColor(QPalette::ButtonText,
                         QColor(primary ? "#FFFFFF" : navy));

        button->setPalette(palette);
        button->setAutoFillBackground(true);
        return button;
    }

    QLineEdit *makeInput(const QString &placeholder, bool password = false)
    {
        auto *input = new QLineEdit;
        input->setPlaceholderText(placeholder);
        input->setMinimumHeight(40);

        if (password)
            input->setEchoMode(QLineEdit::Password);

        return input;
    }

    QComboBox *makeRoleSelector()
    {
        auto *role = new QComboBox;
        role->addItem("Faculty");
        role->addItem("Administrator");
        role->setMinimumHeight(40);
        role->setStyleSheet(
            "QComboBox { background: #FFFFFF; color: #1E293B; "
            "border: 1px solid #CBD5E1; padding: 4px 8px; }"
            "QComboBox QAbstractItemView { background: #FFFFFF; "
            "color: #1E293B; selection-background-color: #2563EB; "
            "selection-color: #FFFFFF; }");
        return role;
    }
}

AuthWindow::AuthWindow(QWidget *parent)
    : QWidget(parent),
      pages(nullptr),
      loginEmail(nullptr),
      loginPassword(nullptr),
      signupName(nullptr),
      signupEmail(nullptr),
      signupPassword(nullptr),
      confirmPassword(nullptr),
      loginRole(nullptr),
      signupRole(nullptr),
      messageLabel(nullptr)
{
    setupUi();
}

void AuthWindow::setupUi()
{
    setMinimumSize(900, 650);
    setWindowTitle("Catalyst - Login");
    setAutoFillBackground(true);

    QPalette pagePalette = palette();
    pagePalette.setColor(QPalette::Window, QColor(light));
    setPalette(pagePalette);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(30, 24, 30, 24);
    root->setSpacing(16);

    auto *topBar = new QHBoxLayout;

    auto *brand = makeText("CATALYST", 19, true);
    QPalette brandPalette = brand->palette();
    brandPalette.setColor(QPalette::WindowText, QColor(navy));
    brand->setPalette(brandPalette);

    auto *backButton = makeAction("< Back to Home", false);
    topBar->addWidget(brand);
    topBar->addStretch();
    topBar->addWidget(backButton);

    root->addLayout(topBar);

    auto *card = new QFrame(this);
    card->setMaximumWidth(470);
    card->setFrameShape(QFrame::StyledPanel);
    card->setAutoFillBackground(true);

    QPalette cardPalette = card->palette();
    cardPalette.setColor(QPalette::Window, QColor("#FFFFFF"));
    cardPalette.setColor(QPalette::Mid, QColor("#DCE4EF"));
    card->setPalette(cardPalette);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(14);

    auto *heading = makeText("Welcome back", 24, true);
    auto *subtitle = makeText(
        "Sign in or create an account to continue.", 10);

    cardLayout->addWidget(heading);
    cardLayout->addWidget(subtitle);

    pages = new QStackedWidget(card);

    // Login page
    auto *loginPage = new QWidget;
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(0, 12, 0, 0);
    loginLayout->setSpacing(12);

    loginLayout->addWidget(makeText("Your role", 10, true));
    loginRole = makeRoleSelector();
    loginLayout->addWidget(loginRole);

    loginLayout->addWidget(makeText("Email address", 10, true));
    loginEmail = makeInput("Enter your college email");
    loginEmail->setMaxLength(254);
    loginLayout->addWidget(loginEmail);

    loginLayout->addWidget(makeText("Password", 10, true));
    loginPassword = makeInput("Enter your password", true);
    loginPassword->setMaxLength(128);
    loginLayout->addWidget(loginPassword);

    auto *loginButton = makeAction("Log In");
    auto *toSignUp = makeAction("New to Catalyst? Create an account", false);

    loginLayout->addSpacing(8);
    loginLayout->addWidget(loginButton);
    loginLayout->addWidget(toSignUp);
    loginLayout->addStretch();

    // Sign-up page
    auto *signupPage = new QWidget;
    auto *signupLayout = new QVBoxLayout(signupPage);
    signupLayout->setContentsMargins(0, 12, 0, 0);
    signupLayout->setSpacing(10);

    signupLayout->addWidget(makeText("Your name", 10, true));
    signupName = makeInput("Enter your full name");
    signupName->setMaxLength(100);
    signupLayout->addWidget(signupName);

    signupLayout->addWidget(makeText("Your role", 10, true));
    signupRole = makeRoleSelector();
    signupLayout->addWidget(signupRole);

    signupLayout->addWidget(makeText("Email address", 10, true));
    signupEmail = makeInput("Enter your college email");
    signupEmail->setMaxLength(254);
    signupLayout->addWidget(signupEmail);

    signupLayout->addWidget(makeText("Create password", 10, true));
    signupPassword = makeInput("At least 8 characters", true);
    signupPassword->setMaxLength(128);
    signupLayout->addWidget(signupPassword);

    signupLayout->addWidget(makeText("Confirm password", 10, true));
    confirmPassword = makeInput("Re-enter your password", true);
    confirmPassword->setMaxLength(128);
    signupLayout->addWidget(confirmPassword);

    auto *signupButton = makeAction("Create Account");
    auto *toLogin = makeAction("Already registered? Log in", false);

    signupLayout->addSpacing(4);
    signupLayout->addWidget(signupButton);
    signupLayout->addWidget(toLogin);
    signupLayout->addStretch();

    pages->addWidget(loginPage);
    pages->addWidget(signupPage);

    cardLayout->addWidget(pages);

    messageLabel = makeText("", 9);
    messageLabel->setWordWrap(true);
    cardLayout->addWidget(messageLabel);

    root->addStretch();

    auto *centerRow = new QHBoxLayout;
    centerRow->addStretch();
    centerRow->addWidget(card);
    centerRow->addStretch();

    root->addLayout(centerRow);
    root->addStretch();

    auto *footer = makeText(
        "Catalyst | Smart College Timetable Management", 9);

    QPalette footerPalette = footer->palette();
    footerPalette.setColor(QPalette::WindowText, QColor(muted));
    footer->setPalette(footerPalette);
    footer->setAlignment(Qt::AlignCenter);
    root->addWidget(footer);

    connect(backButton, &QPushButton::clicked,
            this, &AuthWindow::backRequested);

    connect(toSignUp, &QPushButton::clicked,
            this, &AuthWindow::showSignUp);

    connect(toLogin, &QPushButton::clicked,
            this, &AuthWindow::showLogin);

    connect(loginButton, &QPushButton::clicked,
            this, [this]() { submitForm(false); });

    connect(signupButton, &QPushButton::clicked,
            this, [this]() { submitForm(true); });
}

void AuthWindow::showLogin()
{
    pages->setCurrentIndex(0);
    messageLabel->clear();
    setWindowTitle("Catalyst - Login");
}

void AuthWindow::showSignUp()
{
    pages->setCurrentIndex(1);
    messageLabel->clear();
    setWindowTitle("Catalyst - Sign Up");
}

void AuthWindow::submitForm(bool isSignUp)
{
    QString name;
    QString email;
    QString password;
    QString role;

    if (isSignUp)
    {
        name = signupName->text().trimmed();
        email = signupEmail->text().trimmed();
        password = signupPassword->text();
        role = signupRole->currentText();

        if (name.isEmpty())
        {
            messageLabel->setText("Please enter your name.");
            return;
        }

        if (password != confirmPassword->text())
        {
            messageLabel->setText("Passwords do not match.");
            return;
        }
    }
    else
    {
        email = loginEmail->text().trimmed();
        password = loginPassword->text();
        role = loginRole->currentText();
    }

    if (email.isEmpty() || !email.contains('@'))
    {
        messageLabel->setText("Please enter a valid email address.");
        return;
    }


    if (password.size() < 8)
    {
        messageLabel->setText(
            "Password must contain at least 8 characters.");
        return;
    }

    // Input validation only; real authentication comes later.
    emit authenticationRequested(role, isSignUp);
}

void AuthWindow::openLogin()
{
    showLogin();
}

void AuthWindow::openSignUp()
{
    showSignUp();
}
