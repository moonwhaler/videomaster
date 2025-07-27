#include "videowidget.h"
#include "thememanager.h"
#include <QFileInfo>
#include <QTime>
#include <QCoreApplication>
#include <QTimer>
#include <QPainter>
#include <QFont>

// VideoOverlay implementation - transparent widget that handles drag & drop
VideoOverlay::VideoOverlay(QWidget *parent)
    : QWidget(parent)
    , m_isDragActive(false)
    , m_isValidDrag(false)
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_AcceptDrops, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, false); // We want to catch mouse events
    setStyleSheet("background: transparent;"); // Make it transparent
}

void VideoOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void VideoOverlay::dragEnterEvent(QDragEnterEvent *event)
{
    m_isDragActive = true;
    m_isValidDrag = false;
    
    if (event->mimeData()->hasUrls()) {
        QUrl url = event->mimeData()->urls().first();
        QString filePath = url.toLocalFile();
        QFileInfo fileInfo(filePath);
        
        QStringList videoExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v"};
        if (videoExtensions.contains(fileInfo.suffix().toLower())) {
            m_isValidDrag = true;
            event->acceptProposedAction();
        }
    }
    
    update(); // Trigger repaint to show visual feedback
}

void VideoOverlay::dragMoveEvent(QDragMoveEvent *event)
{
    // Continue accepting if it's a valid drag
    if (m_isValidDrag && event->mimeData()->hasUrls()) {
        QUrl url = event->mimeData()->urls().first();
        QString filePath = url.toLocalFile();
        QFileInfo fileInfo(filePath);
        
        QStringList videoExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v"};
        if (videoExtensions.contains(fileInfo.suffix().toLower())) {
            event->acceptProposedAction();
        }
    }
}

void VideoOverlay::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_isDragActive = false;
    m_isValidDrag = false;
    update(); // Trigger repaint to hide visual feedback
    QWidget::dragLeaveEvent(event);
}

void VideoOverlay::dropEvent(QDropEvent *event)
{
    m_isDragActive = false;
    m_isValidDrag = false;
    update(); // Trigger repaint to hide visual feedback
    
    QUrl url = event->mimeData()->urls().first();
    QString filePath = url.toLocalFile();
    emit fileDropped(filePath);
}

void VideoOverlay::paintEvent(QPaintEvent *event)
{
    if (m_isDragActive) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Get theme colors
        ThemeManager* theme = ThemeManager::instance();
        
        // Semi-transparent overlay
        QColor overlayColor;
        QColor borderColor;
        if (m_isValidDrag) {
            QColor successColor = QColor(theme->successColor());
            overlayColor = QColor(successColor.red(), successColor.green(), successColor.blue(), 80);
            borderColor = QColor(successColor.red(), successColor.green(), successColor.blue(), 180);
        } else {
            QColor dangerColor = QColor(theme->dangerColor());
            overlayColor = QColor(dangerColor.red(), dangerColor.green(), dangerColor.blue(), 80);
            borderColor = QColor(dangerColor.red(), dangerColor.green(), dangerColor.blue(), 180);
        }
        
        painter.fillRect(rect(), overlayColor);
        
        // Dashed border
        QPen borderPen;
        borderPen.setColor(borderColor);
        borderPen.setWidth(3);
        borderPen.setStyle(Qt::DashLine);
        painter.setPen(borderPen);
        painter.drawRect(rect().adjusted(2, 2, -2, -2));
        
        // Text in center
        painter.setPen(QColor(theme->textColor())); // Use theme text color
        painter.setFont(QFont("Arial", 14, QFont::Bold));
        
        QString message;
        if (m_isValidDrag) {
            message = "Drop video file here";
        } else {
            message = "Invalid file type";
        }
        
        painter.drawText(rect(), Qt::AlignCenter, message);
    }
    
    Q_UNUSED(event);
}

VideoWidget::VideoWidget(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_titleLabel(new QLabel(title, this))
    , m_stackedWidget(new QStackedWidget(this))
    , m_videoWidget(new QVideoWidget(this))
    , m_overlay(new VideoOverlay(this))
    , m_videoContainer(nullptr)
    , m_mediaPlayer(new QMediaPlayer(this))
    , m_playButton(new QPushButton("Play", this))
    , m_positionSlider(new QSlider(Qt::Horizontal, this))
    , m_timeLabel(new QLabel("00:00 / 00:00", this))
    , m_dropLabel(new QPushButton("Drop video file here or click to select", this))
{
    setupUI();
    setAcceptDrops(true);
    
    m_mediaPlayer->setVideoOutput(m_videoWidget);
    
    // Connect to theme manager
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &VideoWidget::onThemeChanged);
    
    connect(m_playButton, &QPushButton::clicked, this, &VideoWidget::onPlayPause);
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &VideoWidget::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoWidget::onDurationChanged);
    connect(m_positionSlider, &QSlider::sliderMoved, this, &VideoWidget::seek);
    
    // Connect VideoOverlay signals
    connect(m_overlay, &VideoOverlay::clicked, this, &VideoWidget::onOverlayClicked);
    connect(m_overlay, &VideoOverlay::fileDropped, this, &VideoWidget::onOverlayFileDropped);
    
    // Connect drop label click
    connect(m_dropLabel, &QPushButton::clicked, this, &VideoWidget::onDropLabelClicked);
    
    // Install event filter on container to handle resize events
    m_videoContainer->installEventFilter(this);
}

void VideoWidget::setupUI()
{
    m_titleLabel->setAlignment(Qt::AlignCenter);
    
    m_videoWidget->setMinimumSize(300, 200);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    m_dropLabel->setMinimumSize(300, 200);
    
    m_timeLabel->setAlignment(Qt::AlignCenter);
    
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(0, 4, 0, 0);
    controlsLayout->setSpacing(6);
    controlsLayout->addWidget(m_playButton);
    controlsLayout->addWidget(m_positionSlider, 1);
    controlsLayout->addWidget(m_timeLabel);
    
    // Create a container widget for video widget and overlay
    m_videoContainer = new QWidget();
    m_videoContainer->setMinimumSize(300, 200);
    
    // Set up the stacked widget to hold either drop label or video container
    m_stackedWidget->addWidget(m_dropLabel);
    m_stackedWidget->addWidget(m_videoContainer);
    m_stackedWidget->setCurrentWidget(m_dropLabel);
    
    // Position video widget and overlay in the container
    m_videoWidget->setParent(m_videoContainer);
    m_overlay->setParent(m_videoContainer);
    
    // Make overlay cover the entire video widget
    m_videoWidget->setGeometry(0, 0, 300, 200);
    m_overlay->setGeometry(0, 0, 300, 200);
    
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);
    m_layout->addWidget(m_titleLabel);
    m_layout->addWidget(m_stackedWidget, 1);
    m_layout->addLayout(controlsLayout);
    
    m_playButton->setEnabled(false);
    m_positionSlider->setEnabled(false);
    
    // Apply initial theme
    applyTheme();
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
    
    // Switch to video container in stacked widget
    m_stackedWidget->setCurrentWidget(m_videoContainer);
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

void VideoWidget::applyTheme()
{
    ThemeManager* theme = ThemeManager::instance();
    
    // Apply theme to title label
    m_titleLabel->setStyleSheet(QString(
        "font-size: 12px; font-weight: 500; color: %1; padding: 2px;"
    ).arg(theme->secondaryTextColor()));
    
    // Apply theme to video widget
    m_videoWidget->setStyleSheet(QString(
        "background-color: #000000; border: 1px solid %1; border-radius: 3px;"
    ).arg(theme->borderColor()));
    
    // Apply theme to drop label (now a button)
    m_dropLabel->setStyleSheet(QString(
        "QPushButton { "
        "   border: 2px dashed %1; "
        "   padding: 40px 20px; "
        "   color: %2; "
        "   font-size: 13px; "
        "   background-color: %3; "
        "   border-radius: 3px; "
        "   text-align: center; "
        "} "
        "QPushButton:hover { "
        "   background-color: %4; "
        "   border-color: %5; "
        "} "
        "QPushButton:pressed { "
        "   background-color: %6; "
        "}"
    ).arg(theme->borderColor())
     .arg(theme->secondaryTextColor())
     .arg(theme->surfaceColor())
     .arg(theme->backgroundColor())
     .arg(theme->primaryColor())
     .arg(theme->borderColor()));
    
    // Apply theme to controls
    m_playButton->setStyleSheet(theme->buttonStyleSheet());
    m_positionSlider->setStyleSheet(theme->sliderStyleSheet());
    
    // Apply theme to time label with monospace font
    m_timeLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
        "   font-size: 11px; "
        "   color: %1; "
        "   background-color: %2; "
        "   border: 1px solid %3; "
        "   border-radius: 3px; "
        "   padding: 3px 6px; "
        "   min-width: 70px; "
        "}"
    ).arg(theme->textColor())
     .arg(theme->backgroundColor())
     .arg(theme->borderColor()));
}

void VideoWidget::onThemeChanged()
{
    applyTheme();
}

void VideoWidget::onOverlayClicked()
{
    // Open file dialog when overlay is clicked
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Video File",
        "",
        "Video Files (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm *.m4v);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        loadVideo(fileName);
    }
}

void VideoWidget::onOverlayFileDropped(const QString &filePath)
{
    // Handle file dropped on overlay
    loadVideo(filePath);
}

void VideoWidget::onDropLabelClicked()
{
    // Open file dialog when drop label is clicked (same functionality as overlay click)
    onOverlayClicked();
}

bool VideoWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_videoContainer && event->type() == QEvent::Resize) {
        // Handle container resize - keep overlay positioned over video widget
        if (m_videoWidget && m_overlay) {
            QRect containerRect = m_videoContainer->rect();
            m_videoWidget->setGeometry(containerRect);
            m_overlay->setGeometry(containerRect);
        }
    } else if (obj == m_videoWidget) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent*>(event);
            
            if (dragEvent->mimeData()->hasUrls()) {
                QUrl url = dragEvent->mimeData()->urls().first();
                QString filePath = url.toLocalFile();
                QFileInfo fileInfo(filePath);
                
                QStringList videoExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v"};
                if (videoExtensions.contains(fileInfo.suffix().toLower())) {
                    dragEvent->acceptProposedAction();
                    return true; // Event handled
                }
            }
        } else if (event->type() == QEvent::Drop) {
            QDropEvent *dropEvent = static_cast<QDropEvent*>(event);
            
            if (dropEvent->mimeData()->hasUrls()) {
                QUrl url = dropEvent->mimeData()->urls().first();
                QString filePath = url.toLocalFile();
                loadVideo(filePath);
                return true; // Event handled
            }
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dragMoveEvent = static_cast<QDragMoveEvent*>(event);
            
            if (dragMoveEvent->mimeData()->hasUrls()) {
                QUrl url = dragMoveEvent->mimeData()->urls().first();
                QString filePath = url.toLocalFile();
                QFileInfo fileInfo(filePath);
                
                QStringList videoExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v"};
                if (videoExtensions.contains(fileInfo.suffix().toLower())) {
                    dragMoveEvent->acceptProposedAction();
                    return true; // Event handled
                }
            }
        }
    }
    
    // Standard event processing
    return QWidget::eventFilter(obj, event);
}

