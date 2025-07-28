# Implementation Plan: First Deliverable

## Overview
Build a minimalistic neuromorphic screen capture system that records 5-second bursts of screen activity as DVS-style events and replays them with neuromorphic visualization (moving dots that outline shapes).

## Phase 1: Core Infrastructure (Week 1)

### 1.1 Project Setup
- [ ] Create CMake build system with Visual Studio integration
- [ ] Set up project structure following the file layout in README
- [ ] Configure NVIDIA Capture SDK paths and dependencies
- [ ] Create basic DirectX 11 initialization utilities

### 1.2 Event Data Structures
```cpp
// src/core/event_types.h
struct Event {
    uint64_t timestamp;  // microseconds since epoch
    uint16_t x, y;       // pixel coordinates
    int8_t polarity;     // +1 for increase, -1 for decrease
};

struct EventStream {
    std::vector<Event> events;
    uint32_t width, height;  // screen dimensions
    uint64_t start_time;     // recording start timestamp
};
```

### 1.3 High-Resolution Timing
```cpp
// src/core/timing.h
class HighResTimer {
public:
    static uint64_t GetMicroseconds();
    static double GetSeconds();
    static void SleepMicroseconds(uint64_t microseconds);
};
```

### 1.4 DirectX 11 Setup
- [ ] Create device and device context
- [ ] Set up swap chain for visualization window
- [ ] Create render target and depth stencil
- [ ] Implement basic shader compilation utilities

## Phase 2: NVFBC Capture (Week 2)

### 2.1 NVFBC Initialization
```cpp
// src/capture/nvfbc_capture.cpp
class NVFBCCapture {
private:
    NVFBCToSys* m_fbc;
    void* m_diffMapBuffer;
    uint32_t m_width, m_height;
    uint8_t* m_frameBuffer;
    
public:
    bool Initialize();
    bool CaptureFrame(EventStream& events);
    void Cleanup();
};
```

### 2.2 Frame Capture Implementation
- [ ] Initialize NVFBC with difference map enabled
- [ ] Set up 16×16 block size for difference detection
- [ ] Implement frame capture loop with 16ms timeout
- [ ] Extract difference map from captured frames

### 2.3 Event Generation
```cpp
// src/capture/event_generator.cpp
class EventGenerator {
private:
    uint8_t* m_previousDiffMap;
    uint32_t m_blockWidth, m_blockHeight;
    
public:
    void ProcessDiffMap(uint8_t* diffMap, EventStream& events, uint64_t timestamp);
    void SetChangeThreshold(float threshold);
};
```

### 2.4 5-Second Burst Recording
- [ ] Implement recording timer (5 seconds)
- [ ] Add recording state management
- [ ] Create event stream accumulation
- [ ] Add recording progress feedback

## Phase 3: Event Storage (Week 3)

### 3.1 Binary Event File Format
```
[Header]
- Magic number: "NEVS" (4 bytes)
- Version: 1 (4 bytes)
- Screen width: uint32_t (4 bytes)
- Screen height: uint32_t (4 bytes)
- Start timestamp: uint64_t (8 bytes)
- Event count: uint32_t (4 bytes)

[Events]
- timestamp: uint64_t (8 bytes)
- x: uint16_t (2 bytes)
- y: uint16_t (2 bytes)
- polarity: int8_t (1 byte)
- padding: uint8_t (1 byte) - for alignment
```

### 3.2 Event File I/O
```cpp
// src/core/event_file.h
class EventFile {
public:
    static bool WriteEvents(const EventStream& events, const std::string& filename);
    static bool ReadEvents(EventStream& events, const std::string& filename);
    static bool ValidateFile(const std::string& filename);
};
```

### 3.3 Event Stream Management
- [ ] Implement event sorting by timestamp
- [ ] Add event deduplication (same pixel, same timestamp)
- [ ] Create event stream validation
- [ ] Add memory-efficient event storage

## Phase 4: DVS Visualization (Week 4)

### 4.1 Dot Rendering System
```cpp
// src/visualization/event_renderer.cpp
class EventRenderer {
private:
    ID3D11Buffer* m_vertexBuffer;
    ID3D11VertexShader* m_vertexShader;
    ID3D11PixelShader* m_pixelShader;
    std::vector<DotVertex> m_dots;
    
public:
    bool Initialize(ID3D11Device* device);
    void RenderEvents(const std::vector<Event>& events, float currentTime);
    void UpdateDots(const Event& event, float currentTime);
};
```

### 4.2 Transient Dot Effects
- [ ] Implement 100ms fade-out for dots
- [ ] Add color coding (green for positive, red for negative)
- [ ] Create smooth dot transitions
- [ ] Add dot size variation based on change magnitude

### 4.3 Replay Window
```cpp
// src/visualization/replay_window.cpp
class ReplayWindow {
private:
    HWND m_hwnd;
    IDXGISwapChain* m_swapChain;
    EventStream m_events;
    float m_replayTime;
    bool m_isPlaying;
    
public:
    bool Initialize(const std::string& title, uint32_t width, uint32_t height);
    void LoadEvents(const EventStream& events);
    void StartReplay();
    void Update(float deltaTime);
    void Render();
};
```

### 4.4 Timing Control
- [ ] Implement precise replay timing
- [ ] Add play/pause/stop controls
- [ ] Create replay speed adjustment
- [ ] Add replay progress indicator

## Phase 5: Integration & Testing (Week 5)

### 5.1 Main Application
```cpp
// src/main.cpp
class NeuromorphicScreens {
private:
    NVFBCCapture m_capture;
    EventRenderer m_renderer;
    ReplayWindow m_window;
    EventStream m_currentRecording;
    
public:
    bool Initialize();
    void Run();
    void RecordBurst(uint32_t durationSeconds);
    void ReplayEvents(const std::string& filename);
    void LiveVisualization();
};
```

### 5.2 Command-Line Interface
```cpp
// Command-line options
--record --duration 5 --output filename.evt
--replay --input filename.evt
--live
--help
```

### 5.3 Performance Optimization
- [ ] Profile capture loop performance
- [ ] Optimize event generation (SIMD if needed)
- [ ] Improve rendering performance
- [ ] Add memory pooling for events

### 5.4 Testing Scenarios
- [ ] Static screen (minimal events)
- [ ] Moving window (outline events)
- [ ] Video playback (continuous events)
- [ ] Text typing (sparse events)
- [ ] High-motion content (dense events)

## Implementation Details

### NVFBC Configuration
```cpp
// NVFBC setup parameters
NVFBC_TOSYS_SETUP_PARAMS setupParams = {};
setupParams.bWithHWCursor = true;
setupParams.bDiffMap = true;
setupParams.eDiffMapBlockSize = NVFBC_TOSYS_DIFFMAP_BLOCKSIZE_16X16;
setupParams.ppDiffMap = &m_diffMapBuffer;
setupParams.dwDiffMapBuffSize = diffMapSize;
```

### Event Processing Algorithm
```cpp
void ProcessDiffMap(uint8_t* diffMap, EventStream& events, uint64_t timestamp) {
    for (uint32_t y = 0; y < m_blockHeight; y++) {
        for (uint32_t x = 0; x < m_blockWidth; x++) {
            uint32_t index = y * m_blockWidth + x;
            if (diffMap[index] != 0) {
                // Convert block coordinates to pixel coordinates
                uint16_t pixelX = x * 16;
                uint16_t pixelY = y * 16;
                
                // Determine polarity (simplified - could be more sophisticated)
                int8_t polarity = 1; // Assume positive for now
                
                Event event = {timestamp, pixelX, pixelY, polarity};
                events.events.push_back(event);
            }
        }
    }
}
```

### Visualization Shader
```hlsl
// Vertex shader for dots
struct VSInput {
    float2 position : POSITION;
    float4 color : COLOR;
    float size : SIZE;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

// Pixel shader for dots
float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
```

## Success Criteria

### Functional Requirements
- [ ] Successfully capture 5-second screen activity as events
- [ ] Store events in binary format with <1MB file size for typical usage
- [ ] Replay events with correct timing (±16ms accuracy)
- [ ] Display neuromorphic visualization with moving dots
- [ ] Handle various screen content types (static, dynamic, mixed)

### Performance Requirements
- [ ] Capture overhead <5% CPU usage
- [ ] Event generation <1ms per frame
- [ ] Smooth 60 FPS visualization
- [ ] <100MB memory usage for 5-second recordings

### Quality Requirements
- [ ] Dots clearly outline moving shapes
- [ ] Polarity colors are distinguishable
- [ ] Transient effects are smooth
- [ ] Replay timing matches original

## Risk Mitigation

### NVFBC Availability
- **Risk**: NVFBC not available on RTX 3050
- **Mitigation**: Implement Desktop Duplication API fallback
- **Detection**: Test NVFBC initialization on target hardware

### Performance Issues
- **Risk**: High CPU usage during capture
- **Mitigation**: Profile and optimize event generation
- **Detection**: Monitor CPU usage during recording

### Memory Issues
- **Risk**: Large event streams consuming too much memory
- **Mitigation**: Implement streaming event processing
- **Detection**: Monitor memory usage with various content

## Next Steps After First Deliverable

1. **Real-time streaming** between displays
2. **AI agent integration** with event streams
3. **Advanced visualization** features
4. **Multi-monitor support**
5. **Event compression** algorithms

## Development Timeline

- **Week 1**: Core infrastructure and DirectX setup
- **Week 2**: NVFBC capture and event generation
- **Week 3**: Event storage and file I/O
- **Week 4**: DVS visualization and replay
- **Week 5**: Integration, testing, and optimization

This plan provides a focused path to the first deliverable while maintaining the minimalistic DVS-style approach you requested. 