# Neuromorphic Screens

![Happy Demo](docs/media/happy.gif)
![Maxwell Demo](docs/media/maxwell.gif)

A real-time event-based screen capture system that converts screen changes into neuromorphic events, inspired by Dynamic Vision Sensors (DVS). This system captures pixel-level changes as asynchronous, timestamped events and provides advanced visualization with professional ImGui interface, dimming effects, and export capabilities.

## Features

- **Real Screen Capture**: Desktop Duplication API integration for actual screen capture  
- **Multi-Format Storage**: CSV, binary (.evt), and space-separated text formats
- **rpg_dvs_ros Compatibility**: Space-separated format compatible with ROS DVS packages
- **Advanced ImGui Visualization**: DirectX 11-based interface with stable, segfault-free operation
- **Professional Export**: FFmpeg integration for GIF and MP4 video export
- **Event-Based Timing**: True event-based visualization - all events at same timestamp display simultaneously
- **Pixel Dimming Effects**: Configurable dimming for previous pixels instead of immediate removal
- **High-Performance**: 60 FPS rendering with hardware acceleration and OpenMP parallelization
- **Interactive Controls**: Play/Pause/Stop, speed control (0.01x-5.0x), and real-time statistics

## Quick Start

### Build Requirements
- Windows 11
- Visual Studio 2019/2022  
- CMake 3.16+
- Dear ImGui (automatically configured at `C:/Program Files/imgui`)

### Building
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Usage

**Record screen activity:**
```bash
./neuromorphic_screens.exe --capture --output recording.evt --duration 5 --format binary
```

**Visualize events (ImGui GUI - RECOMMENDED):**
```bash
./neuromorphic_screens_imgui.exe --input recording.evt
```

**CLI statistics:**
```bash
./neuromorphic_screens.exe --replay --input recording.evt
```

## Command Line Options

```bash
# Capture screen activity
./neuromorphic_screens.exe --capture --output <file> --duration <seconds> --format <csv|binary|txt>

# Replay events with statistics
./neuromorphic_screens.exe --replay --input <file>

# Launch ImGui GUI (recommended)
./neuromorphic_screens_imgui.exe --input <file>
```

## File Formats

- **Binary (.evt)**: Efficient binary format for large datasets (recommended)
- **CSV**: Comma-separated with headers (`timestamp,x,y,polarity`)
- **Space-separated**: Compatible with rpg_dvs_ros (`timestamp x y polarity`)

## ImGui GUI Features

### Core Visualization
- **Stable Operation**: Zero segmentation faults with DirectX 11 backend
- **Automatic Playback**: Video-like operation - plays immediately when loaded
- **Event-Based Timing**: All pixels recorded at one timepoint visualize simultaneously
- **60 FPS Rendering**: Smooth, hardware-accelerated visualization
- **Thread-Safe Architecture**: Mutex-protected event processing

### Advanced Controls
- **Speed Control**: 0.01x to 5.0x playback speed with precise adjustment
- **Dimming Effects**: Gradual pixel dimming instead of instant removal
- **Downsample Options**: 1x to 8x visualization downsampling for performance
- **Progress Seeking**: Jump to any point in the recording
- **Real-Time Statistics**: Event count, polarity distribution, and active dots

### Export Functionality
- **GIF Export**: High-quality animated GIF with palette optimization
- **Video Export**: MP4 video with H.264 encoding
- **FFmpeg Integration**: Professional-grade encoding with configurable settings
- **Screen Capture**: Records the entire visualization window

### Interface Layout
- **Event Canvas**: Main visualization area with coordinate scaling
- **Control Panel**: Play/pause/stop controls and speed adjustment
- **Statistics Panel**: Real-time event metrics and recording information
- **Export Panel**: GIF/video export options with duration and FPS settings

## Performance

- **Capture Rate**: 60 FPS with OpenMP parallelization
- **Event Generation**: 95,000+ events per second tested
- **Memory Efficient**: 13 bytes per event with optimized storage
- **Visualization**: Hardware-accelerated 60 FPS with DirectX 11
- **Export Quality**: Professional FFmpeg encoding for high-quality output

## Architecture

```
src/
├── core/                   # Event data structures and timing
├── capture/               # Desktop Duplication screen capture
├── visualization/         # ImGui DirectX 11 GUI (recommended)
└── main.cpp              # Command-line interface
    main_imgui.cpp        # ImGui GUI application
```

## Key Improvements

### ✅ ImGui Implementation (Stable & Professional)
- **Eliminated Segmentation Faults**: Complete replacement of problematic FLTK implementation
- **DirectX 11 Backend**: Native Windows integration with hardware acceleration
- **Thread-Safe Design**: Dedicated replay thread with mutex protection
- **Professional Interface**: Modern UI with organized control panels

### ✅ True Event-Based Visualization
- **Fixed Frame Limiting Bug**: Removed artificial 200 events/frame limit
- **Simultaneous Event Display**: All events at same timestamp now visualize together
- **Proper Timing**: Event processing based on actual timestamps, not linear progression

### ✅ Advanced Features
- **Pixel Dimming**: Configurable gradual dimming instead of instant removal
- **FFmpeg Export**: Professional GIF and video export with quality optimization
- **Performance Optimization**: OpenMP parallelization for capture and rendering

## Usage Examples

### Quick Recording and Visualization
```bash
# Record 5 seconds of screen activity
./neuromorphic_screens.exe --capture --output test.evt --duration 5

# Launch ImGui viewer (automatic playback)
./neuromorphic_screens_imgui.exe --input test.evt
```

### Export Options
1. **Load your recording** in the ImGui GUI
2. **Click "Export Options"** button in the control panel
3. **Configure settings**: Duration (1-30s), FPS (15-60), filename
4. **Choose format**: Click "Export GIF" or "Export Video"
5. **Files saved** to project directory with professional quality

### Performance Tips
- Use **binary (.evt) format** for fastest loading
- Enable **dimming effects** for better visual quality
- Adjust **downsample factor** for dense recordings
- Export at **30 FPS** for optimal file size/quality balance

## License

MIT License - See LICENSE file for details.

The ImGui implementation provides a stable, professional visualization platform that eliminates all previous stability issues while delivering superior performance and user experience.