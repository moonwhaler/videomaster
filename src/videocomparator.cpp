#include "videocomparator.h"
#include "ffmpeghandler.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QtMath>
#include <algorithm>
#include <numeric>
#include <cmath>

VideoComparator::VideoComparator(QObject *parent)
    : QObject(parent)
    , m_videoAOffset(0)
    , m_videoBOffset(0)
    , m_videoDuration1(0)
    , m_videoDuration2(0)
    , m_comparisonTimer(new QTimer(this))
    , m_isComparing(false)
    , m_isAutoComparing(false)
    , m_isDetectingOffset(false)
    , m_currentTimestamp(0)
    , m_videoDuration(0)
    , m_currentSampleIndex(0)
    , m_currentOffsetIndex(0)
{
    connect(m_comparisonTimer, &QTimer::timeout, this, &VideoComparator::performFrameComparison);
    m_comparisonTimer->setInterval(100);
    
    // Initialize frame cache with reasonable size (100 frames ~= 100MB)
    m_frameCache.setMaxCost(100);
}

VideoComparator::~VideoComparator()
{
    stopComparison();
}

void VideoComparator::setVideo(int index, const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    FFmpegHandler handler;
    
    if (index == 0) {
        m_videoPath1 = filePath;
        m_videoDuration1 = handler.getVideoDuration(filePath);
        // Clear cached frames for video 1
        m_cachedFramesVideo1.clear();
    } else if (index == 1) {
        m_videoPath2 = filePath;
        m_videoDuration2 = handler.getVideoDuration(filePath);
        // Clear cached frames for video 2
        m_cachedFramesVideo2.clear();
    }
    
    // Clear frame cache when videos change
    m_frameCache.clear();
    
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
    
    if (m_videoDuration > 0) {
        int progress = (m_currentTimestamp * 100) / m_videoDuration;
        emit comparisonProgress(progress);
    }
    
    m_currentTimestamp += 1000;
    
    if (m_currentTimestamp >= m_videoDuration) {
        stopComparison();
    }
}

// PERCEPTUAL HASHING IMPLEMENTATION
uint64_t VideoComparator::computePerceptualHash(const QImage &image)
{
    // Step 1: Resize to 8x8 and convert to grayscale
    QImage small = image.scaled(8, 8, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                        .convertToFormat(QImage::Format_Grayscale8);
    
    // Step 2: Calculate average pixel value
    double total = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            total += qGray(small.pixel(x, y));
        }
    }
    double average = total / 64.0;
    
    // Step 3: Create hash based on pixels above/below average
    uint64_t hash = 0;
    int bit = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (qGray(small.pixel(x, y)) > average) {
                hash |= (1ULL << bit);
            }
            bit++;
        }
    }
    
    return hash;
}

int VideoComparator::hammingDistance(uint64_t hash1, uint64_t hash2)
{
    uint64_t xor_result = hash1 ^ hash2;
    int distance = 0;
    
    while (xor_result) {
        distance += xor_result & 1;
        xor_result >>= 1;
    }
    
    return distance;
}

// FEATURE EXTRACTION
QVector<double> VideoComparator::computeColorHistogram(const QImage &image)
{
    QVector<double> histogram(3 * 16, 0.0); // 16 bins per channel (R, G, B)
    QImage scaled = image.scaled(160, 120, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    
    int totalPixels = scaled.width() * scaled.height();
    
    for (int y = 0; y < scaled.height(); ++y) {
        for (int x = 0; x < scaled.width(); ++x) {
            QRgb pixel = scaled.pixel(x, y);
            
            int rBin = (qRed(pixel) * 16) / 256;
            int gBin = (qGreen(pixel) * 16) / 256;
            int bBin = (qBlue(pixel) * 16) / 256;
            
            histogram[rBin]++;
            histogram[16 + gBin]++;
            histogram[32 + bBin]++;
        }
    }
    
    // Normalize histogram
    for (int i = 0; i < histogram.size(); ++i) {
        histogram[i] /= totalPixels;
    }
    
    return histogram;
}

double VideoComparator::computeEdgeDensity(const QImage &image)
{
    QImage gray = image.scaled(160, 120, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                       .convertToFormat(QImage::Format_Grayscale8);
    
    double edgeSum = 0;
    int width = gray.width();
    int height = gray.height();
    
    // Simple Sobel edge detection
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            // Sobel X
            int gx = -qGray(gray.pixel(x-1, y-1)) - 2*qGray(gray.pixel(x-1, y)) - qGray(gray.pixel(x-1, y+1))
                    + qGray(gray.pixel(x+1, y-1)) + 2*qGray(gray.pixel(x+1, y)) + qGray(gray.pixel(x+1, y+1));
            
            // Sobel Y
            int gy = -qGray(gray.pixel(x-1, y-1)) - 2*qGray(gray.pixel(x, y-1)) - qGray(gray.pixel(x+1, y-1))
                    + qGray(gray.pixel(x-1, y+1)) + 2*qGray(gray.pixel(x, y+1)) + qGray(gray.pixel(x+1, y+1));
            
            double magnitude = std::sqrt(gx*gx + gy*gy);
            if (magnitude > 50) { // Edge threshold
                edgeSum += 1.0;
            }
        }
    }
    
    return edgeSum / ((width - 2) * (height - 2));
}

bool VideoComparator::isSceneChange(const QImage &prevFrame, const QImage &currentFrame)
{
    if (prevFrame.isNull() || currentFrame.isNull()) {
        return false;
    }
    
    // Compare histograms for scene change detection
    QVector<double> hist1 = computeColorHistogram(prevFrame);
    QVector<double> hist2 = computeColorHistogram(currentFrame);
    
    double histDiff = 0;
    for (int i = 0; i < hist1.size(); ++i) {
        histDiff += std::abs(hist1[i] - hist2[i]);
    }
    
    return histDiff > 0.3; // Scene change threshold
}

// FRAME MANAGEMENT
VideoComparator::FrameInfo VideoComparator::extractFrameInfo(const QString &videoPath, qint64 timestamp)
{
    FFmpegHandler handler;
    FrameInfo info;
    
    info.image = handler.extractFrame(videoPath, timestamp);
    
    if (!info.image.isNull()) {
        info.perceptualHash = computePerceptualHash(info.image);
        info.colorHistogram = computeColorHistogram(info.image);
        info.edgeDensity = computeEdgeDensity(info.image);
        info.isSceneChange = false; // Will be set during scene detection
    }
    
    return info;
}

VideoComparator::FrameInfo VideoComparator::getCachedOrExtractFrame(const QString &videoPath, qint64 timestamp)
{
    QString cacheKey = QString("%1_%2").arg(videoPath).arg(timestamp);
    
    // Check cache first
    if (m_frameCache.contains(cacheKey)) {
        return *m_frameCache.object(cacheKey);
    }
    
    // Extract and cache if not found
    FrameInfo info = extractFrameInfo(videoPath, timestamp);
    m_frameCache.insert(cacheKey, new FrameInfo(info));
    
    return info;
}

void VideoComparator::preloadFramesForOffsetDetection()
{
    qDebug() << "=== Preloading frames for simple cross-correlation ===";
    
    // Clear caches
    m_cachedFramesVideo1.clear();
    m_cachedFramesVideo2.clear();
    
    // Simple approach: extract frames every 500ms for first 20 seconds
    qint64 duration = qMin(m_videoDuration1, m_videoDuration2);
    qint64 sampleDuration = qMin(duration, qint64(20000)); // First 20 seconds
    qint64 maxOffset = 15000; // ±15 seconds
    
    qDebug() << "Sampling duration:" << sampleDuration << "ms";
    qDebug() << "Max offset range: ±" << maxOffset << "ms";
    
    // Extract frames from video 1 every 500ms
    for (qint64 t = 2000; t < sampleDuration; t += 500) { // Start at 2s to skip intro
        m_cachedFramesVideo1[t] = extractFrameInfo(m_videoPath1, t);
    }
    
    // Extract frames from video 2 with extended range to cover all offset possibilities
    qint64 startTime = qMax(0LL, 2000 - maxOffset);
    qint64 endTime = qMin(m_videoDuration2, sampleDuration + maxOffset);
    
    for (qint64 t = startTime; t < endTime; t += 500) {
        m_cachedFramesVideo2[t] = extractFrameInfo(m_videoPath2, t);
    }
    
    qDebug() << "Loaded" << m_cachedFramesVideo1.size() << "reference frames from video A";
    qDebug() << "Loaded" << m_cachedFramesVideo2.size() << "search frames from video B";
    qDebug() << "Video B range:" << startTime << "ms to" << endTime << "ms";
}

// MULTI-METRIC FRAME COMPARISON - OPTIMIZED FOR QUALITY DIFFERENCES
double VideoComparator::compareFrameInfo(const FrameInfo &frame1, const FrameInfo &frame2)
{
    if (frame1.image.isNull() || frame2.image.isNull()) {
        return 0.0;
    }
    
    // 1. Perceptual hash similarity (weight: 70% - most robust for quality differences)
    int hashDistance = hammingDistance(frame1.perceptualHash, frame2.perceptualHash);
    double hashSimilarity = 1.0 - (hashDistance / 64.0);
    
    // 2. Simplified color similarity (weight: 30% - less affected by compression)
    // Use only major color bins to be more robust to quality differences
    double colorSimilarity = 0;
    if (frame1.colorHistogram.size() == frame2.colorHistogram.size()) {
        // Compare only the most significant color bins (reduce noise from compression)
        double totalDiff = 0;
        int significantBins = 0;
        
        for (int i = 0; i < frame1.colorHistogram.size(); ++i) {
            double avg = (frame1.colorHistogram[i] + frame2.colorHistogram[i]) / 2.0;
            if (avg > 0.01) { // Only consider bins with significant presence
                double diff = std::abs(frame1.colorHistogram[i] - frame2.colorHistogram[i]);
                totalDiff += diff;
                significantBins++;
            }
        }
        
        if (significantBins > 0) {
            colorSimilarity = 1.0 - std::min(1.0, (totalDiff / significantBins) * 10);
        } else {
            colorSimilarity = hashSimilarity; // Fallback to hash if no significant color data
        }
    }
    
    // Weighted combination - emphasize perceptual hash for quality robustness
    double totalSimilarity = hashSimilarity * 0.70 + colorSimilarity * 0.30;
    
    return totalSimilarity;
}

double VideoComparator::compareFramesAtTimestamp(qint64 timestamp)
{
    // Apply offsets to timestamps
    qint64 timestamp1 = timestamp + m_videoAOffset;
    qint64 timestamp2 = timestamp + m_videoBOffset;
    
    // Ensure timestamps are within valid bounds
    timestamp1 = qMax(0LL, qMin(timestamp1, m_videoDuration1 - 1));
    timestamp2 = qMax(0LL, qMin(timestamp2, m_videoDuration2 - 1));
    
    // Get frame info (cached or extracted)
    FrameInfo frame1 = getCachedOrExtractFrame(m_videoPath1, timestamp1);
    FrameInfo frame2 = getCachedOrExtractFrame(m_videoPath2, timestamp2);
    
    return compareFrameInfo(frame1, frame2);
}

// SMART SAMPLING
QList<qint64> VideoComparator::generateSmartSampleTimestamps(qint64 duration)
{
    QList<qint64> timestamps;
    
    if (duration <= 0) {
        return timestamps;
    }
    
    // Start earlier to catch offsets better - only skip first 500ms for black frames
    qint64 startTime = qMin(500LL, duration / 20);
    qint64 endTime = duration - 500;
    
    if (endTime <= startTime) {
        timestamps.append((startTime + endTime) / 2);
        return timestamps;
    }
    
    // Strategic sampling points with more early samples
    // 1. Very early content (critical for offset detection)
    timestamps.append(startTime);
    timestamps.append(startTime + 500);
    timestamps.append(startTime + 1000);
    timestamps.append(startTime + 1500);
    timestamps.append(startTime + 2000);
    timestamps.append(startTime + 3000);
    timestamps.append(startTime + 5000);
    timestamps.append(startTime + 7500);
    timestamps.append(startTime + 10000);
    timestamps.append(startTime + 15000);
    
    // 2. Detect scene changes for additional sampling points
    QList<qint64> sceneChanges = detectSceneChanges(
        m_videoPath1, startTime, qMin(startTime + 30000, endTime));
    
    // Add up to 5 scene change points
    for (int i = 0; i < qMin(5, sceneChanges.size()); ++i) {
        timestamps.append(sceneChanges[i]);
    }
    
    // 3. Some regular intervals through the rest
    qint64 step = (endTime - startTime) / 5;
    for (int i = 1; i < 5; ++i) {
        timestamps.append(startTime + i * step);
    }
    
    // Remove duplicates and sort
    std::sort(timestamps.begin(), timestamps.end());
    timestamps.erase(std::unique(timestamps.begin(), timestamps.end()), timestamps.end());
    
    // Limit to reasonable number but allow more samples
    if (timestamps.size() > 20) {
        timestamps = timestamps.mid(0, 20);
    }
    
    return timestamps;
}

QList<qint64> VideoComparator::detectSceneChanges(const QString &videoPath, qint64 startMs, qint64 endMs)
{
    QList<qint64> sceneChanges;
    
    FFmpegHandler handler;
    QImage prevFrame;
    qint64 step = 500; // Check every 500ms
    
    for (qint64 t = startMs; t <= endMs; t += step) {
        QImage currentFrame = handler.extractFrame(videoPath, t);
        
        if (!prevFrame.isNull() && !currentFrame.isNull()) {
            if (isSceneChange(prevFrame, currentFrame)) {
                sceneChanges.append(t);
            }
        }
        
        prevFrame = currentFrame;
        
        // Limit scene changes to avoid too many
        if (sceneChanges.size() >= 10) {
            break;
        }
    }
    
    return sceneChanges;
}

// HIERARCHICAL OFFSET DETECTION
QList<qint64> VideoComparator::generateHierarchicalOffsetCandidates(qint64 maxOffsetMs)
{
    QList<qint64> candidates;
    
    // Start with coarse 1-second steps for speed
    for (qint64 offset = -maxOffsetMs; offset <= maxOffsetMs; offset += 1000) {
        candidates.append(offset);
    }
    
    qDebug() << "Generated" << candidates.size() << "coarse candidates with 1s steps";
    return candidates;
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
    
    qDebug() << "=== Starting Offset Detection ===";
    qDebug() << "Video A duration:" << m_videoDuration1 << "ms";
    qDebug() << "Video B duration:" << m_videoDuration2 << "ms";
    
    // Preload frames for fast comparison
    preloadFramesForOffsetDetection();
    
    // Generate hierarchical offset candidates - increase range to 15 seconds
    m_offsetCandidates = generateHierarchicalOffsetCandidates(15000);
    
    qDebug() << "Starting offset detection with" << m_offsetCandidates.size() 
             << "candidates (range: ±15 seconds)";
    
    QTimer::singleShot(0, this, &VideoComparator::performOffsetDetection);
}

void VideoComparator::performOffsetDetection()
{
    if (!m_isDetectingOffset || m_currentOffsetIndex >= m_offsetCandidates.size()) {
        // Phase 1 complete - find best coarse offset
        if (!m_offsetSimilarityMap.isEmpty()) {
            qint64 bestCoarseOffset = 0;
            double bestSimilarity = 0.0;
            
            for (auto it = m_offsetSimilarityMap.constBegin(); it != m_offsetSimilarityMap.constEnd(); ++it) {
                if (it.value() > bestSimilarity) {
                    bestSimilarity = it.value();
                    bestCoarseOffset = it.key();
                }
            }
            
            // Phase 2: Fine-tune around best coarse offset
            if (m_currentOffsetIndex >= m_offsetCandidates.size() && bestSimilarity > 0.4) {
                // Check if we need to do fine-tuning
                bool needsFineTuning = false;
                for (qint64 offset : m_offsetCandidates) {
                    if (std::abs(offset - bestCoarseOffset) == 500) {
                        needsFineTuning = true;
                        break;
                    }
                }
                
                if (needsFineTuning && m_offsetCandidates.last() >= 100) {
                    // Generate fine-tuning candidates with wider range
                    m_offsetCandidates.clear();
                    for (qint64 offset = bestCoarseOffset - 750; offset <= bestCoarseOffset + 750; offset += 25) {
                        if (!m_offsetSimilarityMap.contains(offset)) {
                            m_offsetCandidates.append(offset);
                        }
                    }
                    
                    if (!m_offsetCandidates.isEmpty()) {
                        m_currentOffsetIndex = 0;
                        qDebug() << "Fine-tuning around" << bestCoarseOffset << "ms with" 
                                << m_offsetCandidates.size() << "candidates";
                        QTimer::singleShot(1, this, &VideoComparator::performOffsetDetection);
                        return;
                    }
                }
            }
            
            // Final result
            qint64 bestOffset = 0;
            double bestFinalSimilarity = 0.0;
            
            for (auto it = m_offsetSimilarityMap.constBegin(); it != m_offsetSimilarityMap.constEnd(); ++it) {
                if (it.value() > bestFinalSimilarity) {
                    bestFinalSimilarity = it.value();
                    bestOffset = it.key();
                }
            }
            
            double confidence = calculateConfidenceScore(m_offsetSimilarityMap, bestOffset);
            
            qDebug() << "Offset detection complete. Best offset:" << bestOffset << "ms"
                     << "with similarity:" << (bestFinalSimilarity * 100) << "% and confidence:" 
                     << (confidence * 100) << "%";
            
            emit optimalOffsetFound(bestOffset, confidence);
        }
        
        m_isDetectingOffset = false;
        m_cachedFramesVideo1.clear();
        m_cachedFramesVideo2.clear();
        return;
    }
    
    // Test current offset candidate
    qint64 offsetToTest = m_offsetCandidates[m_currentOffsetIndex];
    double similarity = testOffsetWithCache(offsetToTest);
    m_offsetSimilarityMap[offsetToTest] = similarity;
    
    // Update progress
    int progress = ((m_currentOffsetIndex + 1) * 100) / m_offsetCandidates.size();
    emit comparisonProgress(progress);
    
    m_currentOffsetIndex++;
    
    // Minimal delay for speed
    QTimer::singleShot(1, this, &VideoComparator::performOffsetDetection);
}

double VideoComparator::testOffsetWithCache(qint64 offset)
{
    QList<double> similarities;
    int validComparisons = 0;
    
    qDebug() << "=== Testing offset" << offset << "ms ===";
    
    // For each reference frame in video A
    for (auto it = m_cachedFramesVideo1.constBegin(); it != m_cachedFramesVideo1.constEnd(); ++it) {
        qint64 timestampA = it.key();
        // Find corresponding frame in video B
        qint64 timestampB = timestampA + offset;
        
        qDebug() << "  Looking for Video A@" << timestampA << "ms -> Video B@" << timestampB << "ms";
        
        // Check if we have the corresponding frame in video B
        if (m_cachedFramesVideo2.contains(timestampB)) {
            const FrameInfo &frameA = it.value();
            const FrameInfo &frameB = m_cachedFramesVideo2[timestampB];
            
            if (!frameA.image.isNull() && !frameB.image.isNull()) {
                double similarity = compareFrameInfo(frameA, frameB);
                similarities.append(similarity);
                validComparisons++;
                
                qDebug() << "    Comparison" << validComparisons << ": similarity =" << (similarity * 100) << "%";
                
                // Log high similarity matches with more detail
                if (similarity > 0.6) {
                    int hashDist = hammingDistance(frameA.perceptualHash, frameB.perceptualHash);
                    qDebug() << "    ** Good match: hash distance =" << hashDist << "/64";
                }
            }
        } else {
            qDebug() << "    No frame available at" << timestampB << "ms in video B";
        }
    }
    
    if (similarities.isEmpty()) {
        qDebug() << "  No valid comparisons for offset" << offset << "ms";
        return 0.0;
    }
    
    // Use simple average for better interpretability
    double avgSimilarity = std::accumulate(similarities.begin(), similarities.end(), 0.0) / similarities.size();
    
    qDebug() << "  RESULT: offset" << offset << "ms -> avg similarity" << (avgSimilarity * 100) 
             << "% from" << validComparisons << "comparisons";
    
    return avgSimilarity;
}

// CONFIDENCE CALCULATION
double VideoComparator::calculateConfidenceScore(const QMap<qint64, double> &offsetSimilarityMap, qint64 bestOffset)
{
    if (offsetSimilarityMap.size() < 3) {
        return 0.0;
    }
    
    double bestSimilarity = offsetSimilarityMap[bestOffset];
    
    // Calculate statistics
    QList<double> allSimilarities = offsetSimilarityMap.values();
    std::sort(allSimilarities.begin(), allSimilarities.end());
    
    double median = allSimilarities[allSimilarities.size() / 2];
    double q75 = allSimilarities[allSimilarities.size() * 3 / 4];
    
    // Confidence based on how much better the best is compared to others
    double separation = (bestSimilarity - q75) / (1.0 - q75 + 0.001);
    separation = qMax(0.0, qMin(1.0, separation));
    
    // Also consider absolute similarity value
    double absoluteConfidence = qMax(0.0, (bestSimilarity - 0.7) / 0.3);
    absoluteConfidence = qMin(1.0, absoluteConfidence);
    
    // Combined confidence
    double confidence = separation * 0.6 + absoluteConfidence * 0.4;
    
    return confidence;
}

// AUTO COMPARISON
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
    
    // Calculate effective duration
    qint64 effectiveStartTime = qMax(0LL, qMax(-m_videoAOffset, -m_videoBOffset));
    qint64 effectiveEndTime = qMin(m_videoDuration1 - m_videoAOffset, m_videoDuration2 - m_videoBOffset);
    qint64 effectiveDuration = qMax(0LL, effectiveEndTime - effectiveStartTime);
    
    if (effectiveDuration <= 0) {
        qWarning() << "Invalid effective duration due to offsets";
        m_isAutoComparing = false;
        emit autoComparisonComplete(0.0, false, "Cannot compare: offsets result in no overlapping video content.");
        return;
    }
    
    m_autoSampleTimestamps = generateSmartSampleTimestamps(effectiveDuration);
    
    // Adjust timestamps to start from effective start time
    for (int i = 0; i < m_autoSampleTimestamps.size(); ++i) {
        m_autoSampleTimestamps[i] += effectiveStartTime;
    }
    
    qDebug() << "Starting auto comparison with" << m_autoSampleTimestamps.size() << "samples";
    
    QTimer::singleShot(0, this, &VideoComparator::performAutoComparison);
}

void VideoComparator::performAutoComparison()
{
    if (!m_isAutoComparing || m_currentSampleIndex >= m_autoSampleTimestamps.size()) {
        // Comparison complete
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
    
    // Update progress
    int progress = ((m_currentSampleIndex + 1) * 100) / m_autoSampleTimestamps.size();
    emit comparisonProgress(progress);
    
    m_currentSampleIndex++;
    
    // Small delay to allow UI updates
    QTimer::singleShot(20, this, &VideoComparator::performAutoComparison);
}

double VideoComparator::calculateOverallSimilarity(const QList<double> &similarities)
{
    if (similarities.isEmpty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(similarities.begin(), similarities.end(), 0.0);
    return sum / similarities.size();
}

bool VideoComparator::determineIfIdentical(double overallSimilarity, const QList<double> &similarities)
{
    const double OVERALL_THRESHOLD = 0.85; // Slightly lower with better metrics
    const double MIN_FRAME_THRESHOLD = 0.75;
    const double HIGH_SIMILARITY_THRESHOLD = 0.90;
    const double HIGH_SIMILARITY_RATIO = 0.70;
    
    if (overallSimilarity < OVERALL_THRESHOLD) {
        return false;
    }
    
    int highSimilarityCount = 0;
    for (double sim : similarities) {
        if (sim < MIN_FRAME_THRESHOLD) {
            return false;
        }
        if (sim >= HIGH_SIMILARITY_THRESHOLD) {
            highSimilarityCount++;
        }
    }
    
    double highSimilarityRatio = static_cast<double>(highSimilarityCount) / similarities.size();
    return highSimilarityRatio >= HIGH_SIMILARITY_RATIO;
}
