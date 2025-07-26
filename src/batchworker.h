#ifndef BATCHWORKER_H
#define BATCHWORKER_H

#include <QObject>
#include <QThread>
#include <QStringList>
#include <QPair>

class BatchWorker : public QObject
{
    Q_OBJECT

public:
    struct ProcessingJob {
        QString sourceFile;
        QString targetFile;
        QString outputFile;
        QList<QPair<QString, int>> selectedAudioTracks;
        QList<QPair<QString, int>> selectedSubtitleTracks;
    };

    explicit BatchWorker(QObject *parent = nullptr);
    
    void setJobs(const QList<ProcessingJob> &jobs);
    void requestStop();

public slots:
    void startProcessing();

signals:
    void progressUpdated(int current, int total, const QString &currentFile);
    void jobCompleted(int jobIndex, bool success, const QString &message);
    void processingFinished(bool cancelled);
    void logMessage(const QString &message);

private:
    QList<ProcessingJob> m_jobs;
    bool m_stopRequested;
    
    bool processJob(const ProcessingJob &job);
};

#endif // BATCHWORKER_H