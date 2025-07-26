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
    
    struct ComparisonResult {
        double similarity;
        qint64 timestamp;
        QString description;
    };

signals:
    void comparisonProgress(int percentage);
    void comparisonComplete(const QList<ComparisonResult> &results);
    void frameCompared(qint64 timestamp, double similarity);

private slots:
    void performFrameComparison();

private:
    QString m_videoPath1;
    QString m_videoPath2;
    QTimer *m_comparisonTimer;
    QMutex m_mutex;
    bool m_isComparing;
    qint64 m_currentTimestamp;
    qint64 m_videoDuration;
    
    double compareFramesAtTimestamp(qint64 timestamp);
    QList<ComparisonResult> m_results;
};

#endif // VIDEOCOMPARATOR_H