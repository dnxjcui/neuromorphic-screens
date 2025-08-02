# Neuromorphic Screens

![Maxwell Demo](docs/media/maxwell.gif)
![Bernstein Demo](docs/media/bernstein.gif)

<!-- <img src="docs/media/happy.gif" width="400" height="200" alt="Happy Demo" style="display: inline-block; margin-right: 10px;"> -->
<!-- <img src="docs/media/happy_original.gif" width="200" height="200" alt="Happy Original" style="display: inline-block;"> -->

A real-time event-based screen capture system that converts screen changes into neuromorphic events, inspired by Dynamic Vision Sensors (DVS). This system captures pixel-level changes as asynchronous, timestamped events and provides advanced visualization with professional ImGui interface, direct overlay mode, streaming capabilities, and configurable parameters.


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
- **Monitor performance stats** in console output to verify optimization benefits:
  ```
  Streaming GUI: 2834 active dots, buffer: 3200, processed: 125847, duplicates: 87432
  Overlay: 1203 active dots, buffer: 1450, processed: 89234, duplicates: 53219
  ```

## License

Apache 2.0 License - See LICENSE file for details.
## Performance Architecture

The system now employs a sophisticated performance optimization stack:

### Core Optimizations
1. **TemporalEventIndex**: High-performance ring buffer with O(k) recent event access
2. **Dirty Region Tracking**: Selective pixel clearing to minimize memory bandwidth
3. **Event Deduplication**: Unique event ID system prevents duplicate processing
4. **Batch Processing**: Efficient bulk updates instead of individual event operations

### Technical Implementation
- **Thread-Safe Design**: Mutex-protected data structures with lock-free optimizations
- **Memory Efficient**: Configurable time windows (100ms default) and buffer limits
- **Real-Time Monitoring**: Performance counters for processed events and duplicates
- **Adaptive Scaling**: Automatically handles varying event densities

The ImGui implementation provides a stable, professional visualization platform that eliminates all previous stability issues while delivering superior performance and user experience with **20-1500x performance improvements** for high event rate scenarios.

## Project Structure

```
neuromorphic_screens/
├── CMakeLists.txt              # Build configuration with 4 executable targets
├── LICENSE                     # Apache 2.0 license
├── README.md                   # This documentation file
├── REPORT.md                   # Comprehensive technical report
├── imgui.ini                   # ImGui window layout configuration
├── benchmark_parallelization.cpp  # OpenMP performance benchmarking tool
├── build_benchmark.bat         # Windows script to build and run benchmarks
│
├── data/                       # Directory for captured event files
│
├── docs/                       # Documentation and media assets
│   └── media/                  # Demo GIFs and visualizations
│       ├── bernstein.gif       # Demonstration recording
│       ├── maxwell.gif         # Demonstration recording
│       ├── happy.gif           # Demonstration recording
│       └── happy_original.gif  # Original comparison recording
│
└── src/                        # Source code organized by functionality
    ├── main.cpp                # CLI application for capture/replay with statistics
    ├── main_imgui.cpp          # ImGui GUI application entry point
    ├── main_streaming.cpp      # Real-time streaming application entry point
    ├── main_overlay.cpp        # Direct screen overlay application entry point
    ├── streaming_app.cpp       # Shared streaming application logic
    ├── streaming_app.h         # Streaming application interface and configuration
    │
    ├── capture/                # Screen capture system using Desktop Duplication API
    │   ├── screen_capture.cpp  # DirectX 11 screen capture implementation
    │   └── screen_capture.h    # Screen capture interface and pixel processing
    │
    ├── core/                   # Core data structures and algorithms
    │   ├── event_types.h       # Event data structures, EventStream, and constants
    │   ├── event_file.cpp      # Event file I/O operations
    │   ├── event_file.h        # Event file interface
    │   ├── event_file_formats.cpp  # Multi-format event file serialization
    │   ├── event_file_formats.h    # File format definitions (AEDAT, CSV, space-separated)
    │   ├── temporal_index.h    # High-performance temporal event indexing with O(k) access
    │   ├── timing.cpp          # High-resolution timing implementation
    │   └── timing.h            # Microsecond precision timing utilities
    │
    └── visualization/          # Event visualization and GUI components
        ├── imgui_event_viewer.cpp   # Professional event viewer with playback controls
        ├── imgui_event_viewer.h     # Video-like interface with export capabilities
        ├── imgui_streaming_viewer.cpp  # Real-time streaming visualization window
        ├── imgui_streaming_viewer.h    # Live event display with parameter controls
        ├── direct_overlay_viewer.cpp   # Screen overlay rendering engine
        └── direct_overlay_viewer.h    # Direct screen event visualization
```

### Core Components

#### **Executables** (4 Applications)
- **`neuromorphic_screens.exe`** - CLI application for event capture and replay with detailed statistics
- **`neuromorphic_screens_imgui.exe`** - Professional ImGui GUI with video-like playback controls and export features
- **`neuromorphic_screens_streaming.exe`** - Real-time event streaming with live visualization and parameter adjustment
- **`neuromorphic_screens_overlay.exe`** - Direct screen overlay mode displaying events as colored dots on your desktop

#### **Screen Capture System** (`src/capture/`)
- **`screen_capture.cpp/.h`** - Desktop Duplication API implementation with OpenMP parallelization for high-performance pixel-by-pixel change detection, supporting configurable thresholds and stride patterns

#### **Core Event System** (`src/core/`)
- **`event_types.h`** - Fundamental neuromorphic event data structures including Event, EventStream, and system constants
- **`event_file.cpp/.h`** - Core event file I/O operations with buffering and error handling
- **`event_file_formats.cpp/.h`** - Multi-format serialization supporting AEDAT binary, CSV, and space-separated text formats
- **`temporal_index.h`** - High-performance temporal indexing with O(k) recent event access, eliminating O(n) linear scanning bottlenecks
- **`timing.cpp/.h`** - Microsecond precision timing utilities with frame rate limiting and high-resolution timestamp generation

#### **Visualization Engines** (`src/visualization/`)
- **`imgui_event_viewer.cpp/.h`** - Professional DirectX 11-based event viewer with video-like playback controls, progress seeking, speed adjustment, and GIF/MP4 export capabilities
- **`imgui_streaming_viewer.cpp/.h`** - Real-time streaming visualization with configurable parameters (threshold, stride, max events) and live performance monitoring
- **`direct_overlay_viewer.cpp/.h`** - Direct screen overlay rendering engine using layered windows with efficient dirty region tracking and GDI-based dot rendering

#### **Application Architecture** (`src/`)
- **`streaming_app.cpp/.h`** - Shared streaming application logic with thread-safe event capture, configurable parameters, and optional file saving
- **`main_*.cpp`** - Application entry points for each of the four distinct usage modes (CLI, GUI, streaming, overlay)

#### **Performance Tools**
- **`benchmark_parallelization.cpp`** - OpenMP benchmarking tool for testing pixel processing performance with serial vs parallel implementations
- **`build_benchmark.bat`** - Windows build script for performance testing

### File Formats Supported

- **Binary (.evt/.aedat)**: High-performance AEDAT binary format for large datasets
- **CSV (.csv)**: Human-readable comma-separated format with headers
- **Space-separated (.txt)**: rpg_dvs_ros compatible format for ROS integration

### Performance Optimizations

- **TemporalEventIndex**: Ring buffer with O(k) recent event access
- **Event Deduplication**: Unique event ID system preventing duplicate processing  
- **Dirty Region Tracking**: Selective rendering for minimal memory bandwidth usage
- **OpenMP Parallelization**: Multi-threaded pixel processing for real-time capture
- **Mutex-Protected Threading**: Thread-safe data structures with lock-free optimizations
