# Neuromorphic Screens

![Maxwell Demo](docs/media/maxwell.gif)
![Bernstein Demo](docs/media/bernstein.gif)

<!-- <img src="docs/media/happy.gif" width="400" height="200" alt="Happy Demo" style="display: inline-block; margin-right: 10px;"> -->
<!-- <img src="docs/media/happy_original.gif" width="200" height="200" alt="Happy Original" style="display: inline-block;"> -->

A real-time event-based screen capture system that converts screen changes into neuromorphic events, inspired by Dynamic Vision Sensors (DVS). This system captures pixel-level changes as asynchronous, timestamped events and provides advanced visualization with professional ImGui interface, direct overlay mode, streaming capabilities, and configurable parameters.

## Features

- **Real Screen Capture**: Desktop Duplication API integration for actual screen capture  
- **Multi-Format Storage**: CSV, binary (.evt), AEDAT, and space-separated text formats
- **rpg_dvs_ros Compatibility**: Space-separated format compatible with ROS DVS packages
- **Advanced ImGui Visualization**: DirectX 11-based interface with stable, segfault-free operation
- **Direct Overlay Mode**: Events displayed directly on your screen as colored dots (green=positive, red=negative)
- **Real-Time Streaming**: Live event capture and visualization with configurable parameters
- **Professional Export**: FFmpeg integration for GIF and MP4 video export
- **Event-Based Timing**: True event-based visualization - all events at same timestamp display simultaneously
- **Pixel Dimming Effects**: Configurable dimming for previous pixels instead of immediate removal
- **High-Performance**: 60 FPS rendering with hardware acceleration and OpenMP parallelization
- **Interactive Controls**: Play/Pause/Stop, speed control (0.01x-5.0x), threshold/stride/max events adjustment, and real-time statistics
- **Resizable GUI**: All control windows are fully resizable with professional layouts

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

**Real-time streaming with live visualization:**
```bash
./neuromorphic_screens_streaming.exe --save recording.aedat --format aedat
```

**Direct overlay mode (events on screen):**
```bash
./neuromorphic_screens_overlay.exe --save overlay_capture.aedat --format aedat
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

# Real-time streaming mode
./neuromorphic_screens_streaming.exe [--save <file>] [--format <aedat|csv>]

# Direct overlay mode
./neuromorphic_screens_overlay.exe [--save <file>] [--format <aedat|csv|space>] [--dimming <rate>] [--no-dimming]
```

## Application Modes

### 1. ImGui Visualization (`neuromorphic_screens_imgui.exe`)
- Professional DirectX 11 interface with video-like playback
- Interactive controls: Play/Pause/Stop, speed control, progress seeking
- Export to GIF and MP4 with FFmpeg integration
- Real-time statistics and event analysis

### 2. Real-Time Streaming (`neuromorphic_screens_streaming.exe`)  
- Live event capture and visualization in a resizable window
- **Configurable Parameters**: Threshold (0-100), Stride (1-30), Max Events (1000-100000)
- Real-time parameter adjustment with immediate visual feedback
- Optional event saving to file while streaming

### 3. Direct Overlay Mode (`neuromorphic_screens_overlay.exe`)
- Events displayed directly on your screen as colored dots
- **Green dots** = positive events (brightness increase)
- **Red dots** = negative events (brightness decrease)  
- Resizable control window with same parameters as streaming mode
- Perfect for monitoring screen activity in real-time

## File Formats

- **Binary (.evt)**: Efficient binary format for large datasets (recommended)
- **CSV**: Comma-separated with headers (`timestamp,x,y,polarity`)
- **Space-separated**: Compatible with rpg_dvs_ros (`timestamp x y polarity`)

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