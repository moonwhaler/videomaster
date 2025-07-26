#include "ffmpeghandler.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <cmath>

FFmpegHandler::FFmpegHandler()
    : m_initialized(false)
{
    initializeFFmpeg();
}

FFmpegHandler::~FFmpegHandler() = default;

void FFmpegHandler::initializeFFmpeg()
{
    if (!m_initialized) {
        // Set FFmpeg log level to suppress verbose warnings about attachments and unknown codecs
        av_log_set_level(AV_LOG_ERROR); // Only show actual errors, not warnings
        m_initialized = true;
    }
}

qint64 FFmpegHandler::getVideoDuration(const QString &filePath)
{
    AVFormatContext *formatContext = openVideoFile(filePath);
    if (!formatContext) {
        return 0;
    }
    
    qint64 duration = formatContext->duration / AV_TIME_BASE * 1000; // Convert to milliseconds
    closeVideoFile(formatContext);
    
    return duration;
}

QImage FFmpegHandler::extractFrame(const QString &filePath, qint64 timestampMs)
{
    AVFormatContext *formatContext = openVideoFile(filePath);
    if (!formatContext) {
        return QImage();
    }
    
    // Find video stream
    AVCodecParameters *codecParameters = nullptr;
    int videoStreamIndex = -1;
    
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            codecParameters = formatContext->streams[i]->codecpar;
            videoStreamIndex = i;
            break;
        }
    }
    
    if (videoStreamIndex == -1) {
        closeVideoFile(formatContext);
        return QImage();
    }
    
    // Find decoder
    const AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!codec) {
        closeVideoFile(formatContext);
        return QImage();
    }
    
    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecParameters);
    
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        avcodec_free_context(&codecContext);
        closeVideoFile(formatContext);
        return QImage();
    }
    
    // Seek to timestamp
    qint64 seekTarget = timestampMs * AV_TIME_BASE / 1000;
    av_seek_frame(formatContext, -1, seekTarget, AVSEEK_FLAG_BACKWARD);
    
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *frameRGB = av_frame_alloc();
    
    QImage result;
    
    // Allocate buffer for RGB frame
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes);
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);
    
    // Setup swscale context
    SwsContext *swsContext = sws_getContext(codecContext->width, codecContext->height, codecContext->pix_fmt,
                                           codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
                                           SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            if (avcodec_send_packet(codecContext, packet) == 0) {
                if (avcodec_receive_frame(codecContext, frame) == 0) {
                    // Convert frame to RGB
                    sws_scale(swsContext, frame->data, frame->linesize, 0, codecContext->height,
                             frameRGB->data, frameRGB->linesize);
                    
                    // Create QImage
                    result = QImage(frameRGB->data[0], codecContext->width, codecContext->height, 
                                   frameRGB->linesize[0], QImage::Format_RGB888).copy();
                    break;
                }
            }
        }
        av_packet_unref(packet);
    }
    
    // Cleanup
    sws_freeContext(swsContext);
    av_free(buffer);
    av_frame_free(&frameRGB);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    closeVideoFile(formatContext);
    
    return result;
}

QList<AudioTrackInfo> FFmpegHandler::getAudioTracks(const QString &filePath)
{
    QList<AudioTrackInfo> tracks;
    
    AVFormatContext *formatContext = openVideoFile(filePath);
    if (!formatContext) {
        return tracks;
    }
    
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        AVStream *stream = formatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            // Skip streams with unknown or problematic codecs
            if (stream->codecpar->codec_id == AV_CODEC_ID_NONE) {
                continue;
            }
            
            AudioTrackInfo track;
            track.index = i;
            
            const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
            track.codec = codec ? codec->name : "unknown";
            track.channels = stream->codecpar->ch_layout.nb_channels;
            track.sampleRate = stream->codecpar->sample_rate;
            
            // Get language from metadata
            AVDictionaryEntry *langEntry = av_dict_get(stream->metadata, "language", nullptr, 0);
            track.language = langEntry ? langEntry->value : "und";
            
            // Get title from metadata
            AVDictionaryEntry *titleEntry = av_dict_get(stream->metadata, "title", nullptr, 0);
            track.title = titleEntry ? titleEntry->value : QString("Audio Track %1").arg(tracks.size() + 1);
            
            tracks.append(track);
        }
    }
    
    closeVideoFile(formatContext);
    return tracks;
}

QList<SubtitleTrackInfo> FFmpegHandler::getSubtitleTracks(const QString &filePath)
{
    QList<SubtitleTrackInfo> tracks;
    
    AVFormatContext *formatContext = openVideoFile(filePath);
    if (!formatContext) {
        return tracks;
    }
    
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        AVStream *stream = formatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) {
            // Skip streams with unknown or problematic codecs
            if (stream->codecpar->codec_id == AV_CODEC_ID_NONE) {
                continue;
            }
            
            SubtitleTrackInfo track;
            track.index = i;
            
            const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
            track.codec = codec ? codec->name : "unknown";
            
            // Get language from metadata
            AVDictionaryEntry *langEntry = av_dict_get(stream->metadata, "language", nullptr, 0);
            track.language = langEntry ? langEntry->value : "und";
            
            // Get title from metadata
            AVDictionaryEntry *titleEntry = av_dict_get(stream->metadata, "title", nullptr, 0);
            track.title = titleEntry ? titleEntry->value : QString("Subtitle Track %1").arg(tracks.size() + 1);
            
            tracks.append(track);
        }
    }
    
    closeVideoFile(formatContext);
    return tracks;
}

bool FFmpegHandler::transferTracks(const QString &sourceFile,
                                  const QString &targetFile,
                                  const QString &outputFile,
                                  const QList<int> &audioTrackIndexes,
                                  const QList<int> &subtitleTrackIndexes)
{
    QStringList arguments;
    arguments << "-i" << targetFile;  // Target video (main content)
    arguments << "-i" << sourceFile;  // Source video (tracks to transfer)
    
    // Map video from target file
    arguments << "-map" << "0:v:0";
    
    // Map existing audio from target if no audio tracks to transfer
    if (audioTrackIndexes.isEmpty()) {
        arguments << "-map" << "0:a?";
    }
    
    // Map selected audio tracks from source
    for (int trackIndex : audioTrackIndexes) {
        arguments << "-map" << QString("1:%1").arg(trackIndex);
    }
    
    // Map existing subtitles from target if no subtitle tracks to transfer
    if (subtitleTrackIndexes.isEmpty()) {
        arguments << "-map" << "0:s?";
    }
    
    // Map selected subtitle tracks from source
    for (int trackIndex : subtitleTrackIndexes) {
        arguments << "-map" << QString("1:%1").arg(trackIndex);
    }
    
    // Copy codecs without re-encoding
    arguments << "-c" << "copy";
    
    // Output file
    arguments << outputFile;
    
    // Overwrite output file if it exists
    arguments << "-y";
    
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", arguments);
    ffmpeg.waitForFinished(-1);
    
    return ffmpeg.exitCode() == 0;
}

bool FFmpegHandler::mergeTracks(const QString &sourceFile,
                               const QString &targetFile, 
                               const QString &outputFile,
                               const QList<QPair<QString, int>> &selectedAudioTracks,
                               const QList<QPair<QString, int>> &selectedSubtitleTracks)
{
    QStringList arguments;
    arguments << "-i" << sourceFile;  // Input 0: source video
    arguments << "-i" << targetFile;  // Input 1: target video
    
    // Always map the video stream from target (base video)
    arguments << "-map" << "1:v:0";
    
    // Map selected audio tracks
    for (const auto &track : selectedAudioTracks) {
        QString source = track.first;
        int trackIndex = track.second;
        
        if (source == "source") {
            arguments << "-map" << QString("0:%1").arg(trackIndex);
        } else if (source == "target") {
            arguments << "-map" << QString("1:%1").arg(trackIndex);
        }
    }
    
    // Map selected subtitle tracks
    for (const auto &track : selectedSubtitleTracks) {
        QString source = track.first;
        int trackIndex = track.second;
        
        if (source == "source") {
            arguments << "-map" << QString("0:%1").arg(trackIndex);
        } else if (source == "target") {
            arguments << "-map" << QString("1:%1").arg(trackIndex);
        }
    }
    
    // Copy all streams without re-encoding
    arguments << "-c" << "copy";
    
    // Output file
    arguments << outputFile;
    
    // Overwrite output file if it exists
    arguments << "-y";
    
    QProcess ffmpeg;
    ffmpeg.start("ffmpeg", arguments);
    ffmpeg.waitForFinished(-1);
    
    return ffmpeg.exitCode() == 0;
}

bool FFmpegHandler::batchTransferTracks(const QStringList &sourceFiles,
                                       const QStringList &targetFiles,
                                       const QString &outputDir,
                                       const QString &postfix,
                                       const QList<int> &audioTrackIndexes,
                                       const QList<int> &subtitleTrackIndexes)
{
    if (sourceFiles.size() != targetFiles.size()) {
        qWarning() << "Source and target file lists must have the same size";
        return false;
    }
    
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(outputDir);
    }
    
    bool allSuccessful = true;
    
    for (int i = 0; i < sourceFiles.size(); ++i) {
        QFileInfo targetInfo(targetFiles[i]);
        QString outputFileName = targetInfo.baseName() + postfix + "." + targetInfo.suffix();
        QString outputPath = dir.absoluteFilePath(outputFileName);
        
        bool success = transferTracks(sourceFiles[i], targetFiles[i], outputPath,
                                     audioTrackIndexes, subtitleTrackIndexes);
        
        if (!success) {
            qWarning() << "Failed to process:" << targetFiles[i];
            allSuccessful = false;
        }
    }
    
    return allSuccessful;
}

AVFormatContext* FFmpegHandler::openVideoFile(const QString &filePath)
{
    AVFormatContext *formatContext = nullptr;
    
    if (avformat_open_input(&formatContext, filePath.toUtf8().data(), nullptr, nullptr) != 0) {
        return nullptr;
    }
    
    // Use simpler approach without dictionary options to avoid memory issues
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        avformat_close_input(&formatContext);
        return nullptr;
    }
    
    return formatContext;
}

void FFmpegHandler::closeVideoFile(AVFormatContext *formatContext)
{
    if (formatContext) {
        avformat_close_input(&formatContext);
    }
}