#ifndef TRANSFERWORKER_H
#define TRANSFERWORKER_H

#include <QObject>
#include <QPair>
#include <QList>

class TransferWorker : public QObject
{
    Q_OBJECT

public:
    explicit TransferWorker(QObject *parent = nullptr);
    
    void setTransferJob(const QString &sourceFile,
                       const QString &targetFile,
                       const QString &outputFile,
                       const QList<QPair<QString, int>> &selectedAudioTracks,
                       const QList<QPair<QString, int>> &selectedSubtitleTracks);

public slots:
    void startTransfer();

signals:
    void transferCompleted(bool success, const QString &message);
    void logMessage(const QString &message);

private:
    QString m_sourceFile;
    QString m_targetFile;
    QString m_outputFile;
    QList<QPair<QString, int>> m_selectedAudioTracks;
    QList<QPair<QString, int>> m_selectedSubtitleTracks;
};

#endif // TRANSFERWORKER_H