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

class VideoWidget;
class VideoComparator;
class BatchProcessor;

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

private:
    void setupUI();
    void setupComparisonTab();
    void setupTransferTab();
    void setupBatchTab();
    void updateTrackLists();
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
};

#endif // MAINWINDOW_H