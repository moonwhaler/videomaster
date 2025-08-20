#include "mainwindow.h"
#include "videowidget.h"
#include "videocomparator.h"
#include "batchprocessor.h"
#include "transferworker.h"
#include "ffmpeghandler.h"
#include "thememanager.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QPixmap>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_comparator(new VideoComparator(this))
    , m_isPlaying(false)
    , m_currentChapterIndex(-1)
    , m_transferThread(nullptr)
    , m_transferWorker(nullptr)
{
    // Connect to theme manager
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
    
    setupMenuBar();
    setupUI();
    applyTheme();
    
    setCentralWidget(m_tabWidget);
    setWindowTitle("VideoMaster - Video Comparison & Track Transfer Tool");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    // Clean up transfer thread
    if (m_transferThread) {
        m_transferThread->quit();
        m_transferThread->wait(3000);
        delete m_transferWorker;
        delete m_transferThread;
    }
}

void MainWindow::setupUI()
{
    setupComparisonTab();
    setupTransferTab();
    setupBatchTab();
    
    m_tabWidget->addTab(m_comparisonTab, "Video Comparison");
    m_tabWidget->addTab(m_transferTab, "Track Transfer");
    m_tabWidget->addTab(m_batchTab, "Batch Processing");
}

void MainWindow::setupComparisonTab()
{
    ThemeManager* theme = ThemeManager::instance();
    
    m_comparisonTab = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(m_comparisonTab);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);
    
    // Video widgets in horizontal splitter - clean and simple
    QSplitter *videoSplitter = new QSplitter(Qt::Horizontal);
    videoSplitter->setChildrenCollapsible(false);
    
    // Left video - minimal styling
    QWidget *leftContainer = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->setSpacing(4);
    
    QLabel *leftLabel = new QLabel("Video A");
    leftLabel->setStyleSheet(QString("font-size: 12px; font-weight: 500; color: %1; padding: 2px;").arg(theme->secondaryTextColor()));
    
    m_leftVideoWidget = new VideoWidget("Drop video file here", this);
    m_leftVideoWidget->setMinimumHeight(250);
    
    leftLayout->addWidget(leftLabel);
    leftLayout->addWidget(m_leftVideoWidget, 1);
    
    // Right video - minimal styling
    QWidget *rightContainer = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(4);
    
    QLabel *rightLabel = new QLabel("Video B");
    rightLabel->setStyleSheet(QString("font-size: 12px; font-weight: 500; color: %1; padding: 2px;").arg(theme->secondaryTextColor()));
    
    m_rightVideoWidget = new VideoWidget("Drop video file here", this);
    m_rightVideoWidget->setMinimumHeight(250);
    
    rightLayout->addWidget(rightLabel);
    rightLayout->addWidget(m_rightVideoWidget, 1);
    
    videoSplitter->addWidget(leftContainer);
    videoSplitter->addWidget(rightContainer);
    videoSplitter->setSizes({1, 1}); // Equal sizing
    
    // Control panel - clean business style
    QWidget *controlPanel = new QWidget();
    controlPanel->setStyleSheet(QString(
        "QWidget { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 4px; "
        "}"
    ).arg(theme->surfaceColor()).arg(theme->borderColor()));
    QHBoxLayout *controlLayout = new QHBoxLayout(controlPanel);
    controlLayout->setContentsMargins(12, 8, 12, 8);
    controlLayout->setSpacing(12);
    
    // Sync button - standard business button
    m_syncButton = new QPushButton("Sync Playback", this);
    m_syncButton->setMinimumWidth(100);
    m_syncButton->setStyleSheet(theme->primaryButtonStyleSheet());
    
    // Auto compare button
    m_autoCompareButton = new QPushButton("Auto Compare", this);
    m_autoCompareButton->setMinimumWidth(100);
    m_autoCompareButton->setStyleSheet(theme->successButtonStyleSheet());
    
    // Auto offset button
    m_autoOffsetButton = new QPushButton("Auto Offset", this);
    m_autoOffsetButton->setMinimumWidth(100);
    m_autoOffsetButton->setStyleSheet(theme->buttonStyleSheet());
    m_autoOffsetButton->setToolTip("Automatically detect the optimal time offset between videos");
    
    // Relative offset control
    QLabel *offsetLabel = new QLabel("Offset A→B:");
    offsetLabel->setStyleSheet(QString("font-size: 13px; color: %1; font-weight: 500;").arg(theme->secondaryTextColor()));
    offsetLabel->setToolTip("Positive: Video A starts later than Video B\nNegative: Video A starts earlier than Video B");
    
    m_relativeOffsetSpinBox = new QSpinBox();
    m_relativeOffsetSpinBox->setRange(-999999, 999999);
    m_relativeOffsetSpinBox->setValue(0);
    m_relativeOffsetSpinBox->setSuffix(" ms");
    m_relativeOffsetSpinBox->setMinimumWidth(80);
    m_relativeOffsetSpinBox->setStyleSheet(theme->lineEditStyleSheet());
    m_relativeOffsetSpinBox->setToolTip("Positive: Video A starts later than Video B\nNegative: Video A starts earlier than Video B");
    
    // Timeline label
    QLabel *timelineLabel = new QLabel("Timeline:");
    timelineLabel->setStyleSheet(QString("font-size: 13px; color: %1; font-weight: 500;").arg(theme->secondaryTextColor()));
    
    // Timeline slider - clean business style
    m_timestampSlider = new QSlider(Qt::Horizontal, this);
    m_timestampSlider->setEnabled(false);
    m_timestampSlider->setMinimumWidth(200);
    m_timestampSlider->setStyleSheet(theme->sliderStyleSheet());
    
    // Timestamp display
    m_timestampLabel = new QLabel("00:00");
    m_timestampLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
        "   font-size: 13px; "
        "   color: %1; "
        "   background-color: %2; "
        "   border: 1px solid %3; "
        "   border-radius: 3px; "
        "   padding: 4px 8px; "
        "   min-width: 45px; "
        "}"
    ).arg(theme->textColor()).arg(theme->backgroundColor()).arg(theme->borderColor()));
    m_timestampLabel->setAlignment(Qt::AlignCenter);
    
    controlLayout->addWidget(m_syncButton);
    controlLayout->addWidget(m_autoCompareButton);
    controlLayout->addWidget(m_autoOffsetButton);
    controlLayout->addWidget(offsetLabel);
    controlLayout->addWidget(m_relativeOffsetSpinBox);
    controlLayout->addWidget(timelineLabel);
    controlLayout->addWidget(m_timestampSlider, 1);
    controlLayout->addWidget(m_timestampLabel);
    
    // Auto comparison results area
    QWidget *resultsPanel = new QWidget();
    resultsPanel->setStyleSheet(QString(
        "QWidget { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 4px; "
        "}"
    ).arg(theme->surfaceColor()).arg(theme->borderColor()));
    QVBoxLayout *resultsLayout = new QVBoxLayout(resultsPanel);
    resultsLayout->setContentsMargins(12, 8, 12, 8);
    resultsLayout->setSpacing(6);
    
    // Progress bar for auto comparison
    m_comparisonProgressBar = new QProgressBar(this);
    m_comparisonProgressBar->setVisible(false);
    m_comparisonProgressBar->setStyleSheet(theme->progressBarStyleSheet());
    
    // Result label
    m_comparisonResultLabel = new QLabel("Load both videos and click 'Auto Compare' to analyze similarity", this);
    m_comparisonResultLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-size: 12px; "
        "   color: %1; "
        "   padding: 4px; "
        "   background-color: transparent; "
        "   border: none; "
        "}"
    ).arg(theme->secondaryTextColor()));
    m_comparisonResultLabel->setWordWrap(true);
    m_comparisonResultLabel->setAlignment(Qt::AlignCenter);
    
    resultsLayout->addWidget(m_comparisonProgressBar);
    resultsLayout->addWidget(m_comparisonResultLabel);
    
    // Chapter navigation area
    QWidget *chapterPanel = new QWidget();
    chapterPanel->setStyleSheet(QString(
        "QWidget { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 4px; "
        "}"
    ).arg(theme->surfaceColor()).arg(theme->borderColor()));
    QHBoxLayout *chapterLayout = new QHBoxLayout(chapterPanel);
    chapterLayout->setContentsMargins(12, 8, 12, 8);
    chapterLayout->setSpacing(12);
    
    // Left video chapters
    QVBoxLayout *leftChapterLayout = new QVBoxLayout();
    QLabel *leftChapterLabel = new QLabel("Video A Chapters");
    leftChapterLabel->setStyleSheet(QString("font-size: 12px; font-weight: 500; color: %1;").arg(theme->textColor()));
    leftChapterLabel->setAlignment(Qt::AlignCenter);
    
    m_leftChaptersList = new QListWidget();
    m_leftChaptersList->setMaximumHeight(120);
    m_leftChaptersList->setStyleSheet(theme->listWidgetStyleSheet());
    
    leftChapterLayout->addWidget(leftChapterLabel);
    leftChapterLayout->addWidget(m_leftChaptersList);
    
    // Chapter navigation controls
    QVBoxLayout *chapterControlsLayout = new QVBoxLayout();
    
    m_currentChapterLabel = new QLabel("No chapters");
    m_currentChapterLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-size: 12px; "
        "   color: %1; "
        "   padding: 4px; "
        "   background-color: transparent; "
        "   border: none; "
        "   font-weight: 500; "
        "}"
    ).arg(theme->textColor()));
    m_currentChapterLabel->setAlignment(Qt::AlignCenter);
    m_currentChapterLabel->setWordWrap(true);
    
    QHBoxLayout *navButtonsLayout = new QHBoxLayout();
    m_prevChapterButton = new QPushButton("◄ Prev", this);
    m_nextChapterButton = new QPushButton("Next ►", this);
    
    m_prevChapterButton->setEnabled(false);
    m_nextChapterButton->setEnabled(false);
    m_prevChapterButton->setStyleSheet(theme->buttonStyleSheet());
    m_nextChapterButton->setStyleSheet(theme->buttonStyleSheet());
    
    navButtonsLayout->addWidget(m_prevChapterButton);
    navButtonsLayout->addWidget(m_nextChapterButton);
    
    chapterControlsLayout->addWidget(m_currentChapterLabel);
    chapterControlsLayout->addLayout(navButtonsLayout);
    chapterControlsLayout->addStretch();
    
    // Right video chapters
    QVBoxLayout *rightChapterLayout = new QVBoxLayout();
    QLabel *rightChapterLabel = new QLabel("Video B Chapters");
    rightChapterLabel->setStyleSheet(QString("font-size: 12px; font-weight: 500; color: %1;").arg(theme->textColor()));
    rightChapterLabel->setAlignment(Qt::AlignCenter);
    
    m_rightChaptersList = new QListWidget();
    m_rightChaptersList->setMaximumHeight(120);
    m_rightChaptersList->setStyleSheet(theme->listWidgetStyleSheet());
    
    rightChapterLayout->addWidget(rightChapterLabel);
    rightChapterLayout->addWidget(m_rightChaptersList);
    
    chapterLayout->addLayout(leftChapterLayout, 1);
    chapterLayout->addLayout(chapterControlsLayout);
    chapterLayout->addLayout(rightChapterLayout, 1);
    
    // Assemble main layout
    mainLayout->addWidget(videoSplitter, 1);
    mainLayout->addWidget(controlPanel);
    mainLayout->addWidget(chapterPanel);
    mainLayout->addWidget(resultsPanel);
    
    // Connect signals
    connect(m_leftVideoWidget, &VideoWidget::videoLoaded, 
            [this](const QString &path) { onVideoLoaded(0, path); });
    connect(m_rightVideoWidget, &VideoWidget::videoLoaded, 
            [this](const QString &path) { onVideoLoaded(1, path); });
    connect(m_syncButton, &QPushButton::clicked, this, &MainWindow::onSyncPlayback);
    connect(m_autoCompareButton, &QPushButton::clicked, this, &MainWindow::onAutoCompare);
    connect(m_autoOffsetButton, &QPushButton::clicked, this, &MainWindow::onAutoOffset);
    connect(m_timestampSlider, &QSlider::sliderMoved, this, &MainWindow::onSeekToTimestamp);
    connect(m_timestampSlider, &QSlider::sliderPressed, this, &MainWindow::onSeekToTimestamp);
    
    // Connect position changes from video widgets to update slider and sync
    connect(m_leftVideoWidget, &VideoWidget::positionChanged, this, &MainWindow::onVideoPositionChanged);
    connect(m_rightVideoWidget, &VideoWidget::positionChanged, this, &MainWindow::onVideoPositionChanged);
    
    // Connect video comparator signals for auto comparison
    connect(m_comparator, &VideoComparator::comparisonProgress, this, &MainWindow::onComparisonProgress);
    connect(m_comparator, &VideoComparator::autoComparisonComplete, this, &MainWindow::onAutoComparisonComplete);
    connect(m_comparator, &VideoComparator::optimalOffsetFound, this, &MainWindow::onOptimalOffsetFound);
    
    // Connect chapter navigation signals
    connect(m_leftChaptersList, &QListWidget::itemClicked, this, &MainWindow::onChapterSelected);
    connect(m_rightChaptersList, &QListWidget::itemClicked, this, &MainWindow::onChapterSelected);
    connect(m_prevChapterButton, &QPushButton::clicked, this, &MainWindow::onPreviousChapter);
    
    // Connect relative offset control
    connect(m_relativeOffsetSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int value) { 
                // Positive offset means Video A starts later (is delayed) relative to Video B
                // So Video A gets positive offset, Video B gets 0
                m_comparator->setVideoOffset(0, value);   // Video A offset
                m_comparator->setVideoOffset(1, 0);       // Video B baseline (no offset)
                // Clear previous comparison results when offset changes
                if (value == 0) {
                    m_comparisonResultLabel->setText("Videos synchronized. Click 'Auto Compare' to analyze similarity.");
                } else {
                    QString direction = (value > 0) ? "later" : "earlier";
                    m_comparisonResultLabel->setText(QString("Video A starts %1ms %2 than Video B. Click 'Auto Compare' to re-analyze.").arg(qAbs(value)).arg(direction));
                }
            });
    connect(m_nextChapterButton, &QPushButton::clicked, this, &MainWindow::onNextChapter);
}

void MainWindow::setupTransferTab()
{
    ThemeManager* theme = ThemeManager::instance();
    
    m_transferTab = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(m_transferTab);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);
    
    // Video selection area
    QHBoxLayout *videoLayout = new QHBoxLayout();
    videoLayout->setSpacing(12);
    
    // Source video section
    QWidget *sourceContainer = new QWidget();
    sourceContainer->setStyleSheet(QString(
        "QWidget { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 6px; "
        "   padding: 8px; "
        "}"
    ).arg(theme->surfaceColor()).arg(theme->borderColor()));
    QVBoxLayout *sourceLayout = new QVBoxLayout(sourceContainer);
    sourceLayout->setContentsMargins(4, 4, 4, 4);
    sourceLayout->setSpacing(4);
    
    QLabel *sourceLabel = new QLabel("Source Video (tracks to copy from)");
    sourceLabel->setStyleSheet(QString("font-size: 13px; font-weight: 500; color: %1;").arg(theme->textColor()));
    
    m_sourceVideoWidget = new VideoWidget("Drop source video here", this);
    
    sourceLayout->addWidget(sourceLabel);
    sourceLayout->addWidget(m_sourceVideoWidget, 1);
    
    // Target video section  
    QWidget *targetContainer = new QWidget();
    targetContainer->setStyleSheet(QString(
        "QWidget { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 6px; "
        "   padding: 8px; "
        "}"
    ).arg(theme->surfaceColor()).arg(theme->borderColor()));
    QVBoxLayout *targetLayout = new QVBoxLayout(targetContainer);
    targetLayout->setContentsMargins(4, 4, 4, 4);
    targetLayout->setSpacing(4);
    
    QLabel *targetLabel = new QLabel("Target Video (base video to merge with)");
    targetLabel->setStyleSheet(QString("font-size: 13px; font-weight: 500; color: %1;").arg(theme->textColor()));
    
    m_targetVideoWidget = new VideoWidget("Drop target video here", this);
    
    targetLayout->addWidget(targetLabel);
    targetLayout->addWidget(m_targetVideoWidget, 1);
    
    videoLayout->addWidget(sourceContainer);
    videoLayout->addWidget(targetContainer);
    
    // Track selection area
    QHBoxLayout *tracksLayout = new QHBoxLayout();
    tracksLayout->setSpacing(12);
    
    // Audio tracks section
    QGroupBox *audioGroup = new QGroupBox("Audio Tracks");
    audioGroup->setStyleSheet(theme->groupBoxStyleSheet());
    QVBoxLayout *audioLayout = new QVBoxLayout(audioGroup);
    
    // Audio template controls
    QHBoxLayout *audioTemplateLayout = new QHBoxLayout();
    QLabel *audioTemplateLabel = new QLabel("Template:");
    audioTemplateLabel->setStyleSheet(QString("font-size: 12px; color: %1;").arg(theme->secondaryTextColor()));
    
    m_audioTemplateEdit = new QLineEdit("*eng*", this);
    m_audioTemplateEdit->setPlaceholderText("e.g., *eng*, *jpn*, *ac3*");
    m_audioTemplateEdit->setStyleSheet(theme->lineEditStyleSheet());
    
    m_applyAudioTemplateButton = new QPushButton("Apply", this);
    m_selectAllAudioButton = new QPushButton("All", this);
    m_clearAudioButton = new QPushButton("Clear", this);
    
    m_applyAudioTemplateButton->setStyleSheet(theme->buttonStyleSheet());
    m_selectAllAudioButton->setStyleSheet(theme->buttonStyleSheet());
    m_clearAudioButton->setStyleSheet(theme->buttonStyleSheet());
    
    audioTemplateLayout->addWidget(audioTemplateLabel);
    audioTemplateLayout->addWidget(m_audioTemplateEdit, 1);
    audioTemplateLayout->addWidget(m_applyAudioTemplateButton);
    audioTemplateLayout->addWidget(m_selectAllAudioButton);
    audioTemplateLayout->addWidget(m_clearAudioButton);
    
    m_audioTracksList = new QListWidget();
    m_audioTracksList->setStyleSheet(theme->listWidgetStyleSheet());
    
    audioLayout->addLayout(audioTemplateLayout);
    audioLayout->addWidget(m_audioTracksList);
    
    // Subtitle tracks section
    QGroupBox *subtitleGroup = new QGroupBox("Subtitle Tracks");
    subtitleGroup->setStyleSheet(theme->groupBoxStyleSheet());
    QVBoxLayout *subtitleLayout = new QVBoxLayout(subtitleGroup);
    
    // Subtitle template controls
    QHBoxLayout *subtitleTemplateLayout = new QHBoxLayout();
    QLabel *subtitleTemplateLabel = new QLabel("Template:");
    subtitleTemplateLabel->setStyleSheet(QString("font-size: 12px; color: %1;").arg(theme->secondaryTextColor()));
    
    m_subtitleTemplateEdit = new QLineEdit("*eng*", this);
    m_subtitleTemplateEdit->setPlaceholderText("e.g., *eng*, *jpn*, *srt*");
    m_subtitleTemplateEdit->setStyleSheet(theme->lineEditStyleSheet());
    
    m_applySubtitleTemplateButton = new QPushButton("Apply", this);
    m_selectAllSubtitleButton = new QPushButton("All", this);
    m_clearSubtitleButton = new QPushButton("Clear", this);
    
    m_applySubtitleTemplateButton->setStyleSheet(theme->buttonStyleSheet());
    m_selectAllSubtitleButton->setStyleSheet(theme->buttonStyleSheet());
    m_clearSubtitleButton->setStyleSheet(theme->buttonStyleSheet());
    
    subtitleTemplateLayout->addWidget(subtitleTemplateLabel);
    subtitleTemplateLayout->addWidget(m_subtitleTemplateEdit, 1);
    subtitleTemplateLayout->addWidget(m_applySubtitleTemplateButton);
    subtitleTemplateLayout->addWidget(m_selectAllSubtitleButton);
    subtitleTemplateLayout->addWidget(m_clearSubtitleButton);
    
    m_subtitleTracksList = new QListWidget();
    m_subtitleTracksList->setStyleSheet(theme->listWidgetStyleSheet());
    
    subtitleLayout->addLayout(subtitleTemplateLayout);
    subtitleLayout->addWidget(m_subtitleTracksList);
    
    tracksLayout->addWidget(audioGroup);
    tracksLayout->addWidget(subtitleGroup);
    
    // Transfer controls
    QWidget *transferPanel = new QWidget();
    transferPanel->setStyleSheet(QString(
        "QWidget { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 6px; "
        "}"
    ).arg(theme->surfaceColor()).arg(theme->borderColor()));
    QHBoxLayout *transferLayout = new QHBoxLayout(transferPanel);
    transferLayout->setContentsMargins(12, 8, 12, 8);
    
    QLabel *postfixLabel = new QLabel("Output Postfix:");
    postfixLabel->setStyleSheet(QString("font-size: 13px; color: %1; font-weight: 500;").arg(theme->secondaryTextColor()));
    
    m_postfixEdit = new QLineEdit("_merged", this);
    m_postfixEdit->setMinimumWidth(100);
    m_postfixEdit->setStyleSheet(theme->lineEditStyleSheet());
    
    m_transferButton = new QPushButton("Transfer Selected Tracks", this);
    m_transferButton->setStyleSheet(theme->primaryButtonStyleSheet());
    
    transferLayout->addWidget(postfixLabel);
    transferLayout->addWidget(m_postfixEdit);
    transferLayout->addStretch();
    transferLayout->addWidget(m_transferButton);
    
    // Assemble main layout
    mainLayout->addLayout(videoLayout, 1);
    mainLayout->addLayout(tracksLayout, 1);
    mainLayout->addWidget(transferPanel);
    
    // Connect signals
    connect(m_transferButton, &QPushButton::clicked, this, &MainWindow::onTransferTracks);
    connect(m_postfixEdit, &QLineEdit::textChanged, this, &MainWindow::onPostfixChanged);
    
    // Template controls
    connect(m_applyAudioTemplateButton, &QPushButton::clicked, this, &MainWindow::onApplyAudioTemplate);
    connect(m_applySubtitleTemplateButton, &QPushButton::clicked, this, &MainWindow::onApplySubtitleTemplate);
    connect(m_selectAllAudioButton, &QPushButton::clicked, this, &MainWindow::onSelectAllAudio);
    connect(m_selectAllSubtitleButton, &QPushButton::clicked, this, &MainWindow::onSelectAllSubtitles);
    connect(m_clearAudioButton, &QPushButton::clicked, this, &MainWindow::onClearAudioSelection);
    connect(m_clearSubtitleButton, &QPushButton::clicked, this, &MainWindow::onClearSubtitleSelection);
    
    // Connect video loading events
    connect(m_sourceVideoWidget, &VideoWidget::videoLoaded, this, [this](const QString &path) {
        updateTrackLists();
    });
    connect(m_targetVideoWidget, &VideoWidget::videoLoaded, this, [this](const QString &path) {
        updateTrackLists();
    });
}

void MainWindow::setupBatchTab()
{
    m_batchTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_batchTab);
    
    m_batchProcessor = new BatchProcessor(this);
    layout->addWidget(m_batchProcessor);
    
    connect(m_batchProcessor, &BatchProcessor::batchProcessRequested, 
            this, &MainWindow::onBatchProcess);
}

void MainWindow::onVideoLoaded(int playerIndex, const QString &filePath)
{
    m_comparator->setVideo(playerIndex, filePath);
    
    // Update slider range when either video is loaded
    qint64 leftDuration = m_leftVideoWidget->duration();
    qint64 rightDuration = m_rightVideoWidget->duration();
    
    if (leftDuration > 0 && rightDuration > 0) {
        // Use the shorter duration to prevent seeking beyond the end of either video
        qint64 maxDuration = qMin(leftDuration, rightDuration);
        m_timestampSlider->setRange(0, maxDuration);
        m_timestampSlider->setEnabled(true);
    } else if (leftDuration > 0 || rightDuration > 0) {
        // At least one video is loaded
        qint64 duration = qMax(leftDuration, rightDuration);
        m_timestampSlider->setRange(0, duration);
        m_timestampSlider->setEnabled(true);
    }
    
    // Update chapter information
    updateChapterLists();
}

void MainWindow::onSyncPlayback()
{
    if (m_leftVideoWidget->currentFilePath().isEmpty() || 
        m_rightVideoWidget->currentFilePath().isEmpty()) {
        return;
    }
    
    if (m_isPlaying) {
        // Currently playing, so pause both
        m_leftVideoWidget->pause();
        m_rightVideoWidget->pause();
        m_syncButton->setText("Sync Playback");
        m_isPlaying = false;
    } else {
        // Currently paused, so sync positions with offset applied and play both
        qint64 basePosition = m_leftVideoWidget->position();
        
        // Apply offset: positive offset means Video A starts later than Video B
        qint64 relativeOffset = m_relativeOffsetSpinBox->value();
        qint64 videoAPosition = basePosition;
        qint64 videoBPosition = basePosition - relativeOffset;  // B needs to be ahead if A is delayed
        
        // Ensure positions are within bounds
        videoAPosition = qMax(0LL, videoAPosition);
        videoBPosition = qMax(0LL, videoBPosition);
        
        // Seek both videos to their offset-adjusted positions
        m_leftVideoWidget->seek(videoAPosition);
        m_rightVideoWidget->seek(videoBPosition);
        
        m_leftVideoWidget->play();
        m_rightVideoWidget->play();
        m_syncButton->setText("Sync Pause");
        m_isPlaying = true;
        
        qDebug() << "Sync playback with offset:" << relativeOffset << "ms";
        qDebug() << "  Video A position:" << videoAPosition << "ms";
        qDebug() << "  Video B position:" << videoBPosition << "ms";
    }
}

void MainWindow::onSeekToTimestamp()
{
    qint64 basePosition = m_timestampSlider->value();
    
    // Apply offset: positive offset means Video A starts later than Video B
    qint64 relativeOffset = m_relativeOffsetSpinBox->value();
    qint64 videoAPosition = basePosition;
    qint64 videoBPosition = basePosition - relativeOffset;  // B needs to be ahead if A is delayed
    
    // Ensure positions are within bounds
    videoAPosition = qMax(0LL, videoAPosition);
    videoBPosition = qMax(0LL, videoBPosition);
    
    // Seek both videos to their offset-adjusted positions
    if (!m_leftVideoWidget->currentFilePath().isEmpty()) {
        m_leftVideoWidget->seek(videoAPosition);
    }
    if (!m_rightVideoWidget->currentFilePath().isEmpty()) {
        m_rightVideoWidget->seek(videoBPosition);
    }
    
    // Update timestamp label (show the base position, not the offset positions) 
    QTime time = QTime::fromMSecsSinceStartOfDay(basePosition);
    m_timestampLabel->setText(time.toString("mm:ss"));
}

void MainWindow::onVideoPositionChanged(qint64 position)
{
    // Update the timestamp slider when videos are playing (but don't trigger seeking)
    // Only update if the slider is not currently being dragged by the user
    if (!m_timestampSlider->isSliderDown()) {
        // Convert the actual video position back to base timeline position
        // We use Video A position as the base since that's our reference
        qint64 basePosition = position; // Video A position is the base
        
        m_timestampSlider->blockSignals(true);
        m_timestampSlider->setValue(basePosition);
        m_timestampSlider->blockSignals(false);
        
        // Update timestamp label
        QTime time = QTime::fromMSecsSinceStartOfDay(basePosition);
        m_timestampLabel->setText(time.toString("mm:ss"));
        
        // Update current chapter display
        updateCurrentChapterDisplay(basePosition);
    }
}

void MainWindow::onTransferTracks()
{
    QString sourceFile = m_sourceVideoWidget->currentFilePath();
    QString targetFile = m_targetVideoWidget->currentFilePath();
    
    if (sourceFile.isEmpty() || targetFile.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please load both source and target videos.");
        return;
    }
    
    // Get checked tracks with their source information
    QList<QPair<QString, int>> selectedAudioTracks; // (source, trackIndex)
    QList<QPair<QString, int>> selectedSubtitleTracks;
    
    for (int i = 0; i < m_audioTracksList->count(); ++i) {
        QListWidgetItem *item = m_audioTracksList->item(i);
        if (item->checkState() == Qt::Checked) {
            QString trackData = item->data(Qt::UserRole).toString();
            QStringList parts = trackData.split(":");
            if (parts.size() == 2) {
                selectedAudioTracks.append(qMakePair(parts[0], parts[1].toInt()));
            }
        }
    }
    
    for (int i = 0; i < m_subtitleTracksList->count(); ++i) {
        QListWidgetItem *item = m_subtitleTracksList->item(i);
        if (item->checkState() == Qt::Checked) {
            QString trackData = item->data(Qt::UserRole).toString();
            QStringList parts = trackData.split(":");
            if (parts.size() == 2) {
                selectedSubtitleTracks.append(qMakePair(parts[0], parts[1].toInt()));
            }
        }
    }
    
    if (selectedAudioTracks.isEmpty() && selectedSubtitleTracks.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select at least one track to include in the output.");
        return;
    }
    
    // Generate output filename
    QFileInfo targetInfo(targetFile);
    QString outputFile = targetInfo.absolutePath() + "/" + 
                        targetInfo.baseName() + m_postfixEdit->text() + 
                        "." + targetInfo.suffix();
    
    // Clean up previous worker if exists
    if (m_transferThread) {
        m_transferThread->quit();
        m_transferThread->wait(1000);
        delete m_transferWorker;
        delete m_transferThread;
    }
    
    // Create new worker thread for transfer
    m_transferThread = new QThread(this);
    m_transferWorker = new TransferWorker();
    m_transferWorker->moveToThread(m_transferThread);
    
    // Set up the transfer job
    m_transferWorker->setTransferJob(sourceFile, targetFile, outputFile,
                                    selectedAudioTracks, selectedSubtitleTracks);
    
    // Connect worker signals
    connect(m_transferWorker, &TransferWorker::transferCompleted,
            this, &MainWindow::onTransferCompleted);
    connect(m_transferWorker, &TransferWorker::logMessage,
            this, &MainWindow::onTransferLogMessage);
    
    // Connect thread signals
    connect(m_transferThread, &QThread::started,
            m_transferWorker, &TransferWorker::startTransfer);
    connect(m_transferThread, &QThread::finished,
            m_transferWorker, &QObject::deleteLater);
    
    // Disable transfer button during processing
    m_transferButton->setEnabled(false);
    m_transferButton->setText("Transferring...");
    
    // Start processing in worker thread
    m_transferThread->start();
}

void MainWindow::onBatchProcess()
{
    // Implementation will be added with batch processor
}

void MainWindow::onTransferCompleted(bool success, const QString &message)
{
    // Re-enable transfer button
    m_transferButton->setEnabled(true);
    m_transferButton->setText("Transfer Selected Tracks");
    
    // Clean up worker thread
    if (m_transferThread) {
        m_transferThread->quit();
        m_transferThread->wait(3000);
        delete m_transferWorker;
        delete m_transferThread;
        m_transferWorker = nullptr;
        m_transferThread = nullptr;
    }
    
    // Show result to user
    if (success) {
        QMessageBox::information(this, "Success", message);
    } else {
        QMessageBox::critical(this, "Error", message);
    }
}

void MainWindow::onTransferLogMessage(const QString &message)
{
    // For now, we don't have a log widget in the transfer tab
    // This could be extended to add logging if needed
    qDebug() << "Transfer log:" << message;
}

void MainWindow::onPostfixChanged()
{
    // Update batch processor with new postfix
    if (m_batchProcessor) {
        m_batchProcessor->setOutputPostfix(m_postfixEdit->text());
    }
}

void MainWindow::updateTrackLists()
{
    m_audioTracksList->clear();
    m_subtitleTracksList->clear();
    
    QString sourceFile = m_sourceVideoWidget->currentFilePath();
    QString targetFile = m_targetVideoWidget->currentFilePath();
    
    FFmpegHandler handler;
    QIcon sourceIcon = createColoredIcon(QColor("#4CAF50"), 20); // Green for source
    QIcon targetIcon = createColoredIcon(QColor("#2196F3"), 20); // Blue for target
    
    // Load tracks from SOURCE video (tracks to potentially add)
    if (!sourceFile.isEmpty()) {
        QList<AudioTrackInfo> sourceAudioTracks = handler.getAudioTracks(sourceFile);
        for (const AudioTrackInfo &track : sourceAudioTracks) {
            QListWidgetItem *item = new QListWidgetItem();
            item->setText(QString("📁 FROM Source - Track %1: %2 [%3] - %4 (%5 ch, %6 Hz)")
                         .arg(track.index)
                         .arg(track.title)
                         .arg(track.language.toUpper())
                         .arg(track.codec.toUpper())
                         .arg(track.channels)
                         .arg(track.sampleRate));
            item->setIcon(sourceIcon);
            item->setCheckState(Qt::Unchecked); // Start unchecked - user decides what to add
            item->setData(Qt::UserRole, QString("source:%1").arg(track.index));
            item->setData(Qt::UserRole + 1, track.language);
            item->setData(Qt::UserRole + 2, track.codec);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            m_audioTracksList->addItem(item);
        }
        
        QList<SubtitleTrackInfo> sourceSubtitleTracks = handler.getSubtitleTracks(sourceFile);
        for (const SubtitleTrackInfo &track : sourceSubtitleTracks) {
            QListWidgetItem *item = new QListWidgetItem();
            item->setText(QString("📁 FROM Source - Track %1: %2 [%3] - %4")
                         .arg(track.index)
                         .arg(track.title)
                         .arg(track.language.toUpper())
                         .arg(track.codec.toUpper()));
            item->setIcon(sourceIcon);
            item->setCheckState(Qt::Unchecked); // Start unchecked - user decides what to add
            item->setData(Qt::UserRole, QString("source:%1").arg(track.index));
            item->setData(Qt::UserRole + 1, track.language);
            item->setData(Qt::UserRole + 2, track.codec);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            m_subtitleTracksList->addItem(item);
        }
    }
    
    // Load tracks from TARGET video (existing tracks to keep)
    if (!targetFile.isEmpty()) {
        QList<AudioTrackInfo> targetAudioTracks = handler.getAudioTracks(targetFile);
        for (const AudioTrackInfo &track : targetAudioTracks) {
            QListWidgetItem *item = new QListWidgetItem();
            item->setText(QString("🎯 FROM Target - Track %1: %2 [%3] - %4 (%5 ch, %6 Hz)")
                         .arg(track.index)
                         .arg(track.title)
                         .arg(track.language.toUpper())
                         .arg(track.codec.toUpper())
                         .arg(track.channels)
                         .arg(track.sampleRate));
            item->setIcon(targetIcon);
            item->setCheckState(Qt::Checked); // Start checked - preserve existing tracks by default
            item->setData(Qt::UserRole, QString("target:%1").arg(track.index));
            item->setData(Qt::UserRole + 1, track.language);
            item->setData(Qt::UserRole + 2, track.codec);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            m_audioTracksList->addItem(item);
        }
        
        QList<SubtitleTrackInfo> targetSubtitleTracks = handler.getSubtitleTracks(targetFile);
        for (const SubtitleTrackInfo &track : targetSubtitleTracks) {
            QListWidgetItem *item = new QListWidgetItem();
            item->setText(QString("🎯 FROM Target - Track %1: %2 [%3] - %4")
                         .arg(track.index)
                         .arg(track.title)
                         .arg(track.language.toUpper())
                         .arg(track.codec.toUpper()));
            item->setIcon(targetIcon);
            item->setCheckState(Qt::Checked); // Start checked - preserve existing tracks by default
            item->setData(Qt::UserRole, QString("target:%1").arg(track.index));
            item->setData(Qt::UserRole + 1, track.language);
            item->setData(Qt::UserRole + 2, track.codec);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            m_subtitleTracksList->addItem(item);
        }
    }
}

void MainWindow::onApplyAudioTemplate()
{
    QString template_ = m_audioTemplateEdit->text().toLower();
    if (template_.isEmpty()) return;
    
    for (int i = 0; i < m_audioTracksList->count(); ++i) {
        QListWidgetItem *item = m_audioTracksList->item(i);
        QString language = item->data(Qt::UserRole + 1).toString().toLower();
        QString codec = item->data(Qt::UserRole + 2).toString().toLower();
        QString fullText = item->text().toLower();
        
        bool matches = false;
        if (template_.contains("*")) {
            // Wildcard matching
            QString pattern = template_;
            pattern.replace("*", ".*");
            QRegularExpression regex(pattern);
            matches = regex.match(language).hasMatch() || 
                     regex.match(codec).hasMatch() || 
                     regex.match(fullText).hasMatch();
        } else {
            // Exact matching
            matches = language.contains(template_) || 
                     codec.contains(template_) || 
                     fullText.contains(template_);
        }
        
        item->setCheckState(matches ? Qt::Checked : Qt::Unchecked);
    }
}

void MainWindow::onApplySubtitleTemplate()
{
    QString template_ = m_subtitleTemplateEdit->text().toLower();
    if (template_.isEmpty()) return;
    
    for (int i = 0; i < m_subtitleTracksList->count(); ++i) {
        QListWidgetItem *item = m_subtitleTracksList->item(i);
        QString language = item->data(Qt::UserRole + 1).toString().toLower();
        QString codec = item->data(Qt::UserRole + 2).toString().toLower();
        QString fullText = item->text().toLower();
        
        bool matches = false;
        if (template_.contains("*")) {
            // Wildcard matching
            QString pattern = template_;
            pattern.replace("*", ".*");
            QRegularExpression regex(pattern);
            matches = regex.match(language).hasMatch() || 
                     regex.match(codec).hasMatch() || 
                     regex.match(fullText).hasMatch();
        } else {
            // Exact matching
            matches = language.contains(template_) || 
                     codec.contains(template_) || 
                     fullText.contains(template_);
        }
        
        item->setCheckState(matches ? Qt::Checked : Qt::Unchecked);
    }
}

void MainWindow::onSelectAllAudio()
{
    for (int i = 0; i < m_audioTracksList->count(); ++i) {
        m_audioTracksList->item(i)->setCheckState(Qt::Checked);
    }
}

void MainWindow::onSelectAllSubtitles()
{
    for (int i = 0; i < m_subtitleTracksList->count(); ++i) {
        m_subtitleTracksList->item(i)->setCheckState(Qt::Checked);
    }
}

void MainWindow::onClearAudioSelection()
{
    for (int i = 0; i < m_audioTracksList->count(); ++i) {
        m_audioTracksList->item(i)->setCheckState(Qt::Unchecked);
    }
}

void MainWindow::onClearSubtitleSelection()
{
    for (int i = 0; i < m_subtitleTracksList->count(); ++i) {
        m_subtitleTracksList->item(i)->setCheckState(Qt::Unchecked);
    }
}

QIcon MainWindow::createColoredIcon(const QColor &color, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Create a rounded rectangle with the specified color
    painter.setBrush(QBrush(color));
    painter.setPen(QPen(color.darker(150), 1));
    painter.drawRoundedRect(1, 1, size-2, size-2, 3, 3);
    
    return QIcon(pixmap);
}

void MainWindow::setupMenuBar()
{
    // Create View menu
    m_viewMenu = menuBar()->addMenu("&View");
    
    // Create Theme submenu
    m_themeMenu = m_viewMenu->addMenu("&Theme");
    
    // Create theme action group for exclusive selection
    m_themeActionGroup = new QActionGroup(this);
    
    // System theme action
    m_systemThemeAction = new QAction("&System", this);
    m_systemThemeAction->setCheckable(true);
    m_systemThemeAction->setActionGroup(m_themeActionGroup);
    connect(m_systemThemeAction, &QAction::triggered, this, &MainWindow::onSystemThemeTriggered);
    m_themeMenu->addAction(m_systemThemeAction);
    
    // Light theme action
    m_lightThemeAction = new QAction("&Light", this);
    m_lightThemeAction->setCheckable(true);
    m_lightThemeAction->setActionGroup(m_themeActionGroup);
    connect(m_lightThemeAction, &QAction::triggered, this, &MainWindow::onLightThemeTriggered);
    m_themeMenu->addAction(m_lightThemeAction);
    
    // Dark theme action
    m_darkThemeAction = new QAction("&Dark", this);
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setActionGroup(m_themeActionGroup);
    connect(m_darkThemeAction, &QAction::triggered, this, &MainWindow::onDarkThemeTriggered);
    m_themeMenu->addAction(m_darkThemeAction);
    
    // Set initial checked state based on current theme
    ThemeManager::Theme currentTheme = ThemeManager::instance()->currentTheme();
    switch (currentTheme) {
        case ThemeManager::System:
            m_systemThemeAction->setChecked(true);
            break;
        case ThemeManager::Light:
            m_lightThemeAction->setChecked(true);
            break;
        case ThemeManager::Dark:
            m_darkThemeAction->setChecked(true);
            break;
    }
}

void MainWindow::applyTheme()
{
    // Apply theme to main window and tabs
    setStyleSheet(ThemeManager::instance()->tabWidgetStyleSheet());
    
    // Refresh individual tab styling by re-calling setup methods
    // This ensures all widgets get updated with new theme colors
    refreshTabStyling();
    
    // Explicitly update BatchProcessor theme
    if (m_batchProcessor) {
        m_batchProcessor->applyTheme();
    }
}

void MainWindow::refreshTabStyling()
{
    // We can't easily re-setup all tabs without destroying the content,
    // but we can update the critical styling elements that use theme colors
    ThemeManager* theme = ThemeManager::instance();
    
    // Update comparison tab labels if they exist
    if (m_comparisonTab) {
        // Find and update labels in comparison tab
        QList<QLabel*> labels = m_comparisonTab->findChildren<QLabel*>();
        for (QLabel* label : labels) {
            if (label->text() == "Video A" || label->text() == "Video B") {
                label->setStyleSheet(QString("font-size: 12px; font-weight: 500; color: %1; padding: 2px;").arg(theme->secondaryTextColor()));
            } else if (label->text() == "Timeline:") {
                label->setStyleSheet(QString("font-size: 13px; color: %1; font-weight: 500;").arg(theme->secondaryTextColor()));
            }
        }
        
        // Update control panel
        QList<QWidget*> widgets = m_comparisonTab->findChildren<QWidget*>("controlPanel");
        for (QWidget* widget : widgets) {
            widget->setStyleSheet(QString(
                "QWidget { "
                "   background-color: %1; "
                "   border: 1px solid %2; "
                "   border-radius: 4px; "
                "}"
            ).arg(theme->surfaceColor()).arg(theme->borderColor()));
        }
        
        // Update sync button, auto compare button and timestamp label
        if (m_syncButton) m_syncButton->setStyleSheet(theme->primaryButtonStyleSheet());
        if (m_autoCompareButton) m_autoCompareButton->setStyleSheet(theme->successButtonStyleSheet());
        if (m_comparisonProgressBar) m_comparisonProgressBar->setStyleSheet(theme->progressBarStyleSheet());
        if (m_prevChapterButton) m_prevChapterButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_nextChapterButton) m_nextChapterButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_leftChaptersList) m_leftChaptersList->setStyleSheet(theme->listWidgetStyleSheet());
        if (m_rightChaptersList) m_rightChaptersList->setStyleSheet(theme->listWidgetStyleSheet());
        if (m_timestampSlider) m_timestampSlider->setStyleSheet(theme->sliderStyleSheet());
        if (m_timestampLabel) {
            m_timestampLabel->setStyleSheet(QString(
                "QLabel { "
                "   font-family: 'Consolas', 'Monaco', 'Courier New', monospace; "
                "   font-size: 13px; "
                "   color: %1; "
                "   background-color: %2; "
                "   border: 1px solid %3; "
                "   border-radius: 3px; "
                "   padding: 4px 8px; "
                "   min-width: 45px; "
                "}"
            ).arg(theme->textColor()).arg(theme->backgroundColor()).arg(theme->borderColor()));
        }
    }
    
    // Update Track Transfer tab styling
    if (m_transferTab) {
        // Update source and target container styling
        QList<QWidget*> containers = m_transferTab->findChildren<QWidget*>();
        for (QWidget* container : containers) {
            // Check if this is a source or target container by checking its parent layout
            QVBoxLayout* parentLayout = qobject_cast<QVBoxLayout*>(container->layout());
            if (parentLayout && (parentLayout->count() >= 2)) {
                QLabel* label = qobject_cast<QLabel*>(parentLayout->itemAt(0)->widget());
                if (label && (label->text().contains("Source Video") || label->text().contains("Target Video"))) {
                    container->setStyleSheet(QString(
                        "QWidget { "
                        "   background-color: %1; "
                        "   border: 1px solid %2; "
                        "   border-radius: 6px; "
                        "   padding: 8px; "
                        "}"
                    ).arg(theme->surfaceColor()).arg(theme->borderColor()));
                }
            }
        }
        
        // Update labels in Transfer tab
        QList<QLabel*> transferLabels = m_transferTab->findChildren<QLabel*>();
        for (QLabel* label : transferLabels) {
            if (label->text().contains("Source Video") || label->text().contains("Target Video")) {
                label->setStyleSheet(QString("font-size: 13px; font-weight: 500; color: %1;").arg(theme->textColor()));
            } else if (label->text() == "Template:") {
                label->setStyleSheet(QString("font-size: 12px; color: %1;").arg(theme->secondaryTextColor()));
            } else if (label->text() == "Output Postfix:") {
                label->setStyleSheet(QString("font-size: 13px; color: %1; font-weight: 500;").arg(theme->secondaryTextColor()));
            }
        }
        
        // Update all buttons in Transfer tab
        if (m_applyAudioTemplateButton) m_applyAudioTemplateButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_selectAllAudioButton) m_selectAllAudioButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_clearAudioButton) m_clearAudioButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_applySubtitleTemplateButton) m_applySubtitleTemplateButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_selectAllSubtitleButton) m_selectAllSubtitleButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_clearSubtitleButton) m_clearSubtitleButton->setStyleSheet(theme->buttonStyleSheet());
        if (m_transferButton) m_transferButton->setStyleSheet(theme->primaryButtonStyleSheet());
        
        // Update line edits
        if (m_audioTemplateEdit) m_audioTemplateEdit->setStyleSheet(theme->lineEditStyleSheet());
        if (m_subtitleTemplateEdit) m_subtitleTemplateEdit->setStyleSheet(theme->lineEditStyleSheet());
        if (m_postfixEdit) m_postfixEdit->setStyleSheet(theme->lineEditStyleSheet());
        
        // Update list widgets
        if (m_audioTracksList) m_audioTracksList->setStyleSheet(theme->listWidgetStyleSheet());
        if (m_subtitleTracksList) m_subtitleTracksList->setStyleSheet(theme->listWidgetStyleSheet());
        
        // Update group boxes - find and update them
        QList<QGroupBox*> groupBoxes = m_transferTab->findChildren<QGroupBox*>();
        for (QGroupBox* groupBox : groupBoxes) {
            groupBox->setStyleSheet(theme->groupBoxStyleSheet());
        }
        
        // Update transfer panel
        QList<QWidget*> transferPanels = m_transferTab->findChildren<QWidget*>();
        for (QWidget* panel : transferPanels) {
            // Look for the transfer panel by checking if it contains the postfix edit
            if (panel && panel->layout()) {
                QHBoxLayout* hLayout = qobject_cast<QHBoxLayout*>(panel->layout());
                if (hLayout && hLayout->count() >= 3) {
                    // Check if this layout contains our postfix controls
                    for (int i = 0; i < hLayout->count(); ++i) {
                        QWidget* widget = hLayout->itemAt(i)->widget();
                        if (widget == m_postfixEdit) {
                            panel->setStyleSheet(QString(
                                "QWidget { "
                                "   background-color: %1; "
                                "   border: 1px solid %2; "
                                "   border-radius: 6px; "
                                "}"
                            ).arg(theme->surfaceColor()).arg(theme->borderColor()));
                            break;
                        }
                    }
                }
            }
        }
    }
}

void MainWindow::onThemeChanged()
{
    applyTheme();
}

void MainWindow::onSystemThemeTriggered()
{
    ThemeManager::instance()->setTheme(ThemeManager::System);
}

void MainWindow::onLightThemeTriggered()
{
    ThemeManager::instance()->setTheme(ThemeManager::Light);
}

void MainWindow::onDarkThemeTriggered()
{
    ThemeManager::instance()->setTheme(ThemeManager::Dark);
}

void MainWindow::onAutoCompare()
{
    QString leftPath = m_leftVideoWidget->currentFilePath();
    QString rightPath = m_rightVideoWidget->currentFilePath();
    
    if (leftPath.isEmpty() || rightPath.isEmpty()) {
        m_comparisonResultLabel->setText("Please load both videos before starting auto comparison.");
        m_comparisonResultLabel->setStyleSheet(QString(
            "QLabel { "
            "   font-size: 12px; "
            "   color: %1; "
            "   padding: 4px; "
            "   background-color: transparent; "
            "   border: none; "
            "}"
        ).arg(ThemeManager::instance()->dangerColor()));
        return;
    }
    
    // Disable button and show progress
    m_autoCompareButton->setEnabled(false);
    m_autoCompareButton->setText("Comparing...");
    m_comparisonProgressBar->setVisible(true);
    m_comparisonProgressBar->setValue(0);
    m_comparisonResultLabel->setText("Analyzing video similarity... This may take a moment.");
    m_comparisonResultLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-size: 12px; "
        "   color: %1; "
        "   padding: 4px; "
        "   background-color: transparent; "
        "   border: none; "
        "}"
    ).arg(ThemeManager::instance()->secondaryTextColor()));
    
    // Start the automatic comparison
    m_comparator->startAutoComparison();
}

void MainWindow::onComparisonProgress(int percentage)
{
    m_comparisonProgressBar->setValue(percentage);
    m_comparisonResultLabel->setText(QString("Analyzing video similarity... %1% complete").arg(percentage));
}

void MainWindow::onComparisonComplete(const QList<ComparisonResult> &results)
{
    Q_UNUSED(results); // We're not using the detailed results for this version
}

void MainWindow::onAutoComparisonComplete(double overallSimilarity, bool videosIdentical, const QString &summary)
{
    // Re-enable button and hide progress
    m_autoCompareButton->setEnabled(true);
    m_autoCompareButton->setText("Auto Compare");
    m_comparisonProgressBar->setVisible(false);
    
    // Set result color based on outcome
    QString resultColor;
    if (videosIdentical) {
        resultColor = ThemeManager::instance()->successColor();
    } else if (overallSimilarity > 0.80) {
        resultColor = ThemeManager::instance()->primaryColor();
    } else {
        resultColor = ThemeManager::instance()->dangerColor();
    }
    
    m_comparisonResultLabel->setText(summary);
    m_comparisonResultLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-size: 12px; "
        "   color: %1; "
        "   padding: 4px; "
        "   background-color: transparent; "
        "   border: none; "
        "   font-weight: 500; "
        "}"
    ).arg(resultColor));
}

void MainWindow::onAutoOffset()
{
    QString leftPath = m_leftVideoWidget->currentFilePath();
    QString rightPath = m_rightVideoWidget->currentFilePath();
    
    if (leftPath.isEmpty() || rightPath.isEmpty()) {
        m_comparisonResultLabel->setText("Please load both videos before detecting optimal offset.");
        m_comparisonResultLabel->setStyleSheet(QString(
            "QLabel { "
            "   font-size: 12px; "
            "   color: %1; "
            "   padding: 4px; "
            "   background-color: transparent; "
            "   border: none; "
            "}"
        ).arg(ThemeManager::instance()->dangerColor()));
        return;
    }
    
    // Disable button and show progress
    m_autoOffsetButton->setEnabled(false);
    m_autoOffsetButton->setText("Detecting...");
    m_comparisonProgressBar->setVisible(true);
    m_comparisonProgressBar->setValue(0);
    m_comparisonResultLabel->setText("Fast detection: ±5s in 25ms steps (first quarter only)...");
    m_comparisonResultLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-size: 12px; "
        "   color: %1; "
        "   padding: 4px; "
        "   background-color: transparent; "
        "   border: none; "
        "}"
    ).arg(ThemeManager::instance()->secondaryTextColor()));
    
    // Start the automatic offset detection
    m_comparator->findOptimalOffset();
}

void MainWindow::onOptimalOffsetFound(qint64 optimalOffset, double confidence)
{
    // Re-enable button and hide progress
    m_autoOffsetButton->setEnabled(true);
    m_autoOffsetButton->setText("Auto Offset");
    m_comparisonProgressBar->setVisible(false);
    
    // Update the offset spinbox with the detected value
    m_relativeOffsetSpinBox->setValue(static_cast<int>(optimalOffset));
    
    // Show result message
    QString confidenceText;
    QColor resultColor;
    
    if (confidence > 0.7) {
        confidenceText = "High";
        resultColor = QColor(ThemeManager::instance()->successColor());
    } else if (confidence > 0.4) {
        confidenceText = "Medium";
        resultColor = QColor(ThemeManager::instance()->primaryColor());
    } else {
        confidenceText = "Low";
        resultColor = QColor(ThemeManager::instance()->dangerColor());
    }
    
    QString resultMessage;
    if (optimalOffset == 0) {
        resultMessage = QString("Videos appear to be already synchronized.\nConfidence: %1 (%2%)")
                            .arg(confidenceText)
                            .arg(static_cast<int>(confidence * 100));
    } else {
        QString direction = (optimalOffset > 0) ? "later" : "earlier";
        resultMessage = QString("Optimal offset detected: %1ms\nVideo A starts %2ms %3 than Video B.\nConfidence: %4 (%5%)")
                            .arg(optimalOffset)
                            .arg(qAbs(optimalOffset))
                            .arg(direction)
                            .arg(confidenceText)
                            .arg(static_cast<int>(confidence * 100));
    }
    
    m_comparisonResultLabel->setText(resultMessage);
    m_comparisonResultLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-size: 12px; "
        "   color: %1; "
        "   padding: 4px; "
        "   background-color: transparent; "
        "   border: none; "
        "   font-weight: 500; "
        "}"
    ).arg(resultColor.name()));
}

void MainWindow::updateChapterLists()
{
    FFmpegHandler handler;
    
    // Clear existing chapters
    m_leftChaptersList->clear();
    m_rightChaptersList->clear();
    m_leftVideoChapters.clear();
    m_rightVideoChapters.clear();
    
    // Load chapters for left video
    QString leftPath = m_leftVideoWidget->currentFilePath();
    if (!leftPath.isEmpty()) {
        m_leftVideoChapters = handler.getChapters(leftPath);
        for (const ChapterInfo &chapter : m_leftVideoChapters) {
            QListWidgetItem *item = new QListWidgetItem();
            item->setText(QString("%1 - %2").arg(chapter.formattedTime).arg(chapter.title));
            item->setData(Qt::UserRole, chapter.startTimeMs);
            item->setToolTip(QString("Jump to %1").arg(chapter.title));
            m_leftChaptersList->addItem(item);
        }
    }
    
    // Load chapters for right video
    QString rightPath = m_rightVideoWidget->currentFilePath();
    if (!rightPath.isEmpty()) {
        m_rightVideoChapters = handler.getChapters(rightPath);
        for (const ChapterInfo &chapter : m_rightVideoChapters) {
            QListWidgetItem *item = new QListWidgetItem();
            item->setText(QString("%1 - %2").arg(chapter.formattedTime).arg(chapter.title));
            item->setData(Qt::UserRole, chapter.startTimeMs);
            item->setToolTip(QString("Jump to %1").arg(chapter.title));
            m_rightChaptersList->addItem(item);
        }
    }
    
    // Update navigation state
    updateChapterNavigation();
}

void MainWindow::onChapterSelected()
{
    QListWidget *senderList = qobject_cast<QListWidget*>(sender());
    if (!senderList) return;
    
    QListWidgetItem *selectedItem = senderList->currentItem();
    if (!selectedItem) return;
    
    qint64 baseTimestamp = selectedItem->data(Qt::UserRole).toLongLong();
    
    // Apply offset: positive offset means Video A starts later than Video B
    qint64 relativeOffset = m_relativeOffsetSpinBox->value();
    qint64 videoATimestamp = baseTimestamp;
    qint64 videoBTimestamp = baseTimestamp - relativeOffset;
    
    // Ensure timestamps are within bounds
    videoATimestamp = qMax(0LL, videoATimestamp);
    videoBTimestamp = qMax(0LL, videoBTimestamp);
    
    // Seek both videos to their offset-adjusted timestamps
    if (!m_leftVideoWidget->currentFilePath().isEmpty()) {
        m_leftVideoWidget->seek(videoATimestamp);
    }
    if (!m_rightVideoWidget->currentFilePath().isEmpty()) {
        m_rightVideoWidget->seek(videoBTimestamp);
    }
    
    // Update timestamp slider
    m_timestampSlider->blockSignals(true);
    m_timestampSlider->setValue(baseTimestamp);
    m_timestampSlider->blockSignals(false);
    
    // Update current chapter display
    updateCurrentChapterDisplay(baseTimestamp);
}

void MainWindow::onPreviousChapter()
{
    if (m_currentChapterIndex <= 0) return;
    
    m_currentChapterIndex--;
    jumpToCurrentChapter();
}

void MainWindow::onNextChapter()
{
    // Find the maximum available chapters between both videos
    int maxChapters = qMax(m_leftVideoChapters.size(), m_rightVideoChapters.size());
    
    if (m_currentChapterIndex >= maxChapters - 1) return;
    
    m_currentChapterIndex++;
    jumpToCurrentChapter();
}

void MainWindow::jumpToCurrentChapter()
{
    if (m_currentChapterIndex < 0) return;
    
    qint64 baseTimestamp = 0;
    
    // Prefer left video chapters, fall back to right video chapters
    if (m_currentChapterIndex < m_leftVideoChapters.size()) {
        baseTimestamp = m_leftVideoChapters[m_currentChapterIndex].startTimeMs;
    } else if (m_currentChapterIndex < m_rightVideoChapters.size()) {
        baseTimestamp = m_rightVideoChapters[m_currentChapterIndex].startTimeMs;
    } else {
        return; // No valid chapter
    }
    
    // Apply offset: positive offset means Video A starts later than Video B
    qint64 relativeOffset = m_relativeOffsetSpinBox->value();
    qint64 videoATimestamp = baseTimestamp;
    qint64 videoBTimestamp = baseTimestamp - relativeOffset;
    
    // Ensure timestamps are within bounds
    videoATimestamp = qMax(0LL, videoATimestamp);
    videoBTimestamp = qMax(0LL, videoBTimestamp);
    
    // Seek both videos to their offset-adjusted timestamps
    if (!m_leftVideoWidget->currentFilePath().isEmpty()) {
        m_leftVideoWidget->seek(videoATimestamp);
    }
    if (!m_rightVideoWidget->currentFilePath().isEmpty()) {
        m_rightVideoWidget->seek(videoBTimestamp);
    }
    
    // Update timestamp slider
    m_timestampSlider->blockSignals(true);
    m_timestampSlider->setValue(baseTimestamp);
    m_timestampSlider->blockSignals(false);
    
    // Update display
    updateCurrentChapterDisplay(baseTimestamp);
    updateChapterNavigation();
}

void MainWindow::updateCurrentChapterDisplay(qint64 currentTimestamp)
{
    QString chapterText = "No chapters";
    
    // Find current chapter in left video
    for (int i = 0; i < m_leftVideoChapters.size(); ++i) {
        const ChapterInfo &chapter = m_leftVideoChapters[i];
        if (currentTimestamp >= chapter.startTimeMs && 
            (i == m_leftVideoChapters.size() - 1 || currentTimestamp < m_leftVideoChapters[i + 1].startTimeMs)) {
            chapterText = QString("A: %1").arg(chapter.title);
            m_currentChapterIndex = i;
            break;
        }
    }
    
    // Also check right video chapters
    QString rightChapterText;
    for (int i = 0; i < m_rightVideoChapters.size(); ++i) {
        const ChapterInfo &chapter = m_rightVideoChapters[i];
        if (currentTimestamp >= chapter.startTimeMs && 
            (i == m_rightVideoChapters.size() - 1 || currentTimestamp < m_rightVideoChapters[i + 1].startTimeMs)) {
            rightChapterText = QString("B: %1").arg(chapter.title);
            if (chapterText == "No chapters") {
                chapterText = rightChapterText;
                m_currentChapterIndex = i;
            } else {
                chapterText += QString("\n%1").arg(rightChapterText);
            }
            break;
        }
    }
    
    m_currentChapterLabel->setText(chapterText);
    updateChapterNavigation();
}

void MainWindow::updateChapterNavigation()
{
    int maxChapters = qMax(m_leftVideoChapters.size(), m_rightVideoChapters.size());
    
    if (maxChapters == 0) {
        m_prevChapterButton->setEnabled(false);
        m_nextChapterButton->setEnabled(false);
        m_currentChapterLabel->setText("No chapters");
        return;
    }
    
    m_prevChapterButton->setEnabled(m_currentChapterIndex > 0);
    m_nextChapterButton->setEnabled(m_currentChapterIndex < maxChapters - 1);
}

