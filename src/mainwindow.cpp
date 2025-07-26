#include "mainwindow.h"
#include "videowidget.h"
#include "videocomparator.h"
#include "batchprocessor.h"
#include "ffmpeghandler.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_comparator(new VideoComparator(this))
    , m_isPlaying(false)
{
    setupUI();
    setCentralWidget(m_tabWidget);
    setWindowTitle("VideoMaster - Video Comparison & Track Transfer Tool");
    resize(1200, 800);
}

MainWindow::~MainWindow() = default;

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
    m_comparisonTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_comparisonTab);
    
    // Video widgets in splitter
    QSplitter *videoSplitter = new QSplitter(Qt::Horizontal);
    m_leftVideoWidget = new VideoWidget("Video A", this);
    m_rightVideoWidget = new VideoWidget("Video B", this);
    
    videoSplitter->addWidget(m_leftVideoWidget);
    videoSplitter->addWidget(m_rightVideoWidget);
    videoSplitter->setSizes({600, 600});
    
    // Controls
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    m_syncButton = new QPushButton("Sync Playback", this);
    m_timestampSlider = new QSlider(Qt::Horizontal, this);
    m_timestampSlider->setEnabled(false); // Initially disabled until videos are loaded
    m_timestampLabel = new QLabel("Timestamp: 00:00", this);
    
    controlsLayout->addWidget(m_syncButton);
    controlsLayout->addWidget(new QLabel("Seek to:"));
    controlsLayout->addWidget(m_timestampSlider);
    controlsLayout->addWidget(m_timestampLabel);
    controlsLayout->addStretch();
    
    layout->addWidget(videoSplitter);
    layout->addLayout(controlsLayout);
    
    connect(m_leftVideoWidget, &VideoWidget::videoLoaded, 
            [this](const QString &path) { onVideoLoaded(0, path); });
    connect(m_rightVideoWidget, &VideoWidget::videoLoaded, 
            [this](const QString &path) { onVideoLoaded(1, path); });
    connect(m_syncButton, &QPushButton::clicked, this, &MainWindow::onSyncPlayback);
    connect(m_timestampSlider, &QSlider::sliderMoved, this, &MainWindow::onSeekToTimestamp);
    connect(m_timestampSlider, &QSlider::sliderPressed, this, &MainWindow::onSeekToTimestamp);
    
    // Connect position changes from video widgets to update slider and sync
    connect(m_leftVideoWidget, &VideoWidget::positionChanged, this, &MainWindow::onVideoPositionChanged);
    connect(m_rightVideoWidget, &VideoWidget::positionChanged, this, &MainWindow::onVideoPositionChanged);
}

void MainWindow::setupTransferTab()
{
    m_transferTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_transferTab);
    
    // Source and target video widgets with color coding
    QHBoxLayout *videoLayout = new QHBoxLayout();
    m_sourceVideoWidget = new VideoWidget("📁 Source Video (tracks to copy FROM)", this);
    m_targetVideoWidget = new VideoWidget("🎯 Target Video (base video to copy TO)", this);
    
    // Add colored borders to make the connection clear
    m_sourceVideoWidget->setStyleSheet(
        "QWidget { border: 3px solid #4CAF50; border-radius: 8px; background-color: rgba(76, 175, 80, 0.1); }"
        "QLabel { border: none; background-color: transparent; }"
    );
    m_targetVideoWidget->setStyleSheet(
        "QWidget { border: 3px solid #2196F3; border-radius: 8px; background-color: rgba(33, 150, 243, 0.1); }"
        "QLabel { border: none; background-color: transparent; }"
    );
    
    videoLayout->addWidget(m_sourceVideoWidget);
    videoLayout->addWidget(m_targetVideoWidget);
    
    // Track selection with templates
    QHBoxLayout *tracksLayout = new QHBoxLayout();
    
    // Audio tracks section showing all tracks from both videos
    QGroupBox *audioGroup = new QGroupBox("🎵 Audio Tracks Selection (check tracks to include in output)");
    audioGroup->setStyleSheet("QGroupBox::title { color: #333; font-weight: bold; }");
    QVBoxLayout *audioLayout = new QVBoxLayout(audioGroup);
    
    // Audio template controls
    QHBoxLayout *audioTemplateLayout = new QHBoxLayout();
    audioTemplateLayout->addWidget(new QLabel("Template:"));
    m_audioTemplateEdit = new QLineEdit("*eng*", this);
    m_audioTemplateEdit->setPlaceholderText("e.g., *eng*, *jpn*, *deu*, *ac3*");
    m_applyAudioTemplateButton = new QPushButton("Apply", this);
    m_selectAllAudioButton = new QPushButton("All", this);
    m_clearAudioButton = new QPushButton("Clear", this);
    
    audioTemplateLayout->addWidget(m_audioTemplateEdit);
    audioTemplateLayout->addWidget(m_applyAudioTemplateButton);
    audioTemplateLayout->addWidget(m_selectAllAudioButton);
    audioTemplateLayout->addWidget(m_clearAudioButton);
    
    m_audioTracksList = new QListWidget();
    m_audioTracksList->setToolTip("Check the audio tracks to include in the output video (from both source and target)");
    
    audioLayout->addLayout(audioTemplateLayout);
    audioLayout->addWidget(m_audioTracksList);
    
    // Subtitle tracks section showing all tracks from both videos
    QGroupBox *subtitleGroup = new QGroupBox("💬 Subtitle Tracks Selection (check tracks to include in output)");
    subtitleGroup->setStyleSheet("QGroupBox::title { color: #333; font-weight: bold; }");
    QVBoxLayout *subtitleLayout = new QVBoxLayout(subtitleGroup);
    
    // Subtitle template controls
    QHBoxLayout *subtitleTemplateLayout = new QHBoxLayout();
    subtitleTemplateLayout->addWidget(new QLabel("Template:"));
    m_subtitleTemplateEdit = new QLineEdit("*eng*", this);
    m_subtitleTemplateEdit->setPlaceholderText("e.g., *eng*, *jpn*, *deu*, *srt*");
    m_applySubtitleTemplateButton = new QPushButton("Apply", this);
    m_selectAllSubtitleButton = new QPushButton("All", this);
    m_clearSubtitleButton = new QPushButton("Clear", this);
    
    subtitleTemplateLayout->addWidget(m_subtitleTemplateEdit);
    subtitleTemplateLayout->addWidget(m_applySubtitleTemplateButton);
    subtitleTemplateLayout->addWidget(m_selectAllSubtitleButton);
    subtitleTemplateLayout->addWidget(m_clearSubtitleButton);
    
    m_subtitleTracksList = new QListWidget();
    m_subtitleTracksList->setToolTip("Check the subtitle tracks to include in the output video (from both source and target)");
    
    subtitleLayout->addLayout(subtitleTemplateLayout);
    subtitleLayout->addWidget(m_subtitleTracksList);
    
    tracksLayout->addWidget(audioGroup);
    tracksLayout->addWidget(subtitleGroup);
    
    // Add visual legend explaining the new merge approach
    QHBoxLayout *legendLayout = new QHBoxLayout();
    QLabel *legendLabel = new QLabel("📖 Track Selection Guide:");
    legendLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    
    QLabel *sourceColorLabel = new QLabel();
    sourceColorLabel->setPixmap(createColoredIcon(QColor("#4CAF50"), 16).pixmap(16, 16));
    QLabel *sourceTextLabel = new QLabel("📁 Source tracks (unchecked by default - check to ADD)");
    sourceTextLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    
    QLabel *targetColorLabel = new QLabel();
    targetColorLabel->setPixmap(createColoredIcon(QColor("#2196F3"), 16).pixmap(16, 16));
    QLabel *targetTextLabel = new QLabel("🎯 Target tracks (checked by default - uncheck to REMOVE)");
    targetTextLabel->setStyleSheet("color: #2196F3; font-weight: bold;");
    
    legendLayout->addWidget(legendLabel);
    legendLayout->addWidget(sourceColorLabel);
    legendLayout->addWidget(sourceTextLabel);
    legendLayout->addWidget(new QLabel("  |  "));
    legendLayout->addWidget(targetColorLabel);
    legendLayout->addWidget(targetTextLabel);
    legendLayout->addStretch();
    
    // Transfer controls
    QHBoxLayout *transferLayout = new QHBoxLayout();
    transferLayout->addWidget(new QLabel("Output Postfix:"));
    m_postfixEdit = new QLineEdit("_merged", this);
    m_transferButton = new QPushButton("Transfer Selected Tracks", this);
    
    transferLayout->addWidget(m_postfixEdit);
    transferLayout->addWidget(m_transferButton);
    transferLayout->addStretch();
    
    layout->addLayout(videoLayout);
    layout->addLayout(tracksLayout);
    layout->addLayout(legendLayout);
    layout->addLayout(transferLayout);
    
    // Connect all the signals
    connect(m_transferButton, &QPushButton::clicked, this, &MainWindow::onTransferTracks);
    connect(m_postfixEdit, &QLineEdit::textChanged, this, &MainWindow::onPostfixChanged);
    
    // Template controls
    connect(m_applyAudioTemplateButton, &QPushButton::clicked, this, &MainWindow::onApplyAudioTemplate);
    connect(m_applySubtitleTemplateButton, &QPushButton::clicked, this, &MainWindow::onApplySubtitleTemplate);
    connect(m_selectAllAudioButton, &QPushButton::clicked, this, &MainWindow::onSelectAllAudio);
    connect(m_selectAllSubtitleButton, &QPushButton::clicked, this, &MainWindow::onSelectAllSubtitles);
    connect(m_clearAudioButton, &QPushButton::clicked, this, &MainWindow::onClearAudioSelection);
    connect(m_clearSubtitleButton, &QPushButton::clicked, this, &MainWindow::onClearSubtitleSelection);
    
    // Connect both video loading events to track list updates
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
        // Currently paused, so sync positions and play both
        qint64 position = m_leftVideoWidget->position();
        m_rightVideoWidget->seek(position);
        
        m_leftVideoWidget->play();
        m_rightVideoWidget->play();
        m_syncButton->setText("Sync Pause");
        m_isPlaying = true;
    }
}

void MainWindow::onSeekToTimestamp()
{
    qint64 position = m_timestampSlider->value();
    
    // Seek both videos to the same position immediately
    if (!m_leftVideoWidget->currentFilePath().isEmpty()) {
        m_leftVideoWidget->seek(position);
    }
    if (!m_rightVideoWidget->currentFilePath().isEmpty()) {
        m_rightVideoWidget->seek(position);
    }
    
    // Update timestamp label
    QTime time = QTime::fromMSecsSinceStartOfDay(position);
    m_timestampLabel->setText(QString("Timestamp: %1").arg(time.toString("mm:ss")));
}

void MainWindow::onVideoPositionChanged(qint64 position)
{
    // Update the timestamp slider when videos are playing (but don't trigger seeking)
    // Only update if the slider is not currently being dragged by the user
    if (!m_timestampSlider->isSliderDown()) {
        m_timestampSlider->blockSignals(true);
        m_timestampSlider->setValue(position);
        m_timestampSlider->blockSignals(false);
        
        // Update timestamp label
        QTime time = QTime::fromMSecsSinceStartOfDay(position);
        m_timestampLabel->setText(QString("Timestamp: %1").arg(time.toString("mm:ss")));
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
    
    // Perform merge operation
    FFmpegHandler handler;
    bool success = handler.mergeTracks(sourceFile, targetFile, outputFile,
                                      selectedAudioTracks, selectedSubtitleTracks);
    
    if (success) {
        QMessageBox::information(this, "Success", 
                                QString("Tracks transferred successfully to:\n%1").arg(outputFile));
    } else {
        QMessageBox::critical(this, "Error", "Failed to transfer tracks.");
    }
}

void MainWindow::onBatchProcess()
{
    // Implementation will be added with batch processor
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

