#include "batchworker.h"
#include "ffmpeghandler.h"
#include <QFileInfo>
#include <QThread>

BatchWorker::BatchWorker(QObject *parent)
    : QObject(parent)
    , m_stopRequested(false)
{
}

void BatchWorker::setJobs(const QList<ProcessingJob> &jobs)
{
    m_jobs = jobs;
}

void BatchWorker::requestStop()
{
    m_stopRequested = true;
}

void BatchWorker::startProcessing()
{
    m_stopRequested = false;
    
    emit logMessage("Starting batch processing...");
    
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_stopRequested) {
            emit logMessage("Processing cancelled by user");
            emit processingFinished(true);
            return;
        }
        
        const ProcessingJob &job = m_jobs[i];
        QString fileName = QFileInfo(job.targetFile).fileName();
        
        emit progressUpdated(i, m_jobs.size(), fileName);
        emit logMessage(QString("Processing %1/%2: %3")
                       .arg(i + 1)
                       .arg(m_jobs.size())
                       .arg(fileName));
        
        bool success = processJob(job);
        
        QString message = success ? "Success - tracks merged" : "Failed";
        emit jobCompleted(i, success, message);
        emit logMessage(message);
        
        // Check for cancellation again after processing
        if (m_stopRequested) {
            emit logMessage("Processing cancelled by user");
            emit processingFinished(true);
            return;
        }
        
        // Small delay to allow UI updates and cancellation checks
        QThread::msleep(10);
    }
    
    emit logMessage("Batch processing completed!");
    emit processingFinished(false);
}

bool BatchWorker::processJob(const ProcessingJob &job)
{
    try {
        FFmpegHandler handler;
        
        // Use the merge operation
        bool success = handler.mergeTracks(
            job.sourceFile,
            job.targetFile, 
            job.outputFile,
            job.selectedAudioTracks,
            job.selectedSubtitleTracks
        );
        
        return success;
    } catch (...) {
        return false;
    }
}