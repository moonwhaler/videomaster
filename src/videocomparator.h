#ifndef VIDEOCOMPARATOR_H
#define VIDEOCOMPARATOR_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMap>

class VideoComparator : public QObject
{
    Q_OBJECT

public:
    explicit VideoComparator(QObject *parent = nullptr);
    
    void setVideo(int index, const QString &filePath);
    void setVideoOffset(int index, qint64 offsetMs);
    void startComparison();
    void stopComparison();
    void startAutoComparison();  // New method for automatic comparison
    void findOptimalOffset();    // New method for automatic offset detection
    
    struct ComparisonResult {
        double similarity;
        qint64 timestamp;
        QString description;
    };

signals:
    void comparisonProgress(int percentage);
    void comparisonComplete(const QList<ComparisonResult> &results);
    void frameCompared(qint64 timestamp, double similarity);
    void autoComparisonComplete(double overallSimilarity, bool videosIdentical, const QString &summary);
    void optimalOffsetFound(qint64 optimalOffset, double confidence);

private slots:
    void performFrameComparison();
    void performAutoComparison();
    void performOffsetDetection();

private:
    QString m_videoPath1;
    QString m_videoPath2;
    qint64 m_videoAOffset;  // Offset in milliseconds for Video A
    qint64 m_videoBOffset;  // Offset in milliseconds for Video B
    qint64 m_videoDuration1;  // Duration of Video A
    qint64 m_videoDuration2;  // Duration of Video B
    QTimer *m_comparisonTimer;
    QMutex m_mutex;
    bool m_isComparing;
    bool m_isAutoComparing;
    qint64 m_currentTimestamp;
    qint64 m_videoDuration;
    
    // Auto comparison state
    QList<qint64> m_autoSampleTimestamps;
    int m_currentSampleIndex;
    QList<double> m_autoSimilarityResults;
    
    // Offset detection state
    bool m_isDetectingOffset;
    QList<qint64> m_offsetCandidates;
    int m_currentOffsetIndex;
    QList<qint64> m_offsetTestTimestamps;
    QMap<qint64, double> m_offsetSimilarityMap;
    
    double compareFramesAtTimestamp(qint64 timestamp);
    QList<qint64> generateSampleTimestamps(qint64 duration, int sampleCount = 15);
    double calculateOverallSimilarity(const QList<double> &similarities);
    bool determineIfIdentical(double overallSimilarity, const QList<double> &similarities);
    QList<qint64> generateOffsetCandidates(qint64 maxOffsetMs = 5000, int stepMs = 25);
    double testOffsetSimilarity(qint64 offset, const QList<qint64> &testTimestamps);
    QList<ComparisonResult> m_results;
};

#endif // VIDEOCOMPARATOR_H