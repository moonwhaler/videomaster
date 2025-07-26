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

private:
    void setupUI();
    
    QVBoxLayout *m_layout;
    QLabel *m_titleLabel;
    QVideoWidget *m_videoWidget;
    QMediaPlayer *m_mediaPlayer;
    QPushButton *m_playButton;
    QSlider *m_positionSlider;
    QLabel *m_timeLabel;
    QLabel *m_dropLabel;
    
    QString m_currentFilePath;
};

#endif // VIDEOWIDGET_H