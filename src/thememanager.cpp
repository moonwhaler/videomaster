#include "thememanager.h"
#include <QStyleHints>
#include <QTimer>
#include <QSettings>
#include <QPalette>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance()
{
    if (!s_instance) {
        s_instance = new ThemeManager();
    }
    return s_instance;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentTheme(System)
    , m_systemIsDark(false)
{
    // Detect initial system theme
    m_systemIsDark = detectSystemDarkMode();
    
    // Connect to system theme changes
    connect(QApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &ThemeManager::onSystemThemeChanged);
    
    // Load saved theme preference
    loadThemePreference();
    
    // Apply initial theme
    applyTheme();
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        saveThemePreference();
        applyTheme();
        emit themeChanged();
    }
}

bool ThemeManager::isDarkMode() const
{
    switch (m_currentTheme) {
        case Dark:
            return true;
        case Light:
            return false;
        case System:
        default:
            return m_systemIsDark;
    }
}

void ThemeManager::onSystemThemeChanged()
{
    bool newSystemIsDark = detectSystemDarkMode();
    if (m_systemIsDark != newSystemIsDark) {
        m_systemIsDark = newSystemIsDark;
        if (m_currentTheme == System) {
            applyTheme();
            emit themeChanged();
        }
    }
}

bool ThemeManager::detectSystemDarkMode() const
{
    // Qt 6.5+ has better support for this
    auto colorScheme = QApplication::styleHints()->colorScheme();
    if (colorScheme == Qt::ColorScheme::Dark) {
        return true;
    } else if (colorScheme == Qt::ColorScheme::Light) {
        return false;
    }
    
    // Fallback: check if system palette is dark
    QPalette palette = QApplication::palette();
    QColor windowColor = palette.color(QPalette::Window);
    QColor textColor = palette.color(QPalette::WindowText);
    
    // If window is darker than text, we're likely in dark mode
    return windowColor.lightness() < textColor.lightness();
}

void ThemeManager::applyTheme()
{
    // The actual theme application is handled by individual widgets
    // requesting colors from this manager
}

void ThemeManager::saveThemePreference()
{
    QSettings settings;
    settings.setValue("theme", static_cast<int>(m_currentTheme));
}

void ThemeManager::loadThemePreference()
{
    QSettings settings;
    int themeValue = settings.value("theme", static_cast<int>(System)).toInt();
    m_currentTheme = static_cast<Theme>(themeValue);
}

// Color definitions for light and dark themes
QString ThemeManager::backgroundColor() const
{
    return isDarkMode() ? "#0d1117" : "#ffffff";
}

QString ThemeManager::surfaceColor() const
{
    return isDarkMode() ? "#161b22" : "#f6f8fa";
}

QString ThemeManager::borderColor() const
{
    return isDarkMode() ? "#30363d" : "#d0d7de";
}

QString ThemeManager::textColor() const
{
    return isDarkMode() ? "#f0f6fc" : "#24292f";
}

QString ThemeManager::secondaryTextColor() const
{
    return isDarkMode() ? "#8b949e" : "#656d76";
}

QString ThemeManager::primaryColor() const
{
    return isDarkMode() ? "#238636" : "#0969da";
}

QString ThemeManager::primaryHoverColor() const
{
    return isDarkMode() ? "#2ea043" : "#0860ca";
}

QString ThemeManager::primaryPressedColor() const
{
    return isDarkMode() ? "#1a7f37" : "#0757ba";
}

QString ThemeManager::successColor() const
{
    return isDarkMode() ? "#238636" : "#1f883d";
}

QString ThemeManager::successHoverColor() const
{
    return isDarkMode() ? "#2ea043" : "#1a7f37";
}

QString ThemeManager::dangerColor() const
{
    return isDarkMode() ? "#da3633" : "#cf222e";
}

QString ThemeManager::dangerHoverColor() const
{
    return isDarkMode() ? "#f85149" : "#b91c1c";
}

QString ThemeManager::inputBackgroundColor() const
{
    return isDarkMode() ? "#0d1117" : "#ffffff";
}

QString ThemeManager::hoverBackgroundColor() const
{
    return isDarkMode() ? "#21262d" : "#eaeef2";
}

QString ThemeManager::selectionBackgroundColor() const
{
    return isDarkMode() ? "#238636" : "#0969da";
}

QString ThemeManager::disabledColor() const
{
    return isDarkMode() ? "#484f58" : "#8c959f";
}

// Complete stylesheets
QString ThemeManager::buttonStyleSheet() const
{
    return QString(
        "QPushButton { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 3px; "
        "   color: %3; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 6px 12px; "
        "   min-width: 60px; "
        "} "
        "QPushButton:hover { "
        "   background-color: %4; "
        "   border-color: %5; "
        "} "
        "QPushButton:pressed { "
        "   background-color: %6; "
        "} "
        "QPushButton:disabled { "
        "   background-color: %1; "
        "   border-color: %2; "
        "   color: %7; "
        "}")
        .arg(surfaceColor())
        .arg(borderColor())
        .arg(textColor())
        .arg(hoverBackgroundColor())
        .arg(isDarkMode() ? "#8b949e" : "#afb8c1")
        .arg(isDarkMode() ? "#30363d" : "#e1e6ea")
        .arg(disabledColor());
}

QString ThemeManager::primaryButtonStyleSheet() const
{
    return QString(
        "QPushButton { "
        "   background-color: %1; "
        "   border: 1px solid %1; "
        "   border-radius: 3px; "
        "   color: #ffffff; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 6px 12px; "
        "   min-width: 60px; "
        "} "
        "QPushButton:hover { "
        "   background-color: %2; "
        "   border-color: %2; "
        "} "
        "QPushButton:pressed { "
        "   background-color: %3; "
        "} "
        "QPushButton:disabled { "
        "   background-color: %4; "
        "   border-color: %4; "
        "   color: #ffffff; "
        "}")
        .arg(primaryColor())
        .arg(primaryHoverColor())
        .arg(primaryPressedColor())
        .arg(disabledColor());
}

QString ThemeManager::successButtonStyleSheet() const
{
    return QString(
        "QPushButton { "
        "   background-color: %1; "
        "   border: 1px solid %1; "
        "   border-radius: 3px; "
        "   color: #ffffff; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 8px 16px; "
        "   min-width: 120px; "
        "} "
        "QPushButton:hover { "
        "   background-color: %2; "
        "   border-color: %2; "
        "} "
        "QPushButton:pressed { "
        "   background-color: %3; "
        "} "
        "QPushButton:disabled { "
        "   background-color: %4; "
        "   border-color: %4; "
        "   color: #ffffff; "
        "}")
        .arg(successColor())
        .arg(successHoverColor())
        .arg(isDarkMode() ? "#1a7f37" : "#166f2c")
        .arg(disabledColor());
}

QString ThemeManager::dangerButtonStyleSheet() const
{
    return QString(
        "QPushButton { "
        "   background-color: %1; "
        "   border: 1px solid %1; "
        "   border-radius: 3px; "
        "   color: #ffffff; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 8px 16px; "
        "   min-width: 100px; "
        "} "
        "QPushButton:hover { "
        "   background-color: %2; "
        "   border-color: %2; "
        "} "
        "QPushButton:pressed { "
        "   background-color: %3; "
        "} "
        "QPushButton:disabled { "
        "   background-color: %4; "
        "   border-color: %4; "
        "   color: #ffffff; "
        "}")
        .arg(dangerColor())
        .arg(dangerHoverColor())
        .arg(isDarkMode() ? "#b91c1c" : "#9f1239")
        .arg(disabledColor());
}

QString ThemeManager::lineEditStyleSheet() const
{
    return QString(
        "QLineEdit { "
        "   border: 1px solid %1; "
        "   border-radius: 3px; "
        "   padding: 6px 8px; "
        "   font-size: 12px; "
        "   background-color: %2; "
        "   color: %3; "
        "} "
        "QLineEdit:focus { "
        "   border-color: %4; "
        "   outline: none; "
        "}")
        .arg(borderColor())
        .arg(inputBackgroundColor())
        .arg(textColor())
        .arg(primaryColor());
}

QString ThemeManager::groupBoxStyleSheet() const
{
    return QString(
        "QGroupBox { "
        "   font-size: 13px; "
        "   font-weight: 600; "
        "   color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 6px; "
        "   margin-top: 6px; "
        "   background-color: %3; "
        "} "
        "QGroupBox::title { "
        "   subcontrol-origin: margin; "
        "   subcontrol-position: top left; "
        "   padding: 0 8px; "
        "   background-color: %3; "
        "}")
        .arg(textColor())
        .arg(borderColor())
        .arg(backgroundColor());
}

QString ThemeManager::listWidgetStyleSheet() const
{
    return QString(
        "QListWidget { "
        "   border: 1px solid %1; "
        "   border-radius: 3px; "
        "   background-color: %2; "
        "   font-size: 12px; "
        "   selection-background-color: %3; "
        "   selection-color: #ffffff; "
        "} "
        "QListWidget::item { "
        "   padding: 4px 8px; "
        "   border-bottom: 1px solid %4; "
        "   color: %5; "
        "} "
        "QListWidget::item:hover { "
        "   background-color: %6; "
        "}")
        .arg(borderColor())
        .arg(backgroundColor())
        .arg(selectionBackgroundColor())
        .arg(isDarkMode() ? "#21262d" : "#f6f8fa")
        .arg(textColor())
        .arg(hoverBackgroundColor());
}

QString ThemeManager::progressBarStyleSheet() const
{
    return QString(
        "QProgressBar { "
        "   border: 1px solid %1; "
        "   border-radius: 3px; "
        "   background-color: %2; "
        "   text-align: center; "
        "   font-size: 12px; "
        "   color: %3; "
        "   height: 20px; "
        "} "
        "QProgressBar::chunk { "
        "   background-color: %4; "
        "   border-radius: 2px; "
        "}")
        .arg(borderColor())
        .arg(surfaceColor())
        .arg(textColor())
        .arg(primaryColor());
}

QString ThemeManager::textEditStyleSheet() const
{
    return QString(
        "QTextEdit { "
        "   border: 1px solid %1; "
        "   border-radius: 3px; "
        "   background-color: %2; "
        "   font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
        "   font-size: 11px; "
        "   color: %3; "
        "   padding: 8px; "
        "}")
        .arg(borderColor())
        .arg(backgroundColor())
        .arg(textColor());
}

QString ThemeManager::checkBoxStyleSheet() const
{
    return QString(
        "QCheckBox { "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   color: %1; "
        "} "
        "QCheckBox::indicator { "
        "   width: 16px; "
        "   height: 16px; "
        "} "
        "QCheckBox::indicator:unchecked { "
        "   border: 1px solid %2; "
        "   border-radius: 3px; "
        "   background-color: %3; "
        "} "
        "QCheckBox::indicator:checked { "
        "   border: 1px solid %4; "
        "   border-radius: 3px; "
        "   background-color: %4; "
        "   image: url(data:image/svg+xml;charset=utf-8,%%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%%3E%%3Cpath fill='white' d='M13.78 4.22a.75.75 0 010 1.06l-7.25 7.25a.75.75 0 01-1.06 0L2.22 9.28a.75.75 0 011.06-1.06L6 10.94l6.72-6.72a.75.75 0 011.06 0z'/%%3E%%3C/svg%%3E); "
        "}")
        .arg(textColor())
        .arg(borderColor())
        .arg(backgroundColor())
        .arg(primaryColor());
}

QString ThemeManager::tabWidgetStyleSheet() const
{
    return QString(
        "QMainWindow { "
        "   background-color: %1; "
        "   color: %2; "
        "} "
        "QTabWidget::pane { "
        "   border: 1px solid %3; "
        "   background-color: %1; "
        "} "
        "QTabWidget::tab-bar { "
        "   alignment: left; "
        "} "
        "QTabBar::tab { "
        "   background-color: %4; "
        "   border: 1px solid %3; "
        "   border-bottom: none; "
        "   padding: 8px 16px; "
        "   margin-right: 2px; "
        "   color: %5; "
        "   font-size: 13px; "
        "   font-weight: 500; "
        "} "
        "QTabBar::tab:selected { "
        "   background-color: %1; "
        "   color: %2; "
        "   border-bottom: 1px solid %1; "
        "} "
        "QTabBar::tab:hover:!selected { "
        "   background-color: %6; "
        "} "
        "QWidget { "
        "   background-color: %1; "
        "   color: %2; "
        "}")
        .arg(backgroundColor())
        .arg(textColor())
        .arg(borderColor())
        .arg(surfaceColor())
        .arg(secondaryTextColor())
        .arg(hoverBackgroundColor());
}

QString ThemeManager::sliderStyleSheet() const
{
    return QString(
        "QSlider::groove:horizontal { "
        "   background-color: %1; "
        "   height: 3px; "
        "   border-radius: 1px; "
        "} "
        "QSlider::handle:horizontal { "
        "   background-color: %2; "        
        "   border: 1px solid %2; "
        "   width: 14px; "
        "   height: 14px; "
        "   border-radius: 7px; "
        "   margin-top: -5px; "
        "   margin-bottom: -5px; "
        "} "
        "QSlider::handle:horizontal:hover { "
        "   background-color: %3; "
        "} "
        "QSlider::sub-page:horizontal { "
        "   background-color: %2; "
        "   border-radius: 1px; "
        "} "
        "QSlider:disabled::groove:horizontal { "
        "   background-color: %4; "
        "} "
        "QSlider:disabled::handle:horizontal { "
        "   background-color: %5; "
        "   border-color: %5; "
        "}")
        .arg(borderColor())
        .arg(primaryColor())
        .arg(primaryHoverColor())
        .arg(isDarkMode() ? "#30363d" : "#eaeef2")
        .arg(disabledColor());
}

QString ThemeManager::labelStyleSheet() const
{
    return QString(
        "QLabel { "
        "   color: %1; "
        "}")
        .arg(textColor());
}