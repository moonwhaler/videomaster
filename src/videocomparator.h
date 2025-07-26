#ifndef VIDEOCOMPARATOR_H
#define VIDEOCOMPARATOR_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QMutex>

class VideoComparator : public QObject
{
    Q_OBJECT

public:
    explicit VideoComparator(QObject *parent = nullptr);
    
    void setVideo(int index, const QString &filePath);
    void startComparison();
    void stopComparison();
    void startAutoComparison();  // New method for automatic comparison
    
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

private slots:
    void performFrameComparison();
    void performAutoComparison();

private:
    QString m_videoPath1;
    QString m_videoPath2;
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
    
    double compareFramesAtTimestamp(qint64 timestamp);
    QList<qint64> generateSampleTimestamps(qint64 duration, int sampleCount = 15);
    double calculateOverallSimilarity(const QList<double> &similarities);
    bool determineIfIdentical(double overallSimilarity, const QList<double> &similarities);
    QList<ComparisonResult> m_results;
};

#endif // VIDEOCOMPARATOR_H