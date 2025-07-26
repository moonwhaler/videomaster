#include "videowidget.h"
#include <QFileInfo>
#include <QTime>
#include <QCoreApplication>

VideoWidget::VideoWidget(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_titleLabel(new QLabel(title, this))
    , m_videoWidget(new QVideoWidget(this))
    , m_mediaPlayer(new QMediaPlayer(this))
    , m_playButton(new QPushButton("Play", this))
    , m_positionSlider(new QSlider(Qt::Horizontal, this))
    , m_timeLabel(new QLabel("00:00 / 00:00", this))
    , m_dropLabel(new QLabel("Drop video file here", this))
{
    setupUI();
    setAcceptDrops(true);
    
    m_mediaPlayer->setVideoOutput(m_videoWidget);
    
    connect(m_playButton, &QPushButton::clicked, this, &VideoWidget::onPlayPause);
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &VideoWidget::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoWidget::onDurationChanged);
    connect(m_positionSlider, &QSlider::sliderMoved, this, &VideoWidget::seek);
}

void VideoWidget::setupUI()
{
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    m_videoWidget->setMinimumSize(320, 240);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_dropLabel->setAlignment(Qt::AlignCenter);
    m_dropLabel->setStyleSheet("border: 2px dashed #aaa; padding: 20px; color: #aaa;");
    m_dropLabel->setMinimumSize(320, 240);
    
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    controlsLayout->addWidget(m_playButton);
    controlsLayout->addWidget(m_positionSlider);
    controlsLayout->addWidget(m_timeLabel);
    
    m_layout->addWidget(m_titleLabel);
    m_layout->addWidget(m_dropLabel);
    m_layout->addWidget(m_videoWidget);
    m_layout->addLayout(controlsLayout);
    
    m_videoWidget->hide();
    m_playButton->setEnabled(false);
    m_positionSlider->setEnabled(false);
}

void VideoWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QUrl url = event->mimeData()->urls().first();
        QString filePath = url.toLocalFile();
        QFileInfo fileInfo(filePath);
        
        QStringList videoExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v"};
        if (videoExtensions.contains(fileInfo.suffix().toLower())) {
            event->acceptProposedAction();
        }
    }
}

void VideoWidget::dropEvent(QDropEvent *event)
{
    QUrl url = event->mimeData()->urls().first();
    QString filePath = url.toLocalFile();
    loadVideo(filePath);
}

void VideoWidget::loadVideo(const QString &filePath)
{
    m_currentFilePath = filePath;
    m_mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
    
    m_dropLabel->hide();
    m_videoWidget->show();
    m_playButton->setEnabled(true);
    m_positionSlider->setEnabled(true);
    
    QFileInfo fileInfo(filePath);
    m_titleLabel->setText(fileInfo.fileName());
    
    // Brief play/pause to ensure media is ready for seeking
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia) {
            // Media is fully loaded and ready for seeking
            m_mediaPlayer->play();
            m_mediaPlayer->pause();
            disconnect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this, nullptr);
        }
    });
    
    emit videoLoaded(filePath);
}

void VideoWidget::play()
{
    m_mediaPlayer->play();
    m_playButton->setText("Pause");
}

void VideoWidget::pause()
{
    m_mediaPlayer->pause();
    m_playButton->setText("Play");
}

void VideoWidget::seek(qint64 position)
{
    // Ensure media player is in a seekable state
    if (m_mediaPlayer->mediaStatus() == QMediaPlayer::NoMedia || 
        m_mediaPlayer->mediaStatus() == QMediaPlayer::InvalidMedia) {
        return;
    }
    
    // If the player is stopped, we need to prepare it for seeking
    if (m_mediaPlayer->playbackState() == QMediaPlayer::StoppedState) {
        // Temporarily start playback to enable seeking, then immediately pause
        m_mediaPlayer->play();
        QCoreApplication::processEvents(); // Allow the play state to be processed
        m_mediaPlayer->pause();
    }
    
    m_mediaPlayer->setPosition(position);
}

qint64 VideoWidget::position() const
{
    return m_mediaPlayer->position();
}

qint64 VideoWidget::duration() const
{
    return m_mediaPlayer->duration();
}

void VideoWidget::onPlayPause()
{
    if (m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) {
        pause();
    } else {
        play();
    }
}

void VideoWidget::onPositionChanged(qint64 position)
{
    m_positionSlider->setValue(position);
    
    QTime currentTime = QTime::fromMSecsSinceStartOfDay(position);
    QTime totalTime = QTime::fromMSecsSinceStartOfDay(m_mediaPlayer->duration());
    
    m_timeLabel->setText(QString("%1 / %2")
                        .arg(currentTime.toString("mm:ss"))
                        .arg(totalTime.toString("mm:ss")));
    
    emit positionChanged(position);
}

void VideoWidget::onDurationChanged(qint64 duration)
{
    m_positionSlider->setRange(0, duration);
}