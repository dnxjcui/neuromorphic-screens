# Neuromorphic Screens

A real-time event-based screen capture system that converts screen changes into neuromorphic events, inspired by Dynamic Vision Sensors (DVS). This system captures pixel-level changes as asynchronous, timestamped events and provides advanced visualization with downsampling and speed controls.

## Features

- **Real Screen Capture**: Desktop Duplication API integration for actual screen capture  
- **Multi-Format Storage**: CSV, binary (.evt), and space-separated text formats
- **rpg_dvs_ros Compatibility**: Space-separated format compatible with ROS DVS packages
- **Advanced GUI Visualization**: FLTK-based interface with speed control (0.01x-5.0x) and downsampling (1x-8x)
- **High-Resolution Timing**: Microsecond precision timing with unique event timestamps
- **Interactive Controls**: Play/Pause/Stop, speed slider, progress seeking, and real-time statistics

## Quick Start

### Build Requirements
- Windows 11
- Visual Studio 2019/2022  
- CMake 3.16+
- FLTK 1.4+

### Building
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### Usage

**Record screen activity:**
```bash
./neuromorphic_screens.exe --capture --output recording.csv --duration 5 --format csv
```

**Visualize events:**
```bash
./neuromorphic_screens_gui.exe --input recording.csv
```

**Test system:**
```bash
./neuromorphic_screens.exe --test
```

## Command Line Options

```bash
# Capture screen activity
./neuromorphic_screens.exe --capture --output <file> --duration <seconds> --format <csv|binary|txt>

# Replay events with statistics
./neuromorphic_screens.exe --replay --input <file>

# Generate test data
./neuromorphic_screens.exe --generate-test-data --output <file> --format <format>

# Launch GUI
./neuromorphic_screens_gui.exe --input <file>
```

## File Formats

- **CSV**: Comma-separated with headers (`timestamp,x,y,polarity`)
- **Binary (.evt)**: Efficient binary format for large datasets
- **Space-separated**: Compatible with rpg_dvs_ros (`timestamp x y polarity`)

## GUI Controls

- **Speed Slider**: 0.01x to 5.0x playback speed
- **Downsample Slider**: 1x to 8x visualization downsampling (reduces rendering load)
- **Progress Slider**: Seek to specific time in recording
- **Statistics Panel**: Real-time event count, polarity distribution, and performance metrics

## Performance

- **Capture Rate**: 60 FPS with configurable pixel stride
- **Event Generation**: Up to 19,000+ events per second tested
- **Memory Efficient**: 13 bytes per event with optimized storage
- **Visualization**: Smooth 60 FPS playback with downsampling options

## Architecture

```
src/
├── core/                   # Event data structures and timing
├── capture/               # Desktop Duplication screen capture
├── visualization/         # FLTK GUI and event rendering
└── main.cpp              # Command-line interface
```

## License

MIT License

Copyright (c) 2024

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.