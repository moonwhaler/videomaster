#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QListWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QThread>

class VideoWidget;
class VideoComparator;
class BatchProcessor;
class TransferWorker;

// Forward declaration for ComparisonResult
struct ComparisonResult {
    double similarity;
    qint64 timestamp;
    QString description;
};

// Forward declaration for ChapterInfo (defined in ffmpeghandler.h)
struct ChapterInfo;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onVideoLoaded(int playerIndex, const QString &filePath);
    void onSyncPlayback();
    void onSeekToTimestamp();
    void onVideoPositionChanged(qint64 position);
    void onTransferTracks();
    void onBatchProcess();
    void onPostfixChanged();
    
    // Track selection slots
    void onApplyAudioTemplate();
    void onApplySubtitleTemplate();
    void onSelectAllAudio();
    void onSelectAllSubtitles();
    void onClearAudioSelection();
    void onClearSubtitleSelection();
    
    // Theme slots
    void onThemeChanged();
    void onSystemThemeTriggered();
    void onLightThemeTriggered();
    void onDarkThemeTriggered();
    
    // Transfer worker slots
    void onTransferCompleted(bool success, const QString &message);
    void onTransferLogMessage(const QString &message);
    
    // Auto comparison slots
    void onAutoCompare();
    void onAutoOffset();
    void onOptimalOffsetFound(qint64 optimalOffset, double confidence);
    void onComparisonProgress(int percentage);
    void onComparisonComplete(const QList<ComparisonResult> &results);
    void onAutoComparisonComplete(double overallSimilarity, bool videosIdentical, const QString &summary);
    
    // Chapter navigation slots
    void onChapterSelected();
    void onPreviousChapter();
    void onNextChapter();
    void updateChapterLists();
    void jumpToCurrentChapter();
    void updateCurrentChapterDisplay(qint64 currentTimestamp);
    void updateChapterNavigation();

private:
    void setupUI();
    void setupMenuBar();
    void setupComparisonTab();
    void setupTransferTab();
    void setupBatchTab();
    void updateTrackLists();
    void applyTheme();
    void refreshTabStyling();
    QIcon createColoredIcon(const QColor &color, int size = 16);

    QTabWidget *m_tabWidget;
    
    // Comparison tab
    QWidget *m_comparisonTab;
    VideoWidget *m_leftVideoWidget;
    VideoWidget *m_rightVideoWidget;
    QPushButton *m_syncButton;
    QSlider *m_timestampSlider;
    QLabel *m_timestampLabel;
    VideoComparator *m_comparator;
    bool m_isPlaying;
    
    // Auto comparison controls
    QPushButton *m_autoCompareButton;
    QPushButton *m_autoOffsetButton;
    QProgressBar *m_comparisonProgressBar;
    QLabel *m_comparisonResultLabel;
    
    // Relative offset control
    QSpinBox *m_relativeOffsetSpinBox;
    
    // Chapter navigation controls
    QListWidget *m_leftChaptersList;
    QListWidget *m_rightChaptersList;
    QPushButton *m_prevChapterButton;
    QPushButton *m_nextChapterButton;
    QLabel *m_currentChapterLabel;
    
    // Chapter data
    QList<ChapterInfo> m_leftVideoChapters;
    QList<ChapterInfo> m_rightVideoChapters;
    int m_currentChapterIndex;
    
    // Transfer tab
    QWidget *m_transferTab;
    VideoWidget *m_sourceVideoWidget;
    VideoWidget *m_targetVideoWidget;
    QPushButton *m_transferButton;
    QLineEdit *m_postfixEdit;
    QListWidget *m_audioTracksList;
    QListWidget *m_subtitleTracksList;
    
    // Track selection templates
    QLineEdit *m_audioTemplateEdit;
    QLineEdit *m_subtitleTemplateEdit;
    QPushButton *m_applyAudioTemplateButton;
    QPushButton *m_applySubtitleTemplateButton;
    QPushButton *m_selectAllAudioButton;
    QPushButton *m_selectAllSubtitleButton;
    QPushButton *m_clearAudioButton;
    QPushButton *m_clearSubtitleButton;
    
    // Batch tab
    QWidget *m_batchTab;
    BatchProcessor *m_batchProcessor;
    
    // Theme menu
    QMenu *m_viewMenu;
    QMenu *m_themeMenu;
    QActionGroup *m_themeActionGroup;
    QAction *m_systemThemeAction;
    QAction *m_lightThemeAction;
    QAction *m_darkThemeAction;
    
    // Threading for transfer operations
    QThread *m_transferThread;
    TransferWorker *m_transferWorker;
};

#endif // MAINWINDOW_H