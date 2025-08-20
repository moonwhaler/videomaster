#include "videocomparator.h"
#include "ffmpeghandler.h"
#include <QDebug>
#include <algorithm>
#include <cstdlib>
#include <random>

VideoComparator::VideoComparator(QObject *parent)
    : QObject(parent)
    , m_videoAOffset(0)
    , m_videoBOffset(0)
    , m_videoDuration1(0)
    , m_videoDuration2(0)
    , m_comparisonTimer(new QTimer(this))
    , m_isComparing(false)
    , m_isAutoComparing(false)
    , m_currentTimestamp(0)
    , m_videoDuration(0)
    , m_currentSampleIndex(0)
    , m_isDetectingOffset(false)
    , m_currentOffsetIndex(0)
{
    connect(m_comparisonTimer, &QTimer::timeout, this, &VideoComparator::performFrameComparison);
    m_comparisonTimer->setInterval(100); // Compare every 100ms during playback
}

void VideoComparator::setVideo(int index, const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    FFmpegHandler handler;
    
    if (index == 0) {
        m_videoPath1 = filePath;
        m_videoDuration1 = handler.getVideoDuration(filePath);
    } else if (index == 1) {
        m_videoPath2 = filePath;
        m_videoDuration2 = handler.getVideoDuration(filePath);
    }
    
    // If both videos are loaded, set duration for comparison progress
    // Use the shorter duration to avoid going beyond either video
    if (!m_videoPath1.isEmpty() && !m_videoPath2.isEmpty()) {
        m_videoDuration = qMin(m_videoDuration1, m_videoDuration2);
        qDebug() << "Video durations - A:" << m_videoDuration1 << "ms, B:" << m_videoDuration2 << "ms";
        qDebug() << "Using comparison duration:" << m_videoDuration << "ms";
    }
}

void VideoComparator::setVideoOffset(int index, qint64 offsetMs)
{
    QMutexLocker locker(&m_mutex);
    
    if (index == 0) {
        m_videoAOffset = offsetMs;
    } else if (index == 1) {
        m_videoBOffset = offsetMs;
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
    
    // Apply offsets to timestamps
    qint64 timestamp1 = timestamp + m_videoAOffset;
    qint64 timestamp2 = timestamp + m_videoBOffset;
    
    // Ensure timestamps are within valid bounds
    timestamp1 = qMax(0LL, qMin(timestamp1, m_videoDuration1 - 1));
    timestamp2 = qMax(0LL, qMin(timestamp2, m_videoDuration2 - 1));
    
    // Debug logging to show actual timestamps being used
    if (m_videoAOffset != 0 || m_videoBOffset != 0) {
        qDebug() << "Comparing frames at original:" << timestamp << "ms";
        qDebug() << "  Video A: offset" << m_videoAOffset << "ms -> timestamp" << timestamp1 << "ms";
        qDebug() << "  Video B: offset" << m_videoBOffset << "ms -> timestamp" << timestamp2 << "ms";
    }
    
    // Extract frames from both videos at the adjusted timestamps
    QImage frame1 = handler.extractFrame(m_videoPath1, timestamp1);
    QImage frame2 = handler.extractFrame(m_videoPath2, timestamp2);
    
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

void VideoComparator::startAutoComparison()
{
    if (m_videoPath1.isEmpty() || m_videoPath2.isEmpty()) {
        qWarning() << "Cannot start auto comparison: both videos must be loaded";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_isComparing || m_isAutoComparing) {
        qWarning() << "Comparison already in progress";
        return;
    }
    
    m_isAutoComparing = true;
    m_currentSampleIndex = 0;
    m_autoSimilarityResults.clear();
    
    // Generate strategic sampling timestamps
    // Calculate effective duration considering offsets
    qint64 effectiveStartTime = qMax(0LL, qMax(-m_videoAOffset, -m_videoBOffset));
    qint64 effectiveEndTime = qMin(m_videoDuration1 - m_videoAOffset, m_videoDuration2 - m_videoBOffset);
    qint64 effectiveDuration = qMax(0LL, effectiveEndTime - effectiveStartTime);
    
    qDebug() << "Offset-aware duration calculation:";
    qDebug() << "  Video A offset:" << m_videoAOffset << "ms, duration:" << m_videoDuration1 << "ms";
    qDebug() << "  Video B offset:" << m_videoBOffset << "ms, duration:" << m_videoDuration2 << "ms";
    qDebug() << "  Effective start:" << effectiveStartTime << "ms, end:" << effectiveEndTime << "ms";
    qDebug() << "  Effective duration:" << effectiveDuration << "ms";
    
    if (effectiveDuration <= 0) {
        qWarning() << "Invalid effective duration due to offsets. Cannot perform comparison.";
        m_isAutoComparing = false;
        emit autoComparisonComplete(0.0, false, "Cannot compare: offsets result in no overlapping video content.");
        return;
    }
    
    m_autoSampleTimestamps = generateSampleTimestamps(effectiveDuration);
    // Adjust timestamps to start from effective start time
    for (int i = 0; i < m_autoSampleTimestamps.size(); ++i) {
        m_autoSampleTimestamps[i] += effectiveStartTime;
    }
    
    qDebug() << "Starting auto comparison with" << m_autoSampleTimestamps.size() << "samples";
    
    // Start the auto comparison process
    QTimer::singleShot(0, this, &VideoComparator::performAutoComparison);
}

void VideoComparator::performAutoComparison()
{
    if (!m_isAutoComparing || m_currentSampleIndex >= m_autoSampleTimestamps.size()) {
        // Comparison complete - calculate results
        if (!m_autoSimilarityResults.isEmpty()) {
            double overallSimilarity = calculateOverallSimilarity(m_autoSimilarityResults);
            bool identical = determineIfIdentical(overallSimilarity, m_autoSimilarityResults);
            
            QString summary = QString("Analyzed %1 frames across video duration.\n"
                                    "Average similarity: %2%\n"
                                    "Verdict: Videos are %3")
                            .arg(m_autoSimilarityResults.size())
                            .arg(overallSimilarity * 100, 0, 'f', 1)
                            .arg(identical ? "IDENTICAL" : "DIFFERENT");
            
            qDebug() << "Auto comparison complete:" << summary;
            emit autoComparisonComplete(overallSimilarity, identical, summary);
        }
        
        m_isAutoComparing = false;
        return;
    }
    
    // Compare current sample
    qint64 timestamp = m_autoSampleTimestamps[m_currentSampleIndex];
    double similarity = compareFramesAtTimestamp(timestamp);
    m_autoSimilarityResults.append(similarity);
    
    qDebug() << "Sample" << (m_currentSampleIndex + 1) << "at" << timestamp << "ms: similarity" << (similarity * 100) << "%";
    
    // Update progress
    int progress = ((m_currentSampleIndex + 1) * 100) / m_autoSampleTimestamps.size();
    emit comparisonProgress(progress);
    
    m_currentSampleIndex++;
    
    // Schedule next comparison (small delay to allow UI updates)
    QTimer::singleShot(50, this, &VideoComparator::performAutoComparison);
}

QList<qint64> VideoComparator::generateSampleTimestamps(qint64 duration, int sampleCount)
{
    QList<qint64> timestamps;
    
    if (duration <= 0 || sampleCount <= 0) {
        return timestamps;
    }
    
    // Optimized for early video content analysis (first quarter only)
    // Skip very beginning to avoid black frames/logos
    qint64 startTime = 2000;  // Start at 2 seconds
    qint64 endTime = qMin(duration - 1000, duration);  // Safe end time
    
    if (endTime <= startTime) {
        // Very short segment
        timestamps.append((startTime + endTime) / 2);
        return timestamps;
    }
    
    // Strategic early content sampling:
    
    // 1. Early content samples (where sync differences are most obvious)
    timestamps.append(startTime);              // 2 seconds
    timestamps.append(startTime + 3000);       // 5 seconds  
    timestamps.append(startTime + 8000);       // 10 seconds
    timestamps.append(startTime + 15000);      // 17 seconds
    
    // 2. Mid-section samples (structural content)
    timestamps.append(duration / 3);           // One third
    timestamps.append(duration / 2);           // Half way
    timestamps.append(2 * duration / 3);       // Two thirds
    
    // 3. Additional evenly distributed samples
    int remainingSamples = sampleCount - timestamps.size();
    if (remainingSamples > 0) {
        qint64 step = (endTime - startTime) / (remainingSamples + 1);
        for (int i = 1; i <= remainingSamples; ++i) {
            qint64 timestamp = startTime + i * step;
            timestamps.append(timestamp);
        }
    }
    
    // Sort and remove duplicates
    std::sort(timestamps.begin(), timestamps.end());
    timestamps.erase(std::unique(timestamps.begin(), timestamps.end()), timestamps.end());
    
    // Ensure all timestamps are valid
    QList<qint64> validTimestamps;
    for (qint64 ts : timestamps) {
        if (ts >= startTime && ts <= endTime) {
            validTimestamps.append(ts);
        }
    }
    
    return validTimestamps;
}

double VideoComparator::calculateOverallSimilarity(const QList<double> &similarities)
{
    if (similarities.isEmpty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    for (double sim : similarities) {
        sum += sim;
    }
    
    return sum / similarities.size();
}

bool VideoComparator::determineIfIdentical(double overallSimilarity, const QList<double> &similarities)
{
    // Videos are considered identical if:
    // 1. Overall similarity is above 95%
    // 2. No individual frame has similarity below 90%
    // 3. At least 80% of frames have similarity above 98%
    
    const double OVERALL_THRESHOLD = 0.95;
    const double MIN_FRAME_THRESHOLD = 0.90;
    const double HIGH_SIMILARITY_THRESHOLD = 0.98;
    const double HIGH_SIMILARITY_RATIO = 0.80;
    
    if (overallSimilarity < OVERALL_THRESHOLD) {
        return false;
    }
    
    int highSimilarityCount = 0;
    for (double sim : similarities) {
        if (sim < MIN_FRAME_THRESHOLD) {
            return false; // Any frame too different
        }
        if (sim >= HIGH_SIMILARITY_THRESHOLD) {
            highSimilarityCount++;
        }
    }
    
    double highSimilarityRatio = static_cast<double>(highSimilarityCount) / similarities.size();
    return highSimilarityRatio >= HIGH_SIMILARITY_RATIO;
}

void VideoComparator::findOptimalOffset()
{
    if (m_videoPath1.isEmpty() || m_videoPath2.isEmpty()) {
        qWarning() << "Cannot find optimal offset: both videos must be loaded";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_isComparing || m_isAutoComparing || m_isDetectingOffset) {
        qWarning() << "Another operation is already in progress";
        return;
    }
    
    m_isDetectingOffset = true;
    m_currentOffsetIndex = 0;
    m_offsetSimilarityMap.clear();
    
    // Fast, precise detection: ±5 seconds in 25ms steps
    m_offsetCandidates = generateOffsetCandidates(5000, 25);
    
    // Focus on first quarter of video for speed and reliability
    qint64 minDuration = qMin(m_videoDuration1, m_videoDuration2);
    qint64 searchDuration = minDuration / 4; // Only first quarter
    m_offsetTestTimestamps = generateSampleTimestamps(searchDuration, 10);
    
    qDebug() << "Starting offset detection with" << m_offsetCandidates.size() 
             << "offset candidates and" << m_offsetTestTimestamps.size() << "test timestamps";
    
    // Start the offset detection process
    QTimer::singleShot(0, this, &VideoComparator::performOffsetDetection);
}

void VideoComparator::performOffsetDetection()
{
    if (!m_isDetectingOffset || m_currentOffsetIndex >= m_offsetCandidates.size()) {
        // Detection complete - find the best offset
        if (!m_offsetSimilarityMap.isEmpty()) {
            qint64 bestOffset = 0;
            double bestSimilarity = 0.0;
            
            for (auto it = m_offsetSimilarityMap.constBegin(); it != m_offsetSimilarityMap.constEnd(); ++it) {
                if (it.value() > bestSimilarity) {
                    bestSimilarity = it.value();
                    bestOffset = it.key();
                }
            }
            
            // Simple but effective confidence calculation
            double averageSimilarity = calculateOverallSimilarity(m_offsetSimilarityMap.values());
            double confidence = (bestSimilarity - averageSimilarity) / averageSimilarity;
            confidence = qMax(0.0, qMin(1.0, confidence * 2.0));
            
            qDebug() << "Offset detection complete. Best offset:" << bestOffset << "ms"
                     << "with similarity:" << (bestSimilarity * 100) << "% and confidence:" << (confidence * 100) << "%";
            
            emit optimalOffsetFound(bestOffset, confidence);
        }
        
        m_isDetectingOffset = false;
        return;
    }
    
    // Test current offset candidate
    qint64 offsetToTest = m_offsetCandidates[m_currentOffsetIndex];
    double similarity = testOffsetSimilarity(offsetToTest, m_offsetTestTimestamps);
    m_offsetSimilarityMap[offsetToTest] = similarity;
    
    qDebug() << "Tested offset" << offsetToTest << "ms: similarity" << (similarity * 100) << "%";
    
    // Update progress
    int progress = ((m_currentOffsetIndex + 1) * 100) / m_offsetCandidates.size();
    emit comparisonProgress(progress);
    
    m_currentOffsetIndex++;
    
    // Schedule next test with minimal delay for speed
    QTimer::singleShot(1, this, &VideoComparator::performOffsetDetection);
}

QList<qint64> VideoComparator::generateOffsetCandidates(qint64 maxOffsetMs, int stepMs)
{
    QList<qint64> candidates;
    
    // Generate candidates from negative to positive offset
    for (qint64 offset = -maxOffsetMs; offset <= maxOffsetMs; offset += stepMs) {
        candidates.append(offset);
    }
    
    // Always include zero offset
    if (!candidates.contains(0)) {
        candidates.append(0);
        std::sort(candidates.begin(), candidates.end());
    }
    
    return candidates;
}

double VideoComparator::testOffsetSimilarity(qint64 offset, const QList<qint64> &testTimestamps)
{
    // Temporarily set the offset for testing
    qint64 originalOffsetA = m_videoAOffset;
    qint64 originalOffsetB = m_videoBOffset;
    
    // Apply the test offset (offset Video A relative to Video B)
    m_videoAOffset = offset;
    m_videoBOffset = 0;
    
    QList<double> similarities;
    
    for (qint64 timestamp : testTimestamps) {
        // Skip timestamps that would go out of bounds with the current offset
        qint64 timestampA = timestamp + m_videoAOffset;
        qint64 timestampB = timestamp + m_videoBOffset;
        
        if (timestampA >= 0 && timestampA < m_videoDuration1 && 
            timestampB >= 0 && timestampB < m_videoDuration2) {
            
            double similarity = compareFramesAtTimestamp(timestamp);
            if (similarity >= 0.0) { // Only count valid comparisons
                similarities.append(similarity);
            }
        }
    }
    
    // Restore original offsets
    m_videoAOffset = originalOffsetA;
    m_videoBOffset = originalOffsetB;
    
    // Return average similarity, or 0 if no valid comparisons
    return similarities.isEmpty() ? 0.0 : calculateOverallSimilarity(similarities);
}

