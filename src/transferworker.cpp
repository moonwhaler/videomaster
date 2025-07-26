#include "transferworker.h"
#include "ffmpeghandler.h"
#include <QFileInfo>

TransferWorker::TransferWorker(QObject *parent)
    : QObject(parent)
{
}

void TransferWorker::setTransferJob(const QString &sourceFile,
                                   const QString &targetFile,
                                   const QString &outputFile,
                                   const QList<QPair<QString, int>> &selectedAudioTracks,
                                   const QList<QPair<QString, int>> &selectedSubtitleTracks)
{
    m_sourceFile = sourceFile;
    m_targetFile = targetFile;
    m_outputFile = outputFile;
    m_selectedAudioTracks = selectedAudioTracks;
    m_selectedSubtitleTracks = selectedSubtitleTracks;
}

void TransferWorker::startTransfer()
{
    QString fileName = QFileInfo(m_targetFile).fileName();
    emit logMessage(QString("Starting track transfer for: %1").arg(fileName));
    
    try {
        FFmpegHandler handler;
        
        bool success = handler.mergeTracks(
            m_sourceFile,
            m_targetFile,
            m_outputFile,
            m_selectedAudioTracks,
            m_selectedSubtitleTracks
        );
        
        if (success) {
            emit logMessage("Track transfer completed successfully!");
            emit transferCompleted(true, QString("Tracks successfully transferred to: %1").arg(QFileInfo(m_outputFile).fileName()));
        } else {
            emit logMessage("Track transfer failed!");
            emit transferCompleted(false, "Track transfer failed. Please check the log for details.");
        }
        
    } catch (...) {
        emit logMessage("Track transfer failed with exception!");
        emit transferCompleted(false, "Track transfer failed due to an unexpected error.");
    }
}