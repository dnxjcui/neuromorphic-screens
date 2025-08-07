# UDP Event Streaming Test Suite

This directory contains tools for testing and visualizing UDP-based neuromorphic event streaming.

## Fixed Issues (August 2025)

### ✅ UDP Visualization Issues Fixed

**Problem**: UDP visualization showed only occasional pixels with mostly empty screen and 0 active events.

**Root Causes Identified**:
1. **Event Structure Mismatch**: Python parser expected 14-byte events but was parsing incorrectly
2. **Incorrect Field Parsing**: Used wrong data type for polarity (uint8_t vs int8_t)
3. **DVSEvent Structure**: Actual structure is 13 bytes: timestamp(8) + x(2) + y(2) + polarity(1)
4. **Event Window Issues**: Time windows and fade durations didn't match C++ implementation
5. **Stride Parameter**: Changes in GUI weren't being reflected in UDP streaming

**Solutions Implemented**:
- Fixed event parsing to match actual DVSEvent structure (13 bytes)
- Corrected polarity field type: int8_t (signed byte) instead of uint8_t
- Updated fade duration and time windows to match C++ constants exactly
- Verified stride parameter is reactive in UDP event source lambda (already working)
- Added comprehensive debug output and testing tools
- Added optional GPU acceleration support with CuPy

### ✅ Stride Parameter Reactivity Fixed

**Problem**: Changing stride in GUI didn't affect UDP streaming throughput.

**Solution**: Modified event source lambda to use current stride value from StreamingApp:
```cpp
uint32_t currentStride = streamingApp.getStride();
size_t eventsToProcess = (std::min)(eventsCopy.size(), static_cast<size_t>(1500));
if (currentStride > 1) {
    eventsToProcess = eventsToProcess / currentStride;
}
```

## Testing Tools

### 1. Simple UDP Test (`test_udp_simple.py`)
Basic packet reception and event parsing test:
```bash
python test_udp_simple.py
```

### 2. Full Visualization (`neuromorphic_udp_visualizer.py`)
Complete ImGui-based visualization with statistics and optional GPU acceleration:
```bash
# Standard CPU visualization
python neuromorphic_udp_visualizer.py --port 9999

# GPU-accelerated visualization (requires CuPy)
python neuromorphic_udp_visualizer.py --port 9999 --gpu

# Custom screen dimensions
python neuromorphic_udp_visualizer.py --port 9999 --width 1920 --height 1080
```

### 3. Event Receiver Test (`test_event_receiver.py`)
Alternative event receiver implementation for comparison.

## Usage Instructions

### Step 1: Start UDP Streaming
```bash
# From project build directory
./neuromorphic_screens_streaming.exe --UDP --port 9999 --batch 1500
```

### Step 2: Start Visualization
```bash
# In test_UDP directory with virtual environment
env\Scripts\activate
python neuromorphic_udp_visualizer.py --port 9999
```

### Step 3: Verify Operation
- Check console output for "UDP: Generated X events" and "UDP: Sent packet" messages
- Verify Python visualizer shows "Received packet: X bytes, Y events"
- Confirm active dots appear in visualization window
- Test stride parameter changes affect event density

## Debug Information

The updated code includes comprehensive debug output:

**C++ UDP Streamer**:
- Event generation frequency
- Packet send success/failure
- Throughput and drop statistics

**Python Visualizer**:
- Packet reception details
- Event parsing results
- Active dot statistics
- Sample event coordinates and properties

## Performance Notes

- **Event Structure**: 13-byte DVSEvent (timestamp:8, x:2, y:2, polarity:1)
- **Event Window**: 100ms for active dots (matches C++ DOT_FADE_DURATION)
- **Fade Duration**: 100ms (matches C++ constants exactly)
- **Dot Size**: 2.0 pixels (matches C++ DOT_SIZE constant)
- **Stride Reactivity**: Real-time parameter updates affect UDP throughput
- **GPU Acceleration**: Optional CuPy support for coordinate transformations

## Troubleshooting

If visualization still shows no events:

1. **Check UDP Reception**: Run `test_udp_simple.py` first
2. **Verify Event Generation**: Look for "UDP: Generated X events" in C++ output
3. **Check Packet Sending**: Look for "UDP: Sent packet" messages
4. **Validate Coordinates**: Ensure events have valid x,y coordinates (0-1920, 0-1080)
5. **Monitor Statistics**: Check active dots count in Python visualizer

The fixes ensure UDP visualization now mirrors the ImGui implementation with proper event parsing, responsive parameter changes, and comprehensive debugging capabilities.