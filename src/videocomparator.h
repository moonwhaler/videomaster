#ifndef VIDEOCOMPARATOR_H
#define VIDEOCOMPARATOR_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QCache>
#include <QImage>
#include <QVector>
#include <memory>

class VideoComparator : public QObject
{
    Q_OBJECT

public:
    explicit VideoComparator(QObject *parent = nullptr);
    ~VideoComparator();
    
    void setVideo(int index, const QString &filePath);
    void setVideoOffset(int index, qint64 offsetMs);
    void startComparison();
    void stopComparison();
    void startAutoComparison();
    void findOptimalOffset();
    
    struct ComparisonResult {
        double similarity;
        qint64 timestamp;
        QString description;
    };
    
    struct FrameInfo {
        QImage image;
        uint64_t perceptualHash;
        QVector<double> colorHistogram;
        double edgeDensity;
        bool isSceneChange;
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
    // Video paths and metadata
    QString m_videoPath1;
    QString m_videoPath2;
    qint64 m_videoAOffset;
    qint64 m_videoBOffset;
    qint64 m_videoDuration1;
    qint64 m_videoDuration2;
    
    // Comparison state
    QTimer *m_comparisonTimer;
    QMutex m_mutex;
    bool m_isComparing;
    bool m_isAutoComparing;
    bool m_isDetectingOffset;
    qint64 m_currentTimestamp;
    qint64 m_videoDuration;
    
    // Frame cache for performance
    QCache<QString, FrameInfo> m_frameCache;
    
    // Auto comparison state
    QList<qint64> m_autoSampleTimestamps;
    int m_currentSampleIndex;
    QList<double> m_autoSimilarityResults;
    
    // Offset detection state
    QList<qint64> m_offsetCandidates;
    int m_currentOffsetIndex;
    QMap<qint64, double> m_offsetSimilarityMap;
    QMap<qint64, FrameInfo> m_cachedFramesVideo1;
    QMap<qint64, FrameInfo> m_cachedFramesVideo2;
    
    // Core comparison methods
    double compareFramesAtTimestamp(qint64 timestamp);
    double compareFrameInfo(const FrameInfo &frame1, const FrameInfo &frame2);
    
    // Perceptual hashing
    uint64_t computePerceptualHash(const QImage &image);
    int hammingDistance(uint64_t hash1, uint64_t hash2);
    
    // Feature extraction
    QVector<double> computeColorHistogram(const QImage &image);
    double computeEdgeDensity(const QImage &image);
    bool isSceneChange(const QImage &prevFrame, const QImage &currentFrame);
    
    // Frame management
    FrameInfo extractFrameInfo(const QString &videoPath, qint64 timestamp);
    FrameInfo getCachedOrExtractFrame(const QString &videoPath, qint64 timestamp);
    void preloadFramesForOffsetDetection();
    
    // Sampling and offset detection
    QList<qint64> generateSmartSampleTimestamps(qint64 duration);
    QList<qint64> detectSceneChanges(const QString &videoPath, qint64 startMs, qint64 endMs);
    QList<qint64> generateHierarchicalOffsetCandidates(qint64 maxOffsetMs);
    double testOffsetWithCache(qint64 offset);
    
    // Statistical analysis
    double calculateOverallSimilarity(const QList<double> &similarities);
    double calculateConfidenceScore(const QMap<qint64, double> &offsetSimilarityMap, qint64 bestOffset);
    bool determineIfIdentical(double overallSimilarity, const QList<double> &similarities);
    
    QList<ComparisonResult> m_results;
};

#endif // VIDEOCOMPARATOR_H
