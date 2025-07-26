#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QApplication>
#include <QSettings>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme {
        System,
        Light,
        Dark
    };

    static ThemeManager* instance();
    
    void setTheme(Theme theme);
    Theme currentTheme() const { return m_currentTheme; }
    bool isDarkMode() const;
    
    // Color getters for current theme
    QString backgroundColor() const;
    QString surfaceColor() const;
    QString borderColor() const;
    QString textColor() const;
    QString secondaryTextColor() const;
    QString primaryColor() const;
    QString primaryHoverColor() const;
    QString primaryPressedColor() const;
    QString successColor() const;
    QString successHoverColor() const;
    QString dangerColor() const;
    QString dangerHoverColor() const;
    QString inputBackgroundColor() const;
    QString hoverBackgroundColor() const;
    QString selectionBackgroundColor() const;
    QString disabledColor() const;
    
    // Complete stylesheets for common widgets
    QString buttonStyleSheet() const;
    QString primaryButtonStyleSheet() const;
    QString successButtonStyleSheet() const;
    QString dangerButtonStyleSheet() const;
    QString lineEditStyleSheet() const;
    QString groupBoxStyleSheet() const;
    QString listWidgetStyleSheet() const;
    QString progressBarStyleSheet() const;
    QString textEditStyleSheet() const;
    QString checkBoxStyleSheet() const;
    QString tabWidgetStyleSheet() const;
    QString sliderStyleSheet() const;
    QString labelStyleSheet() const;

signals:
    void themeChanged();

private slots:
    void onSystemThemeChanged();

private:
    explicit ThemeManager(QObject *parent = nullptr);
    void applyTheme();
    bool detectSystemDarkMode() const;
    void saveThemePreference();
    void loadThemePreference();
    
    static ThemeManager* s_instance;
    Theme m_currentTheme;
    bool m_systemIsDark;
};

#endif // THEMEMANAGER_H