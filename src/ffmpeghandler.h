#ifndef FFMPEGHANDLER_H
#define FFMPEGHANDLER_H

#include <QString>
#include <QStringList>
#include <QImage>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
}

struct AudioTrackInfo {
    int index;
    QString codec;
    QString language;
    int channels;
    int sampleRate;
    QString title;
};

struct SubtitleTrackInfo {
    int index;
    QString codec;
    QString language;
    QString title;
};

struct ChapterInfo {
    int index;
    QString title;
    qint64 startTimeMs;
    qint64 endTimeMs;
    QString formattedTime;
};

class FFmpegHandler
{
public:
    FFmpegHandler();
    ~FFmpegHandler();
    
    // Video information
    qint64 getVideoDuration(const QString &filePath);
    QImage extractFrame(const QString &filePath, qint64 timestampMs);
    
    // Track information
    QList<AudioTrackInfo> getAudioTracks(const QString &filePath);
    QList<SubtitleTrackInfo> getSubtitleTracks(const QString &filePath);
    
    // Chapter information
    QList<ChapterInfo> getChapters(const QString &filePath);
    
    // Track transfer operations
    bool transferTracks(const QString &sourceFile, 
                       const QString &targetFile,
                       const QString &outputFile,
                       const QList<int> &audioTrackIndexes,
                       const QList<int> &subtitleTrackIndexes);
    
    // New merge tracks operation
    bool mergeTracks(const QString &sourceFile,
                    const QString &targetFile, 
                    const QString &outputFile,
                    const QList<QPair<QString, int>> &selectedAudioTracks,
                    const QList<QPair<QString, int>> &selectedSubtitleTracks);
    
    // Batch operations
    bool batchTransferTracks(const QStringList &sourceFiles,
                           const QStringList &targetFiles,
                           const QString &outputDir,
                           const QString &postfix,
                           const QList<int> &audioTrackIndexes,
                           const QList<int> &subtitleTrackIndexes);

private:
    void initializeFFmpeg();
    AVFormatContext* openVideoFile(const QString &filePath);
    void closeVideoFile(AVFormatContext *formatContext);
    
    bool m_initialized;
};

#endif // FFMPEGHANDLER_H