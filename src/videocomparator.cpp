#include "videocomparator.h"
#include "ffmpeghandler.h"
#include <QDebug>

VideoComparator::VideoComparator(QObject *parent)
    : QObject(parent)
    , m_comparisonTimer(new QTimer(this))
    , m_isComparing(false)
    , m_currentTimestamp(0)
    , m_videoDuration(0)
{
    connect(m_comparisonTimer, &QTimer::timeout, this, &VideoComparator::performFrameComparison);
    m_comparisonTimer->setInterval(100); // Compare every 100ms during playback
}

void VideoComparator::setVideo(int index, const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    if (index == 0) {
        m_videoPath1 = filePath;
    } else if (index == 1) {
        m_videoPath2 = filePath;
    }
    
    // If both videos are loaded, get duration for comparison progress
    if (!m_videoPath1.isEmpty() && !m_videoPath2.isEmpty()) {
        FFmpegHandler handler;
        m_videoDuration = handler.getVideoDuration(m_videoPath1);
    }
}

void VideoComparator::startComparison()
{
    if (m_videoPath1.isEmpty() || m_videoPath2.isEmpty()) {
        qWarning() << "Cannot start comparison: both videos must be loaded";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    m_isComparing = true;
    m_currentTimestamp = 0;
    m_results.clear();
    
    m_comparisonTimer->start();
}

void VideoComparator::stopComparison()
{
    QMutexLocker locker(&m_mutex);
    m_isComparing = false;
    m_comparisonTimer->stop();
    
    if (!m_results.isEmpty()) {
        emit comparisonComplete(m_results);
    }
}

void VideoComparator::performFrameComparison()
{
    if (!m_isComparing) {
        return;
    }
    
    double similarity = compareFramesAtTimestamp(m_currentTimestamp);
    
    ComparisonResult result;
    result.similarity = similarity;
    result.timestamp = m_currentTimestamp;
    result.description = QString("Similarity: %1%").arg(similarity * 100, 0, 'f', 2);
    
    m_results.append(result);
    emit frameCompared(m_currentTimestamp, similarity);
    
    // Update progress
    if (m_videoDuration > 0) {
        int progress = (m_currentTimestamp * 100) / m_videoDuration;
        emit comparisonProgress(progress);
    }
    
    // Move to next timestamp (1 second intervals for full comparison)
    m_currentTimestamp += 1000;
    
    if (m_currentTimestamp >= m_videoDuration) {
        stopComparison();
    }
}

double VideoComparator::compareFramesAtTimestamp(qint64 timestamp)
{
    FFmpegHandler handler;
    
    // Extract frames from both videos at the given timestamp
    QImage frame1 = handler.extractFrame(m_videoPath1, timestamp);
    QImage frame2 = handler.extractFrame(m_videoPath2, timestamp);
    
    if (frame1.isNull() || frame2.isNull()) {
        return 0.0; // Cannot compare null frames
    }
    
    // Resize frames to same size for comparison
    QSize compareSize(320, 240);
    frame1 = frame1.scaled(compareSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    frame2 = frame2.scaled(compareSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    
    // Calculate similarity using pixel-by-pixel comparison
    double totalPixels = compareSize.width() * compareSize.height();
    double similarPixels = 0.0;
    
    for (int y = 0; y < compareSize.height(); ++y) {
        for (int x = 0; x < compareSize.width(); ++x) {
            QRgb pixel1 = frame1.pixel(x, y);
            QRgb pixel2 = frame2.pixel(x, y);
            
            // Calculate color difference
            int rDiff = qRed(pixel1) - qRed(pixel2);
            int gDiff = qGreen(pixel1) - qGreen(pixel2);
            int bDiff = qBlue(pixel1) - qBlue(pixel2);
            
            double colorDistance = sqrt(rDiff*rDiff + gDiff*gDiff + bDiff*bDiff);
            double maxDistance = sqrt(3 * 255 * 255); // Maximum possible color distance
            
            double pixelSimilarity = 1.0 - (colorDistance / maxDistance);
            similarPixels += pixelSimilarity;
        }
    }
    
    return similarPixels / totalPixels;
}