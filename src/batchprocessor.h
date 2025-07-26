#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>

class BatchProcessor : public QWidget
{
    Q_OBJECT

public:
    explicit BatchProcessor(QWidget *parent = nullptr);
    
    void setOutputPostfix(const QString &postfix);
    void applyTheme();

signals:
    void batchProcessRequested();

private slots:
    void onSelectSourceDirectory();
    void onSelectTargetDirectory();
    void onSelectOutputDirectory();
    void onStartBatchProcess();
    void onStopBatchProcess();
    void onMoveUp();
    void onMoveDown();
    void onAutoMatch();
    
    // Template and selection slots
    void onApplyAudioTemplate();
    void onApplySubtitleTemplate();
    void onSelectAllAudio();
    void onSelectAllSubtitles();
    void onClearAudioSelection();
    void onClearSubtitleSelection();
    void onThemeChanged();

private:
    void setupUI();
    void updateFileList();
    void matchFiles();
    QIcon createColoredIcon(const QColor &color, int size = 16);
    
    QVBoxLayout *m_mainLayout;
    
    // Directory selection
    QLineEdit *m_sourceDirectoryEdit;
    QLineEdit *m_targetDirectoryEdit;
    QLineEdit *m_outputDirectoryEdit;
    QPushButton *m_selectSourceButton;
    QPushButton *m_selectTargetButton;
    QPushButton *m_selectOutputButton;
    
    // File lists and reordering
    QListWidget *m_sourceFilesList;
    QListWidget *m_targetFilesList;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    QPushButton *m_autoMatchButton;
    
    // Track selection with templates (like main Transfer tab)
    QLineEdit *m_audioTemplateEdit;
    QLineEdit *m_subtitleTemplateEdit;
    QPushButton *m_applyAudioTemplateButton;
    QPushButton *m_applySubtitleTemplateButton;
    QPushButton *m_selectAllAudioButton;
    QPushButton *m_selectAllSubtitleButton;
    QPushButton *m_clearAudioButton;
    QPushButton *m_clearSubtitleButton;
    QListWidget *m_audioTracksList;
    QListWidget *m_subtitleTracksList;
    
    // Output options
    QLineEdit *m_postfixEdit;
    QCheckBox *m_removeExistingTracksCheckbox;
    
    // Processing
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QProgressBar *m_progressBar;
    QTextEdit *m_logOutput;
    
    QString m_currentPostfix;
    bool m_processingCancelled;
};

#endif // BATCHPROCESSOR_H