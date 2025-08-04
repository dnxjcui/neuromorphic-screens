# UDP Testing and Visualization Scripts

This directory contains Python scripts for testing and visualizing UDP event streams from the neuromorphic screens project.

## Scripts

### neuromorphic_udp_visualizer.py
**Professional ImGui-based real-time visualizer** that mimics the C++ ImGui implementation with two main components:
- **Large neuromorphic screen visualization**: Real-time event display with green/red dots for positive/negative events
- **Dynamic event capture plot**: 5-second sliding window showing events per second over time

**Features:**
- Professional Python ImGui interface matching C++ implementation
- Real-time event visualization with coordinate scaling
- Dynamic matplotlib-based event rate plotting  
- Performance monitoring and statistics
- Thread-safe UDP event reception
- Fade effects and polarity-based coloring

**Dependencies:**
```bash
# Install in virtual environment
env/Scripts/activate  # Windows
pip install imgui[glfw] PyOpenGL matplotlib pillow numpy
```

**Usage:**
```bash
# Using virtual environment (recommended)
env/Scripts/activate
python neuromorphic_udp_visualizer.py --port 9999

# Custom configuration
python neuromorphic_udp_visualizer.py --port 9999 --width 1920 --height 1080
```

### test_event_receiver.py
**Simple console-based UDP receiver** for basic testing and debugging. Provides detailed packet statistics and event validation.

**Features:**
- Console-based output with real-time statistics
- Event validation and coordinate bounds checking
- Polarity distribution analysis
- High-performance packet processing
- Increased buffer sizes for high-throughput streams

**Usage:**
```bash
# Basic usage (listens on port 9999)
python test_event_receiver.py

# Custom configuration  
python test_event_receiver.py --port 9999 --buffer 131072
```

## Testing Workflow

### 1. Professional Visualization (Recommended)
```bash
# Terminal 1: Start C++ UDP streamer
cd build/bin/Release
./neuromorphic_screens_streaming.exe --UDP --port 9999 --batch 1500

# Terminal 2: Start Python ImGui visualizer
cd src/test_UDP
env/Scripts/activate
python neuromorphic_udp_visualizer.py --port 9999
```

### 2. Console Testing
```bash
# Terminal 1: Start C++ UDP streamer
cd build/bin/Release  
./neuromorphic_screens_streaming.exe --UDP --port 9999

# Terminal 2: Start console receiver
cd src/test_UDP
python test_event_receiver.py --port 9999
```

## Visualization Features

### Main Canvas
- **Real-time event display**: 800x600 pixel canvas showing neuromorphic events
- **Coordinate scaling**: Automatic scaling from screen coordinates (1920x1080) to canvas
- **Polarity coloring**: Green dots for positive events, red for negative events
- **Fade effects**: 100ms fade duration for natural-looking visualization
- **Performance optimization**: Limited to 10,000 active dots for smooth rendering

### Event Rate Plot
- **5-second sliding window**: Dynamic plotting of events per second
- **Real-time updates**: 60 FPS plot refresh rate
- **Matplotlib integration**: High-quality plotting with OpenGL texture rendering
- **Statistics panel**: FPS, events/sec, active dots, total events

### Performance Monitoring
- **UDP statistics**: Packets received, total events, data throughput
- **Rendering performance**: Frame rate monitoring and optimization
- **Connection status**: Real-time connection state and error handling

## Expected Performance

### High-Throughput Testing
- **Event rates**: Supports 100K+ events/sec sustained
- **Packet rates**: 50+ packets/sec with 1500 events per packet
- **Throughput**: 3-20 MB/s network throughput
- **Latency**: <10ms visualization latency for real-time feedback

### Example Console Output
```
UDP Event Receiver started on port 9999
Event 0: t=1673025123456789, x=543, y=321, pol=1
Event 1: t=1673025123456890, x=544, y=322, pol=0
Packets: 50, Events: 75000, Rate: 25.3 pkt/s, 37500 evt/s
```

## Virtual Environment Setup

The visualizer requires the existing virtual environment:

```bash
# Windows
env\Scripts\activate

# Verify dependencies
python -c "import imgui, OpenGL.GL, matplotlib, numpy; print('All dependencies OK')"
```

## Troubleshooting

### Visualization Issues
1. **ImGui not rendering**: Verify OpenGL drivers and GLFW installation
2. **Plot not updating**: Check matplotlib backend and texture creation
3. **Poor performance**: Reduce max active dots or increase fade duration

### Network Issues  
1. **No events received**: Check C++ streamer is running with `--UDP` flag
2. **Packet loss**: Increase buffer size or reduce event generation rate
3. **Port conflicts**: Verify port availability and firewall settings

### Dependencies
1. **Missing ImGui**: Install with `pip install imgui[glfw]`
2. **OpenGL errors**: Update graphics drivers
3. **Virtual environment**: Activate with `env/Scripts/activate`

## Performance Optimization

- **Large UDP buffers**: 131KB receive buffers for high-throughput
- **Efficient event parsing**: Optimized binary data processing
- **Thread-safe design**: Separate UDP and rendering threads
- **Memory management**: Bounded event collections and fade effects
- **Real-time adaptation**: Automatic performance scaling based on load