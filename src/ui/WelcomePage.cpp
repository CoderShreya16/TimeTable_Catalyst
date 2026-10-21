
#include "WelcomePage.h"
#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>


namespace {
void setBackground(QWidget *widget, const QColor &color) {
  QPalette palette = widget->palette();
  palette.setColor(QPalette::Window, color);
  widget->setPalette(palette);
  widget->setAutoFillBackground(true);
}

QLabel *makeLabel(const QString &text, int size, const QColor &color,
                  bool bold = false) {
  auto *label = new QLabel(text);
  QFont font;
  font.setPointSize(size);
  font.setBold(bold);
  label->setFont(font);

  QPalette palette = label->palette();
  palette.setColor(QPalette::WindowText, color);
  label->setPalette(palette);
  label->setWordWrap(true);

  return label;
}

QPushButton *makeButton(const QString &text, const QColor &background,
                        const QColor &foreground) {
  auto *button = new QPushButton(text);
  button->setMinimumHeight(42);
  button->setMinimumWidth(115);
  button->setCursor(Qt::PointingHandCursor);

  QFont font;
  font.setPointSize(10);
  font.setBold(true);
  button->setFont(font);

  QPalette palette = button->palette();
  palette.setColor(QPalette::Button, background);
  palette.setColor(QPalette::ButtonText, foreground);
  button->setPalette(palette);
  button->setAutoFillBackground(true);

  return button;
}
} // namespace

WelcomePage::WelcomePage(QWidget *parent) : QWidget(parent) { setupUi(); }

void WelcomePage::saetupUi() {
  const QColor navy("#142B4A");
  const QColor blue("#2563EB");
  const QColor background("#F5F7FB");
  const QColor white("#FFFFFF");
  const QColor darkText("#172B4D");
  const QColor muted("#64748B");
  const QColor border("#DCE4EF");

  setWindowTitle("Catalyst - Smart Timetable Management");
  setMinimumSize(900, 650);
  setBackground(this, background);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 1. Header
  auto *header = new QFrame(this);
  setBackground(header, navy);
  header->setMinimumHeight(78);

  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(36, 12, 36, 12);
  headerLayout->setSpacing(14);

  auto *brandLayout = new QVBoxLayout();
  brandLayout->setSpacing(2);

  auto *brand = makeLabel("CATALYST", 19, white, true);
  auto *tagline = makeLabel("SMART TIMETABLE MANAGEMENT", 8, QColor("#C7D5E8"));

  brandLayout->addWidget(brand);
  brandLayout->addWidget(tagline);

  headerLayout->addLayout(brandLayout);
  headerLayout->addStretch();

  auto *aboutButton = makeButton("About Us", navy, white);
  auto *helpButton = makeButton("Help", navy, white);
  auto *infoButton = makeButton("Info", navy, white);
  auto *loginButton = makeButton("Log In", navy, white);
  auto *signUpButton = makeButton("Sign Up", blue, white);

  headerLayout->addWidget(aboutButton);
  headerLayout->addWidget(helpButton);
  headerLayout->addWidget(infoButton);
  headerLayout->addWidget(loginButton);
  headerLayout->addWidget(signUpButton);

  mainLayout->addWidget(header);

  // 2. Hero section
  auto *hero = new QFrame(this);
  setBackground(hero, white);

  auto *heroLayout = new QVBoxLayout(hero);
  heroLayout->setContentsMargins(50, 55, 50, 55);
  heroLayout->setSpacing(18);
  heroLayout->setAlignment(Qt::AlignCenter);

  auto *eyebrow =
      makeLabel("A SMARTER WAY TO MANAGE CAMPUS SCHEDULES", 10, blue, true);
  eyebrow->setAlignment(Qt::AlignCenter);

  auto *heroTitle = makeLabel("Welcome to Catalyst", 32, darkText, true);
  heroTitle->setAlignment(Qt::AlignCenter);

  auto *description =
      makeLabel("One campus. Better coordination.\n"
                "Create conflict-free timetables and keep everyone in sync.",
                12, muted);
  description->setAlignment(Qt::AlignCenter);

  auto *heroButtons = new QHBoxLayout();
  heroButtons->setSpacing(14);

  auto *exploreButton = makeButton("Explore More", navy, white);
  auto *heroLoginButton = makeButton("Get Started", blue, white);

  heroButtons->addStretch();
  heroButtons->addWidget(heroLoginButton);
  heroButtons->addWidget(exploreButton);
  heroButtons->addStretch();

  heroLayout->addStretch();
  heroLayout->addWidget(eyebrow);
  heroLayout->addWidget(heroTitle);
  heroLayout->addWidget(description);
  heroLayout->addLayout(heroButtons);
  heroLayout->addStretch();

  mainLayout->addWidget(hero, 3);

  // 3. Features section
  auto *features = new QFrame(this);
  setBackground(features, background);

  auto *featuresLayout = new QVBoxLayout(features);
  featuresLayout->setContentsMargins(36, 30, 36, 30);
  featuresLayout->setSpacing(20);

  auto *featuresTitle =
      makeLabel("One campus. Better coordination.", 22, darkText, true);
  featuresTitle->setAlignment(Qt::AlignCenter);

  auto *featuresSubtitle = makeLabel(
      "Everything your institution needs for smoother scheduling.", 10, muted);
  featuresSubtitle->setAlignment(Qt::AlignCenter);

  featuresLayout->addWidget(featuresTitle);
  featuresLayout->addWidget(featuresSubtitle);

  auto *cardsLayout = new QHBoxLayout();
  cardsLayout->setSpacing(18);

  auto addFeatureCard = [&](const QString &symbol, const QString &title,
                            const QString &description) {
    auto *card = new QFrame(features);
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Plain);
    card->setLineWidth(1);
    card->setMidLineWidth(0);

    QPalette cardPalette = card->palette();
    cardPalette.setColor(QPalette::Window, white);
    cardPalette.setColor(QPalette::Mid, border);
    card->setPalette(cardPalette);
    card->setAutoFillBackground(true);
    card->setMinimumHeight(165);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(10);

    auto *icon = makeLabel(symbol, 25, blue, true);
    auto *cardTitle = makeLabel(title, 13, darkText, true);
    auto *cardDescription = makeLabel(description, 10, muted);

    cardLayout->addWidget(icon);
    cardLayout->addWidget(cardTitle);
    cardLayout->addWidget(cardDescription);
    cardLayout->addStretch();

    cardsLayout->addWidget(card);
  };

  addFeatureCard("01", "Smart Scheduling",
                 "Organize classes, faculty and rooms into a clear timetable.");

  addFeatureCard("02", "Clash Detection",
                 "Identify scheduling conflicts before they disrupt a class.");

  addFeatureCard(
      "03", "Live Updates",
      "Keep timetable changes visible across open application windows.");

  featuresLayout->addLayout(cardsLayout);
  mainLayout->addWidget(features, 2);

  // 4. Footer
  auto *footer = new QFrame(this);
  setBackground(footer, navy);
  footer->setMinimumHeight(48);

  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(30, 10, 30, 10);

  auto *footerText =
      makeLabel("CATALYST  |  Smarter scheduling for every campus", 9, white);

  footerLayout->addWidget(footerText);
  footerLayout->addStretch();

  auto *copyright =
      makeLabel("Built for better coordination", 9, QColor("#C7D5E8"));

  footerLayout->addWidget(copyright);
  mainLayout->addWidget(footer);

  // Navigation signals
  connect(loginButton, &QPushButton::clicked, this,
          &WelcomePage::loginRequested);

  connect(heroLoginButton, &QPushButton::clicked, this,
          &WelcomePage::loginRequested);

  connect(signUpButton, &QPushButton::clicked, this,
          &WelcomePage::signUpRequested);

  connect(exploreButton, &QPushButton::clicked, this,
          &WelcomePage::exploreRequested);

  // Temporary informational actions
  connect(aboutButton, &QPushButton::clicked, this, [this]() {
    // About section can be added next.
    emit exploreRequested();
  });

  connect(helpButton, &QPushButton::clicked, this, [this]() {
    // Help section can be added next.
    emit exploreRequested();
  });

  connect(infoButton, &QPushButton::clicked, this, [this]() {
    // Information section can be added next.
    emit exploreRequested();
  });
}
