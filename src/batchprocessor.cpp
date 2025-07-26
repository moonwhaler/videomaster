#include "batchprocessor.h"
#include "ffmpeghandler.h"
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QSplitter>
#include <QRegularExpression>
#include <QPainter>
#include <QPixmap>

BatchProcessor::BatchProcessor(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(new QVBoxLayout(this))
    , m_currentPostfix("_merged")
    , m_processingCancelled(false)
{
    setupUI();
}

void BatchProcessor::setupUI()
{
    // Directory selection section with better terminology
    QGroupBox *directionGroup = new QGroupBox("📂 Input & Output Directories");
    QVBoxLayout *dirLayout = new QVBoxLayout(directionGroup);
    
    QHBoxLayout *sourceLayout = new QHBoxLayout();
    QLabel *sourceLabel = new QLabel("📁 Tracks Input Directory (videos with tracks to copy FROM):");
    sourceLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    sourceLayout->addWidget(sourceLabel);
    sourceLayout->addStretch();
    
    QHBoxLayout *sourcePathLayout = new QHBoxLayout();
    m_sourceDirectoryEdit = new QLineEdit();
    m_sourceDirectoryEdit->setStyleSheet("border: 2px solid #4CAF50; border-radius: 4px; padding: 4px;");
    m_selectSourceButton = new QPushButton("Browse");
    sourcePathLayout->addWidget(m_sourceDirectoryEdit);
    sourcePathLayout->addWidget(m_selectSourceButton);
    
    QHBoxLayout *targetLayout = new QHBoxLayout();
    QLabel *targetLabel = new QLabel("🎯 Base Videos Input Directory (videos to receive tracks):");
    targetLabel->setStyleSheet("color: #2196F3; font-weight: bold;");
    targetLayout->addWidget(targetLabel);
    targetLayout->addStretch();
    
    QHBoxLayout *targetPathLayout = new QHBoxLayout();
    m_targetDirectoryEdit = new QLineEdit();
    m_targetDirectoryEdit->setStyleSheet("border: 2px solid #2196F3; border-radius: 4px; padding: 4px;");
    m_selectTargetButton = new QPushButton("Browse");
    targetPathLayout->addWidget(m_targetDirectoryEdit);
    targetPathLayout->addWidget(m_selectTargetButton);
    
    QHBoxLayout *outputLayout = new QHBoxLayout();
    QLabel *outputLabel = new QLabel("💾 Output Directory (where merged videos will be saved):");
    outputLabel->setStyleSheet("color: #FF9800; font-weight: bold;");
    outputLayout->addWidget(outputLabel);
    outputLayout->addStretch();
    
    QHBoxLayout *outputPathLayout = new QHBoxLayout();
    m_outputDirectoryEdit = new QLineEdit();
    m_outputDirectoryEdit->setStyleSheet("border: 2px solid #FF9800; border-radius: 4px; padding: 4px;");
    m_selectOutputButton = new QPushButton("Browse");
    outputPathLayout->addWidget(m_outputDirectoryEdit);
    outputPathLayout->addWidget(m_selectOutputButton);
    
    dirLayout->addLayout(sourceLayout);
    dirLayout->addLayout(sourcePathLayout);
    dirLayout->addWidget(new QLabel("")); // Spacer
    dirLayout->addLayout(targetLayout);
    dirLayout->addLayout(targetPathLayout);
    dirLayout->addWidget(new QLabel("")); // Spacer
    dirLayout->addLayout(outputLayout);
    dirLayout->addLayout(outputPathLayout);
    
    // File matching section with color coding
    QGroupBox *matchingGroup = new QGroupBox("🔗 File Matching & Pairing");
    QHBoxLayout *matchingLayout = new QHBoxLayout(matchingGroup);
    
    QVBoxLayout *sourceFilesLayout = new QVBoxLayout();
    QLabel *sourceFilesLabel = new QLabel("📁 Track Files (FROM these):");
    sourceFilesLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    sourceFilesLayout->addWidget(sourceFilesLabel);
    m_sourceFilesList = new QListWidget();
    m_sourceFilesList->setStyleSheet("border: 2px solid #4CAF50; border-radius: 4px;");
    sourceFilesLayout->addWidget(m_sourceFilesList);
    
    QVBoxLayout *controlsLayout = new QVBoxLayout();
    m_autoMatchButton = new QPushButton("🤖 Auto Match");
    m_autoMatchButton->setToolTip("Automatically match files based on similar names");
    m_moveUpButton = new QPushButton("⬆️ Move Up");
    m_moveDownButton = new QPushButton("⬇️ Move Down");
    controlsLayout->addWidget(m_autoMatchButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(new QLabel("Reorder Target Files:"));
    controlsLayout->addWidget(m_moveUpButton);
    controlsLayout->addWidget(m_moveDownButton);
    controlsLayout->addStretch();
    
    QVBoxLayout *targetFilesLayout = new QVBoxLayout();
    QLabel *targetFilesLabel = new QLabel("🎯 Base Video Files (TO these):");
    targetFilesLabel->setStyleSheet("color: #2196F3; font-weight: bold;");
    targetFilesLayout->addWidget(targetFilesLabel);
    m_targetFilesList = new QListWidget();
    m_targetFilesList->setStyleSheet("border: 2px solid #2196F3; border-radius: 4px;");
    targetFilesLayout->addWidget(m_targetFilesList);
    
    matchingLayout->addLayout(sourceFilesLayout);
    matchingLayout->addLayout(controlsLayout);
    matchingLayout->addLayout(targetFilesLayout);
    
    // Track selection with merge approach (same as main Track Transfer tab)
    QHBoxLayout *tracksOptionsLayout = new QHBoxLayout();
    
    // Audio tracks section showing all tracks from both videos
    QGroupBox *audioGroup = new QGroupBox("🎵 Audio Tracks Selection (template-based for batch scale)");
    audioGroup->setStyleSheet("QGroupBox::title { color: #333; font-weight: bold; }");
    QVBoxLayout *audioLayout = new QVBoxLayout(audioGroup);
    
    // Audio template controls
    QHBoxLayout *audioTemplateLayout = new QHBoxLayout();
    audioTemplateLayout->addWidget(new QLabel("Template:"));
    m_audioTemplateEdit = new QLineEdit("*eng*");
    m_audioTemplateEdit->setPlaceholderText("e.g., *eng*, *jpn*, *deu*, *ac3*");
    m_applyAudioTemplateButton = new QPushButton("Apply");
    m_selectAllAudioButton = new QPushButton("All");
    m_clearAudioButton = new QPushButton("Clear");
    
    audioTemplateLayout->addWidget(m_audioTemplateEdit);
    audioTemplateLayout->addWidget(m_applyAudioTemplateButton);
    audioTemplateLayout->addWidget(m_selectAllAudioButton);
    audioTemplateLayout->addWidget(m_clearAudioButton);
    
    m_audioTracksList = new QListWidget();
    m_audioTracksList->setMaximumHeight(150);
    m_audioTracksList->setToolTip("Template selects tracks from source videos to ADD to existing tracks");
    
    audioLayout->addLayout(audioTemplateLayout);
    audioLayout->addWidget(m_audioTracksList);
    
    // Subtitle tracks section showing all tracks from both videos
    QGroupBox *subtitleGroup = new QGroupBox("💬 Subtitle Tracks Selection (template-based for batch scale)");
    subtitleGroup->setStyleSheet("QGroupBox::title { color: #333; font-weight: bold; }");
    QVBoxLayout *subtitleLayout = new QVBoxLayout(subtitleGroup);
    
    // Subtitle template controls
    QHBoxLayout *subtitleTemplateLayout = new QHBoxLayout();
    subtitleTemplateLayout->addWidget(new QLabel("Template:"));
    m_subtitleTemplateEdit = new QLineEdit("*eng*");
    m_subtitleTemplateEdit->setPlaceholderText("e.g., *eng*, *jpn*, *deu*, *srt*");
    m_applySubtitleTemplateButton = new QPushButton("Apply");
    m_selectAllSubtitleButton = new QPushButton("All");
    m_clearSubtitleButton = new QPushButton("Clear");
    
    subtitleTemplateLayout->addWidget(m_subtitleTemplateEdit);
    subtitleTemplateLayout->addWidget(m_applySubtitleTemplateButton);
    subtitleTemplateLayout->addWidget(m_selectAllSubtitleButton);
    subtitleTemplateLayout->addWidget(m_clearSubtitleButton);
    
    m_subtitleTracksList = new QListWidget();
    m_subtitleTracksList->setMaximumHeight(150);
    m_subtitleTracksList->setToolTip("Template selects tracks from source videos to ADD to existing tracks");
    
    subtitleLayout->addLayout(subtitleTemplateLayout);
    subtitleLayout->addWidget(m_subtitleTracksList);
    
    tracksOptionsLayout->addWidget(audioGroup);
    tracksOptionsLayout->addWidget(subtitleGroup);
    
    // Add visual legend explaining the batch merge approach
    QHBoxLayout *legendLayout = new QHBoxLayout();
    QLabel *legendLabel = new QLabel("📖 Batch Processing Guide:");
    legendLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    
    QLabel *infoLabel = new QLabel("Templates select source tracks to ADD to existing target tracks (preserves originals by default)");
    infoLabel->setStyleSheet("color: #666; font-style: italic;");
    
    legendLayout->addWidget(legendLabel);
    legendLayout->addWidget(infoLabel);
    legendLayout->addStretch();
    
    // Optional checkbox to remove existing tracks
    QHBoxLayout *optionsLayout = new QHBoxLayout();
    m_removeExistingTracksCheckbox = new QCheckBox("Remove existing tracks from target videos (DESTRUCTIVE - use with caution)");
    m_removeExistingTracksCheckbox->setChecked(false); // Default: preserve existing tracks
    m_removeExistingTracksCheckbox->setStyleSheet("color: #d32f2f; font-weight: bold;");
    m_removeExistingTracksCheckbox->setToolTip("WARNING: This will completely remove existing audio/subtitle tracks from target videos!");
    
    optionsLayout->addWidget(m_removeExistingTracksCheckbox);
    optionsLayout->addStretch();
    
    // Output settings and processing section
    QGroupBox *processingGroup = new QGroupBox("💾 Output Settings & Processing");
    QVBoxLayout *processingLayout = new QVBoxLayout(processingGroup);
    
    // Output postfix settings
    QHBoxLayout *postfixLayout = new QHBoxLayout();
    postfixLayout->addWidget(new QLabel("Output File Postfix:"));
    m_postfixEdit = new QLineEdit(m_currentPostfix);
    m_postfixEdit->setStyleSheet("border: 2px solid #FF9800; border-radius: 4px; padding: 4px;");
    m_postfixEdit->setToolTip("This will be added to each output filename (e.g., movie_merged.mp4)");
    postfixLayout->addWidget(m_postfixEdit);
    postfixLayout->addStretch();
    
    // Processing controls
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("🚀 Start Batch Processing");
    m_startButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; border-radius: 4px; } QPushButton:hover { background-color: #45a049; }");
    
    m_stopButton = new QPushButton("⏹️ Stop Processing");
    m_stopButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; padding: 8px; border-radius: 4px; } QPushButton:hover { background-color: #d32f2f; }");
    m_stopButton->setEnabled(false); // Initially disabled
    
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addStretch();
    
    m_progressBar = new QProgressBar();
    m_logOutput = new QTextEdit();
    m_logOutput->setMaximumHeight(120);
    m_logOutput->setReadOnly(true);
    m_logOutput->setStyleSheet("border: 1px solid #ccc; border-radius: 4px;");
    
    processingLayout->addLayout(postfixLayout);
    processingLayout->addWidget(new QLabel("")); // Spacer
    processingLayout->addLayout(buttonLayout);
    processingLayout->addWidget(m_progressBar);
    processingLayout->addWidget(new QLabel("📄 Processing Log:"));
    processingLayout->addWidget(m_logOutput);
    
    // Add all groups to main layout
    m_mainLayout->addWidget(directionGroup);
    m_mainLayout->addWidget(matchingGroup);
    m_mainLayout->addLayout(tracksOptionsLayout);
    m_mainLayout->addLayout(legendLayout);
    m_mainLayout->addLayout(optionsLayout);
    m_mainLayout->addWidget(processingGroup);
    
    // Connect signals
    connect(m_selectSourceButton, &QPushButton::clicked, this, &BatchProcessor::onSelectSourceDirectory);
    connect(m_selectTargetButton, &QPushButton::clicked, this, &BatchProcessor::onSelectTargetDirectory);
    connect(m_selectOutputButton, &QPushButton::clicked, this, &BatchProcessor::onSelectOutputDirectory);
    connect(m_autoMatchButton, &QPushButton::clicked, this, &BatchProcessor::onAutoMatch);
    connect(m_moveUpButton, &QPushButton::clicked, this, &BatchProcessor::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &BatchProcessor::onMoveDown);
    connect(m_startButton, &QPushButton::clicked, this, &BatchProcessor::onStartBatchProcess);
    connect(m_stopButton, &QPushButton::clicked, this, &BatchProcessor::onStopBatchProcess);
    connect(m_postfixEdit, &QLineEdit::textChanged, this, &BatchProcessor::setOutputPostfix);
    
    // Template controls connections
    connect(m_applyAudioTemplateButton, &QPushButton::clicked, this, &BatchProcessor::onApplyAudioTemplate);
    connect(m_applySubtitleTemplateButton, &QPushButton::clicked, this, &BatchProcessor::onApplySubtitleTemplate);
    connect(m_selectAllAudioButton, &QPushButton::clicked, this, &BatchProcessor::onSelectAllAudio);
    connect(m_selectAllSubtitleButton, &QPushButton::clicked, this, &BatchProcessor::onSelectAllSubtitles);
    connect(m_clearAudioButton, &QPushButton::clicked, this, &BatchProcessor::onClearAudioSelection);
    connect(m_clearSubtitleButton, &QPushButton::clicked, this, &BatchProcessor::onClearSubtitleSelection);
}

void BatchProcessor::setOutputPostfix(const QString &postfix)
{
    m_currentPostfix = postfix;
    if (m_postfixEdit->text() != postfix) {
        m_postfixEdit->setText(postfix);
    }
}

void BatchProcessor::onSelectSourceDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Source Directory");
    if (!dir.isEmpty()) {
        m_sourceDirectoryEdit->setText(dir);
        updateFileList();
    }
}

void BatchProcessor::onSelectTargetDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Target Directory");
    if (!dir.isEmpty()) {
        m_targetDirectoryEdit->setText(dir);
        updateFileList();
    }
}

void BatchProcessor::onSelectOutputDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory");
    if (!dir.isEmpty()) {
        m_outputDirectoryEdit->setText(dir);
    }
}

void BatchProcessor::updateFileList()
{
    m_sourceFilesList->clear();
    m_targetFilesList->clear();
    m_audioTracksList->clear();
    m_subtitleTracksList->clear();
    
    QStringList videoExtensions = {"*.mp4", "*.avi", "*.mkv", "*.mov", "*.wmv", "*.flv", "*.webm", "*.m4v"};
    
    // Load source files
    QString sourceDir = m_sourceDirectoryEdit->text();
    if (!sourceDir.isEmpty()) {
        QDir dir(sourceDir);
        QStringList sourceFiles = dir.entryList(videoExtensions, QDir::Files, QDir::Name);
        m_sourceFilesList->addItems(sourceFiles);
        
        // Load track info from first source file
        if (!sourceFiles.isEmpty()) {
            QString firstSourceFile = dir.absoluteFilePath(sourceFiles.first());
            FFmpegHandler handler;
            
            // Load audio tracks with checkboxes and colored icons
            QList<AudioTrackInfo> audioTracks = handler.getAudioTracks(firstSourceFile);
            QIcon sourceIcon = createColoredIcon(QColor("#4CAF50"), 20); // Green for source
            
            for (const AudioTrackInfo &track : audioTracks) {
                QListWidgetItem *item = new QListWidgetItem();
                item->setText(QString("Track %1: %2 [%3] - %4 (%5 ch, %6 Hz)")
                             .arg(track.index)
                             .arg(track.title)
                             .arg(track.language.toUpper())
                             .arg(track.codec.toUpper())
                             .arg(track.channels)
                             .arg(track.sampleRate));
                item->setIcon(sourceIcon);
                item->setCheckState(Qt::Unchecked);
                item->setData(Qt::UserRole, track.index);
                item->setData(Qt::UserRole + 1, track.language); // For template matching
                item->setData(Qt::UserRole + 2, track.codec); // For template matching
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                m_audioTracksList->addItem(item);
            }
            
            // Load subtitle tracks with checkboxes and colored icons
            QList<SubtitleTrackInfo> subtitleTracks = handler.getSubtitleTracks(firstSourceFile);
            for (const SubtitleTrackInfo &track : subtitleTracks) {
                QListWidgetItem *item = new QListWidgetItem();
                item->setText(QString("Track %1: %2 [%3] - %4")
                             .arg(track.index)
                             .arg(track.title)
                             .arg(track.language.toUpper())
                             .arg(track.codec.toUpper()));
                item->setIcon(sourceIcon);
                item->setCheckState(Qt::Unchecked);
                item->setData(Qt::UserRole, track.index);
                item->setData(Qt::UserRole + 1, track.language); // For template matching
                item->setData(Qt::UserRole + 2, track.codec); // For template matching
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                m_subtitleTracksList->addItem(item);
            }
        }
    }
    
    // Load target files
    QString targetDir = m_targetDirectoryEdit->text();
    if (!targetDir.isEmpty()) {
        QDir dir(targetDir);
        QStringList targetFiles = dir.entryList(videoExtensions, QDir::Files, QDir::Name);
        m_targetFilesList->addItems(targetFiles);
    }
    
    // Auto-match if both directories are populated
    if (!sourceDir.isEmpty() && !targetDir.isEmpty()) {
        onAutoMatch();
    }
}

void BatchProcessor::onAutoMatch()
{
    matchFiles();
}

void BatchProcessor::matchFiles()
{
    // Simple name-based matching algorithm
    QStringList sourceFiles;
    QStringList targetFiles;
    
    for (int i = 0; i < m_sourceFilesList->count(); ++i) {
        sourceFiles.append(m_sourceFilesList->item(i)->text());
    }
    
    for (int i = 0; i < m_targetFilesList->count(); ++i) {
        targetFiles.append(m_targetFilesList->item(i)->text());
    }
    
    // Clear target list and re-add in matched order
    m_targetFilesList->clear();
    
    QStringList matchedTargets;
    QStringList unmatchedTargets = targetFiles;
    
    // Try to match based on similar names
    for (const QString &sourceFile : sourceFiles) {
        QFileInfo sourceInfo(sourceFile);
        QString sourceBaseName = sourceInfo.baseName().toLower();
        
        QString bestMatch;
        int bestScore = 0;
        
        for (const QString &targetFile : unmatchedTargets) {
            QFileInfo targetInfo(targetFile);
            QString targetBaseName = targetInfo.baseName().toLower();
            
            // Calculate similarity score (simple substring matching)
            int score = 0;
            QStringList sourceWords = sourceBaseName.split(QRegularExpression("[\\s\\-_\\.]"), Qt::SkipEmptyParts);
            QStringList targetWords = targetBaseName.split(QRegularExpression("[\\s\\-_\\.]"), Qt::SkipEmptyParts);
            
            for (const QString &sourceWord : sourceWords) {
                for (const QString &targetWord : targetWords) {
                    if (sourceWord == targetWord) {
                        score += sourceWord.length();
                    }
                }
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestMatch = targetFile;
            }
        }
        
        if (!bestMatch.isEmpty()) {
            matchedTargets.append(bestMatch);
            unmatchedTargets.removeOne(bestMatch);
        } else {
            matchedTargets.append(""); // No match found
        }
    }
    
    // Add remaining unmatched targets at the end
    matchedTargets.append(unmatchedTargets);
    
    // Update target list
    for (const QString &target : matchedTargets) {
        if (!target.isEmpty()) {
            m_targetFilesList->addItem(target);
        } else {
            QListWidgetItem *item = new QListWidgetItem("(No Match)");
            item->setForeground(QBrush(Qt::red));
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            m_targetFilesList->addItem(item);
        }
    }
}

void BatchProcessor::onMoveUp()
{
    int currentRow = m_targetFilesList->currentRow();
    if (currentRow > 0) {
        QListWidgetItem *item = m_targetFilesList->takeItem(currentRow);
        m_targetFilesList->insertItem(currentRow - 1, item);
        m_targetFilesList->setCurrentRow(currentRow - 1);
    }
}

void BatchProcessor::onMoveDown()
{
    int currentRow = m_targetFilesList->currentRow();
    if (currentRow >= 0 && currentRow < m_targetFilesList->count() - 1) {
        QListWidgetItem *item = m_targetFilesList->takeItem(currentRow);
        m_targetFilesList->insertItem(currentRow + 1, item);
        m_targetFilesList->setCurrentRow(currentRow + 1);
    }
}

void BatchProcessor::onStartBatchProcess()
{
    QString sourceDir = m_sourceDirectoryEdit->text();
    QString targetDir = m_targetDirectoryEdit->text();
    QString outputDir = m_outputDirectoryEdit->text();
    
    if (sourceDir.isEmpty() || targetDir.isEmpty() || outputDir.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select all directories.");
        return;
    }
    
    // Get checked tracks from source (tracks to ADD)
    QList<int> sourceAudioTrackIndexes;
    QList<int> sourceSubtitleTrackIndexes;
    
    for (int i = 0; i < m_audioTracksList->count(); ++i) {
        QListWidgetItem *item = m_audioTracksList->item(i);
        if (item->checkState() == Qt::Checked) {
            sourceAudioTrackIndexes.append(item->data(Qt::UserRole).toInt());
        }
    }
    
    for (int i = 0; i < m_subtitleTracksList->count(); ++i) {
        QListWidgetItem *item = m_subtitleTracksList->item(i);
        if (item->checkState() == Qt::Checked) {
            sourceSubtitleTrackIndexes.append(item->data(Qt::UserRole).toInt());
        }
    }
    
    bool removeExistingTracks = m_removeExistingTracksCheckbox->isChecked();
    
    // If not removing existing tracks and no source tracks selected, warn user
    if (!removeExistingTracks && sourceAudioTrackIndexes.isEmpty() && sourceSubtitleTrackIndexes.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please check at least one source track to add, or enable 'Remove existing tracks' if you want to strip tracks.");
        return;
    }
    
    // Prepare file lists
    QStringList sourceFiles;
    QStringList targetFiles;
    
    QDir sourceDirObj(sourceDir);
    QDir targetDirObj(targetDir);
    
    for (int i = 0; i < qMin(m_sourceFilesList->count(), m_targetFilesList->count()); ++i) {
        QString sourceFile = m_sourceFilesList->item(i)->text();
        QString targetFile = m_targetFilesList->item(i)->text();
        
        if (targetFile != "(No Match)") {
            sourceFiles.append(sourceDirObj.absoluteFilePath(sourceFile));
            targetFiles.append(targetDirObj.absoluteFilePath(targetFile));
        }
    }
    
    if (sourceFiles.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No valid file pairs to process.");
        return;
    }
    
    // Start processing
    m_processingCancelled = false;
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_progressBar->setRange(0, sourceFiles.size());
    m_progressBar->setValue(0);
    m_logOutput->clear();
    
    FFmpegHandler handler;
    
    for (int i = 0; i < sourceFiles.size() && !m_processingCancelled; ++i) {
        QString logMessage = QString("Processing %1/%2: %3")
                            .arg(i + 1)
                            .arg(sourceFiles.size())
                            .arg(QFileInfo(targetFiles[i]).fileName());
        m_logOutput->append(logMessage);
        QApplication::processEvents();
        
        // Check for cancellation after processing events
        if (m_processingCancelled) {
            m_logOutput->append("⚠️ Processing cancelled by user");
            break;
        }
        
        QFileInfo targetInfo(targetFiles[i]);
        QString outputFile = QDir(outputDir).absoluteFilePath(
            targetInfo.baseName() + m_currentPostfix + "." + targetInfo.suffix());
        
        // Build track selection list for merge operation
        QList<QPair<QString, int>> selectedAudioTracks;
        QList<QPair<QString, int>> selectedSubtitleTracks;
        
        // Add selected source tracks (tracks to ADD)
        for (int trackIndex : sourceAudioTrackIndexes) {
            selectedAudioTracks.append(qMakePair(QString("source"), trackIndex));
        }
        for (int trackIndex : sourceSubtitleTrackIndexes) {
            selectedSubtitleTracks.append(qMakePair(QString("source"), trackIndex));
        }
        
        // Add existing target tracks (unless user wants to remove them)
        if (!removeExistingTracks) {
            // Get existing tracks from target file
            QList<AudioTrackInfo> targetAudioTracks = handler.getAudioTracks(targetFiles[i]);
            for (const AudioTrackInfo &track : targetAudioTracks) {
                selectedAudioTracks.append(qMakePair(QString("target"), track.index));
            }
            
            QList<SubtitleTrackInfo> targetSubtitleTracks = handler.getSubtitleTracks(targetFiles[i]);
            for (const SubtitleTrackInfo &track : targetSubtitleTracks) {
                selectedSubtitleTracks.append(qMakePair(QString("target"), track.index));
            }
        }
        
        // Use new merge operation
        bool success = handler.mergeTracks(sourceFiles[i], targetFiles[i], outputFile,
                                          selectedAudioTracks, selectedSubtitleTracks);
        
        if (success) {
            m_logOutput->append("✓ Success - tracks merged");
        } else {
            m_logOutput->append("✗ Failed");
        }
        
        m_progressBar->setValue(i + 1);
        QApplication::processEvents();
        
        // Final cancellation check
        if (m_processingCancelled) {
            break;
        }
    }
    
    // Reset button states
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    
    if (m_processingCancelled) {
        m_logOutput->append("\n❌ Batch processing was cancelled!");
        QMessageBox::information(this, "Cancelled", 
                                QString("Batch processing was cancelled!\nProcessed %1 out of %2 files.")
                                .arg(m_progressBar->value())
                                .arg(sourceFiles.size()));
    } else {
        m_logOutput->append("\n✅ Batch processing completed!");
        QMessageBox::information(this, "Completed", 
                                QString("Batch processing completed!\nProcessed %1 files.")
                                .arg(sourceFiles.size()));
    }
}

void BatchProcessor::onStopBatchProcess()
{
    m_processingCancelled = true;
    m_stopButton->setEnabled(false);  // Disable to prevent multiple clicks
    m_logOutput->append("🛑 Stopping batch processing...");
    QApplication::processEvents(); // Process the UI update immediately
}

QIcon BatchProcessor::createColoredIcon(const QColor &color, int size)
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

void BatchProcessor::onApplyAudioTemplate()
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

void BatchProcessor::onApplySubtitleTemplate()
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

void BatchProcessor::onSelectAllAudio()
{
    for (int i = 0; i < m_audioTracksList->count(); ++i) {
        m_audioTracksList->item(i)->setCheckState(Qt::Checked);
    }
}

void BatchProcessor::onSelectAllSubtitles()
{
    for (int i = 0; i < m_subtitleTracksList->count(); ++i) {
        m_subtitleTracksList->item(i)->setCheckState(Qt::Checked);
    }
}

void BatchProcessor::onClearAudioSelection()
{
    for (int i = 0; i < m_audioTracksList->count(); ++i) {
        m_audioTracksList->item(i)->setCheckState(Qt::Unchecked);
    }
}

void BatchProcessor::onClearSubtitleSelection()
{
    for (int i = 0; i < m_subtitleTracksList->count(); ++i) {
        m_subtitleTracksList->item(i)->setCheckState(Qt::Unchecked);
    }
}