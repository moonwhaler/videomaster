# VideoMaster

A Qt6 application for Linux that provides video comparison and audio/subtitle track transfer capabilities using FFmpeg.

## Features

### Video Comparison
- **Drag & Drop Interface**: Easily load two videos for comparison by dragging them into the application window
- **Visual Comparison**: Frame-by-frame comparison to check if video content is the same (beyond simple checksums)
- **Sync Playback**: Synchronized playback of both videos for real-time comparison
- **Timestamp Navigation**: Jump to specific timestamps in both videos simultaneously

### Track Transfer
- **Audio/Subtitle Transfer**: Transfer audio tracks and subtitles from one video file to another without re-encoding
- **Track Selection**: Choose specific audio and subtitle tracks to transfer
- **Format Preservation**: Maintains original quality by copying streams without transcoding
- **Custom Output Naming**: Configure output file postfix to avoid overwriting originals

### Batch Processing
- **Directory-based Processing**: Select source and target directories for batch operations
- **Intelligent File Matching**: Automatic matching of source and target files based on filename similarity
- **Manual Reordering**: Drag and drop to manually reorder file matching
- **Progress Tracking**: Real-time progress indication and logging during batch operations

## Requirements

### System Dependencies
- **Qt6**: Core, Widgets, Multimedia, MultimediaWidgets
- **FFmpeg**: Development libraries (libavcodec, libavformat, libavutil, libswscale, libswresample)
- **CMake**: Version 3.16 or higher
- **C++ Compiler**: Supporting C++17 standard

### Ubuntu/Debian Installation
```bash
sudo apt update
sudo apt install qt6-base-dev qt6-multimedia-dev cmake build-essential
sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
```

### Arch Linux Installation
```bash
sudo pacman -S qt6-base qt6-multimedia cmake gcc
sudo pacman -S ffmpeg
```

## Building

### Quick Build
```bash
./build.sh
```

### Manual Build
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Usage

### Running the Application
```bash
cd build
./VideoMaster
```

### Video Comparison Tab
1. Drag and drop two video files into the left and right video areas
2. Use the "Sync Playback" button to play both videos simultaneously
   - Button toggles between "Sync Playback" and "Sync Pause"
   - Controls both videos together - play together, pause together
3. Use the timestamp slider to jump to specific points in both videos
   - Slider becomes active when videos are loaded
   - Immediately seeks both videos when moved
   - Updates in real-time during playback
4. The application will analyze visual differences between the videos

### Track Transfer Tab
1. **Load Videos:**
   - **📁 Source Video (Green)**: Drag & drop the video containing tracks to copy FROM
   - **🎯 Target Video (Blue)**: Drag & drop the base video to copy tracks TO
   - Color-coded borders and backgrounds make the distinction crystal clear

2. **Track Merging System (NEW - Preserves Existing Tracks!):**
   - **📁 Source tracks**: Start UNCHECKED - check to ADD tracks from source video
   - **🎯 Target tracks**: Start CHECKED - uncheck to REMOVE existing tracks
   - **Track Preservation**: By default, all existing tracks are preserved and new ones are added
   - **Visual Legend**: Color-coded icons clearly show which video each track comes from

3. **Select Tracks Using Templates:**
   - **Manual Selection**: Check/uncheck individual tracks in the lists
   - **Template Matching**: Use wildcards for automatic selection:
     - `*eng*` - Select all English tracks
     - `*jpn*` - Select all Japanese tracks
     - `*ac3*` - Select all AC3 audio tracks
     - `*srt*` - Select all SRT subtitle tracks
   - **Quick Actions**: Use "All", "Clear" buttons for bulk operations

4. **Execute Transfer:**
   - Configure output postfix (default: "_merged")
   - Click "Transfer Selected Tracks" when ready
   - **SAFE by default**: Preserves existing tracks and adds selected ones
   - No re-encoding - maintains original quality

### Batch Processing Tab
1. **Input Directories:**
   - **📁 Tracks Input Directory (Green)**: Videos containing tracks to copy FROM
   - **🎯 Base Videos Input Directory (Blue)**: Videos that will receive tracks
   - **💾 Output Directory (Orange)**: Where merged videos will be saved

2. **File Matching & Pairing:**
   - **🤖 Auto Match**: Automatically pairs files based on similar names
   - **Manual Reordering**: Use ⬆️⬇️ buttons to adjust file matching
   - **Color-coded lists** clearly show which files provide tracks vs. receive them

3. **Track Selection (NEW - Template-Based Merging):**
   - **Template-only approach**: Due to batch scale, uses template selection from source files
   - **Track Preservation by Default**: Existing tracks in target videos are preserved automatically
   - **Template matching** with wildcards (`*eng*`, `*jpn*`, etc.) selects tracks to ADD
   - **Quick selection** buttons (All, Clear, Apply) for source tracks
   - **Optional destructive mode**: Checkbox to remove existing tracks (use with caution)

4. **Batch Processing:**
   - **Safe by default**: Adds selected source tracks while preserving existing target tracks
   - **Output postfix** configuration (e.g., "_merged")
   - **🚀 Start Batch Processing** with progress tracking
   - **⏹️ Stop Processing** button to cancel batch operation at any time
   - **Real-time log** showing processing status and merge results
   - **Progress tracking** with ability to see partial completion if cancelled

## File Support

### Supported Video Formats
- MP4 (.mp4)
- AVI (.avi)
- Matroska (.mkv)
- QuickTime (.mov)
- Windows Media (.wmv)
- Flash Video (.flv)
- WebM (.webm)
- MPEG-4 (.m4v)

### Track Types
- **Audio**: All FFmpeg-supported audio codecs
- **Subtitles**: SRT, ASS, VTT, and other text-based subtitle formats

## Important Notes

- **Original Files**: The application never modifies original files
- **SAFE TRACK MERGING**: By default, existing tracks in target videos are preserved - new tracks are ADDED, not replaced
- **Quality Preservation**: Track transfer uses stream copying, maintaining original quality
- **No Re-encoding**: Audio and subtitle tracks are copied without transcoding
- **Postfix Safety**: Output files use configurable postfixes to prevent overwrites
- **Track Preservation**: The destructive behavior of removing existing tracks is explicitly optional and clearly marked

## Architecture

The application is built with a modular architecture:

- **MainWindow**: Primary UI coordination and tab management
- **VideoWidget**: Drag & drop video player components
- **VideoComparator**: Frame-by-frame video comparison engine
- **FFmpegHandler**: Low-level FFmpeg integration for video processing
- **BatchProcessor**: Batch operation management and file matching

## Troubleshooting

### Build Issues
- Ensure all Qt6 and FFmpeg development packages are installed
- Check that CMake can find Qt6 with `cmake --find-package Qt6`
- Verify FFmpeg libraries are available with `pkg-config --modversion libavcodec`

### Runtime Issues
- Confirm FFmpeg command-line tool is available in PATH
- Check video file permissions and formats
- Ensure sufficient disk space for output files

### FFmpeg Messages
- The application automatically suppresses verbose FFmpeg warnings about attachments and unknown codecs
- Only actual errors are displayed to keep the interface clean
- These messages don't affect functionality - they're just informational

## License

This project is provided as-is for educational and personal use.