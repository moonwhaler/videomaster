#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <QLabel>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QFileDialog>
#include <QEvent>
#include <QPaintEvent>

// Transparent overlay widget that handles drag & drop and click events
class VideoOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit VideoOverlay(QWidget *parent = nullptr);

signals:
    void clicked();
    void fileDropped(const QString &filePath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_isDragActive;
    bool m_isValidDrag;
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
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onPlayPause();
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onThemeChanged();
    void onOverlayClicked();
    void onOverlayFileDropped(const QString &filePath);
    void onDropLabelClicked();

private:
    void setupUI();
    void applyTheme();
    
    QVBoxLayout *m_layout;
    QLabel *m_titleLabel;
    QStackedWidget *m_stackedWidget;
    QVideoWidget *m_videoWidget;
    VideoOverlay *m_overlay;
    QWidget *m_videoContainer;
    QMediaPlayer *m_mediaPlayer;
    QPushButton *m_playButton;
    QSlider *m_positionSlider;
    QLabel *m_timeLabel;
    QPushButton *m_dropLabel;
    
    QString m_currentFilePath;
};

#endif // VIDEOWIDGET_H
