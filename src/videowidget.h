#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QLabel>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QFileDialog>

// Custom QVideoWidget that handles drag & drop and click events
class ClickableVideoWidget : public QVideoWidget
{
    Q_OBJECT

public:
    explicit ClickableVideoWidget(QWidget *parent = nullptr);

signals:
    void clicked();
    void fileDropped(const QString &filePath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(const QString &title, QWidget *parent = nullptr);
    
    void loadVideo(const QString &filePath);
    void play();
    void pause();
    void seek(qint64 position);
    qint64 position() const;
    qint64 duration() const;
    QString currentFilePath() const { return m_currentFilePath; }

signals:
    void videoLoaded(const QString &filePath);
    void positionChanged(qint64 position);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onPlayPause();
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onThemeChanged();
    void onVideoWidgetClicked();
    void onVideoWidgetFileDropped(const QString &filePath);
    void onDropLabelClicked();

private:
    void setupUI();
    void applyTheme();
    
    QVBoxLayout *m_layout;
    QLabel *m_titleLabel;
    QStackedWidget *m_stackedWidget;
    ClickableVideoWidget *m_videoWidget;
    QMediaPlayer *m_mediaPlayer;
    QPushButton *m_playButton;
    QSlider *m_positionSlider;
    QLabel *m_timeLabel;
    QPushButton *m_dropLabel;
    
    QString m_currentFilePath;
};

#endif // VIDEOWIDGET_H
