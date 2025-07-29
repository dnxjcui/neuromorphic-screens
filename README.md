# Neuromorphic Screens

![Maxwell Demo](docs/media/maxwell.gif)

<img src="docs/media/happy.gif" width="500" height="200" alt="Happy Demo" style="display: inline-block; margin-right: 10px;">
<img src="docs/media/happy_original.gif" width="300" height="200" alt="Happy Original" style="display: inline-block;">

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