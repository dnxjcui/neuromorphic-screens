# FLTK Integration Plan for Neuromorphic Screens

## Overview

FLTK (Fast Light Toolkit) will be integrated to provide a cross-platform GUI for the neuromorphic screens project, replacing the current command-line interface with a user-friendly graphical interface for recording, replay, and visualization.

## Why FLTK?

### Advantages
- **Lightweight**: Small footprint (~1MB) compared to other GUI frameworks
- **Cross-platform**: Windows, Linux, macOS support
- **C++ Native**: Direct C++ integration without bindings
- **Fast**: Minimal overhead, suitable for real-time applications
- **Simple**: Easy to learn and integrate
- **Open Source**: MIT license, no licensing costs

### Disadvantages
- **Basic Styling**: Limited modern UI appearance
- **Smaller Community**: Less documentation and examples than Qt/wxWidgets
- **Limited Widgets**: Fewer built-in widgets than larger frameworks

## Integration Strategy

### Phase 1: Basic GUI Framework
1. **Main Window**: Create primary application window
2. **Control Panel**: Recording controls, file management
3. **Status Display**: Real-time feedback and statistics
4. **Settings Panel**: Configuration options

### Phase 2: Visualization Integration
1. **Event Display**: Real-time event visualization
2. **Replay Controls**: Play/pause/stop with timeline
3. **Statistics Panel**: Event analysis and metrics
4. **Export Options**: Save recordings and statistics

### Phase 3: Advanced Features
1. **Multi-window Support**: Separate windows for different functions
2. **Custom Widgets**: Specialized event display widgets
3. **Plugin System**: Extensible architecture
4. **Theme Support**: Customizable appearance

## Implementation Plan

### 1. Project Structure Updates

```
neuromorphic_screens/
├── src/
│   ├── gui/
│   │   ├── main_window.cpp          # Main application window
│   │   ├── main_window.h
│   │   ├── control_panel.cpp        # Recording controls
│   │   ├── control_panel.h
│   │   ├── event_display.cpp        # Event visualization widget
│   │   ├── event_display.h
│   │   ├── replay_controls.cpp      # Replay timeline and controls
│   │   ├── replay_controls.h
│   │   ├── statistics_panel.cpp     # Event statistics display
│   │   ├── statistics_panel.h
│   │   ├── settings_dialog.cpp      # Configuration dialog
│   │   ├── settings_dialog.h
│   │   └── fltk_utils.h             # FLTK utility functions
│   ├── core/                        # Existing core components
│   ├── capture/                     # Existing capture components
│   └── main_gui.cpp                 # New GUI entry point
├── include/
├── lib/
├── resources/                       # GUI resources
│   ├── icons/
│   ├── themes/
│   └── fonts/
└── CMakeLists.txt                   # Updated build configuration
```

### 2. Core GUI Classes

#### MainWindow Class
```cpp
class MainWindow : public Fl_Window {
private:
    ControlPanel* m_controlPanel;
    EventDisplay* m_eventDisplay;
    ReplayControls* m_replayControls;
    StatisticsPanel* m_statisticsPanel;
    MenuBar* m_menuBar;
    
    // Application state
    bool m_isRecording;
    bool m_isReplaying;
    EventStream m_currentRecording;
    
public:
    MainWindow(int w, int h, const char* title);
    ~MainWindow();
    
    // Event handlers
    void onRecordButton();
    void onStopButton();
    void onLoadFile();
    void onSaveFile();
    void onSettings();
    
    // State management
    void updateStatus(const std::string& status);
    void updateStatistics(const EventStats& stats);
    void showError(const std::string& error);
};
```

#### ControlPanel Class
```cpp
class ControlPanel : public Fl_Group {
private:
    Fl_Button* m_recordButton;
    Fl_Button* m_stopButton;
    Fl_Button* m_loadButton;
    Fl_Button* m_saveButton;
    Fl_Input* m_filenameInput;
    Fl_Choice* m_durationChoice;
    Fl_Box* m_statusBox;
    
public:
    ControlPanel(int x, int y, int w, int h);
    
    // Callbacks
    static void onRecordCallback(Fl_Widget*, void*);
    static void onStopCallback(Fl_Widget*, void*);
    static void onLoadCallback(Fl_Widget*, void*);
    static void onSaveCallback(Fl_Widget*, void*);
    
    // State updates
    void setRecordingState(bool recording);
    void setStatus(const std::string& status);
    void setFilename(const std::string& filename);
};
```

#### EventDisplay Class
```cpp
class EventDisplay : public Fl_Widget {
private:
    std::vector<Event> m_activeEvents;
    uint32_t m_width, m_height;
    float m_scaleX, m_scaleY;
    bool m_showPolarity;
    
public:
    EventDisplay(int x, int y, int w, int h);
    
    // Event management
    void addEvent(const Event& event);
    void clearEvents();
    void setScreenSize(uint32_t width, uint32_t height);
    void setShowPolarity(bool show);
    
    // Drawing
    void draw() override;
    void drawEvent(const Event& event);
    void drawBackground();
    
    // Interaction
    int handle(int event) override;
};
```

#### ReplayControls Class
```cpp
class ReplayControls : public Fl_Group {
private:
    Fl_Button* m_playButton;
    Fl_Button* m_pauseButton;
    Fl_Button* m_stopButton;
    Fl_Slider* m_timelineSlider;
    Fl_Box* m_timeDisplay;
    Fl_Choice* m_speedChoice;
    
    float m_currentTime;
    float m_totalTime;
    bool m_isPlaying;
    float m_playbackSpeed;
    
public:
    ReplayControls(int x, int y, int w, int h);
    
    // Playback control
    void play();
    void pause();
    void stop();
    void setTime(float time);
    void setTotalTime(float time);
    void setPlaybackSpeed(float speed);
    
    // Callbacks
    static void onPlayCallback(Fl_Widget*, void*);
    static void onPauseCallback(Fl_Widget*, void*);
    static void onStopCallback(Fl_Widget*, void*);
    static void onTimelineCallback(Fl_Widget*, void*);
};
```

### 3. FLTK Integration with Existing Code

#### DirectX Integration
```cpp
class DirectXDisplay : public Fl_Widget {
private:
    DirectXDevice* m_dxDevice;
    HWND m_hwnd;
    
public:
    DirectXDisplay(int x, int y, int w, int h);
    ~DirectXDisplay();
    
    // FLTK integration
    void draw() override;
    int handle(int event) override;
    
    // DirectX rendering
    void renderEvents(const std::vector<Event>& events);
    void clear();
    void present();
};
```

#### Event Processing Integration
```cpp
class EventProcessor {
private:
    NVFBCCapture* m_capture;
    EventGenerator* m_generator;
    EventStream m_currentStream;
    
public:
    EventProcessor();
    ~EventProcessor();
    
    // Recording
    bool startRecording(uint32_t duration_seconds);
    bool stopRecording();
    bool isRecording() const;
    
    // Event processing
    void processFrame();
    const EventStream& getCurrentStream() const;
    
    // File operations
    bool saveToFile(const std::string& filename);
    bool loadFromFile(const std::string& filename);
};
```

### 4. Build System Updates

#### CMakeLists.txt Updates
```cmake
# Find FLTK
find_package(FLTK REQUIRED)

# Include FLTK directories
include_directories(${FLTK_INCLUDE_DIR})

# Add GUI source files
set(GUI_SOURCES
    src/gui/main_window.cpp
    src/gui/control_panel.cpp
    src/gui/event_display.cpp
    src/gui/replay_controls.cpp
    src/gui/statistics_panel.cpp
    src/gui/settings_dialog.cpp
    src/gui/fltk_utils.h
    src/main_gui.cpp
)

# Create GUI executable
add_executable(neuromorphic_screens_gui 
    ${SOURCES} 
    ${GUI_SOURCES}
)

# Link FLTK libraries
target_link_libraries(neuromorphic_screens_gui
    ${FLTK_LIBRARIES}
    d3d11
    d3dcompiler
    dxgi
    user32
    kernel32
    gdi32
    winmm
)
```

### 5. User Interface Design

#### Main Window Layout
```
+------------------------------------------+
|  Menu Bar                                |
+------------------------------------------+
|  Control Panel    |  Event Display       |
|  [Record] [Stop]  |                      |
|  [Load] [Save]    |  Real-time event     |
|  Duration: 5s     |  visualization       |
|  Status: Ready    |                      |
+------------------------------------------+
|  Replay Controls  |  Statistics Panel    |
|  [Play] [Pause]   |  Total Events: 1234  |
|  Timeline: [====] |  Events/sec: 567     |
|  Speed: 1x        |  Duration: 5.0s      |
+------------------------------------------+
```

#### Menu Structure
```
File
├── New Recording
├── Open Recording...
├── Save Recording As...
├── Export Statistics...
└── Exit

Recording
├── Start Recording
├── Stop Recording
├── Settings...
└── Duration: 5s/10s/30s

View
├── Show Polarity
├── Show Grid
├── Zoom In
├── Zoom Out
└── Reset View

Help
├── About
└── Documentation
```

### 6. Event Visualization Integration

#### Real-time Event Display
```cpp
void EventDisplay::draw() {
    // Clear background
    fl_color(FL_BLACK);
    fl_rectf(x(), y(), w(), h());
    
    // Draw grid (optional)
    if (m_showGrid) {
        drawGrid();
    }
    
    // Draw active events
    for (const auto& event : m_activeEvents) {
        drawEvent(event);
    }
    
    // Draw UI overlays
    drawOverlays();
}

void EventDisplay::drawEvent(const Event& event) {
    // Calculate screen coordinates
    int screenX = x() + (event.x * m_scaleX);
    int screenY = y() + (event.y * m_scaleY);
    
    // Set color based on polarity
    if (event.polarity > 0) {
        fl_color(FL_GREEN);  // Positive events
    } else {
        fl_color(FL_RED);    // Negative events
    }
    
    // Draw event dot
    fl_circle(screenX, screenY, 2);
}
```

### 7. Threading and Real-time Updates

#### Background Processing
```cpp
class BackgroundProcessor {
private:
    std::thread m_processingThread;
    std::atomic<bool> m_running;
    std::mutex m_eventMutex;
    std::vector<Event> m_pendingEvents;
    
public:
    void start();
    void stop();
    void addEvents(const std::vector<Event>& events);
    std::vector<Event> getEvents();
    
private:
    void processingLoop();
};
```

#### FLTK Timer Integration
```cpp
class UpdateTimer : public Fl_Timeout_Handler {
private:
    MainWindow* m_mainWindow;
    
public:
    UpdateTimer(MainWindow* window);
    void start();
    void stop();
    
    // FLTK timeout callback
    void timeout() override;
};
```

### 8. Configuration and Settings

#### Settings Dialog
```cpp
class SettingsDialog : public Fl_Window {
private:
    Fl_Choice* m_captureMethod;
    Fl_Input* m_blockSize;
    Fl_Input* m_changeThreshold;
    Fl_Choice* m_visualizationStyle;
    Fl_Input* m_fadeDuration;
    
public:
    SettingsDialog();
    
    void loadSettings();
    void saveSettings();
    void applySettings();
};
```

#### Configuration File
```ini
[Capture]
Method=NVFBC
BlockSize=16
ChangeThreshold=0.1
FrameTimeout=16

[Visualization]
Style=Dots
DotSize=2
FadeDuration=0.1
ShowPolarity=true
ShowGrid=false

[Replay]
DefaultSpeed=1.0
AutoPlay=false
LoopPlayback=false
```

### 9. Error Handling and User Feedback

#### Error Display
```cpp
class ErrorDialog : public Fl_Window {
public:
    ErrorDialog(const std::string& title, const std::string& message);
    static void show(const std::string& title, const std::string& message);
};
```

#### Progress Indicators
```cpp
class ProgressDialog : public Fl_Window {
private:
    Fl_Progress* m_progress;
    Fl_Box* m_statusLabel;
    
public:
    ProgressDialog(const std::string& title);
    void setProgress(float progress);
    void setStatus(const std::string& status);
};
```

### 10. Testing and Validation

#### GUI Testing Strategy
1. **Unit Tests**: Test individual GUI components
2. **Integration Tests**: Test GUI with core functionality
3. **User Interface Tests**: Test user interactions and workflows
4. **Performance Tests**: Test GUI responsiveness during recording

#### Test Framework
```cpp
class GUITestFramework {
public:
    static void testMainWindow();
    static void testControlPanel();
    static void testEventDisplay();
    static void testReplayControls();
    static void testFileOperations();
    static void testErrorHandling();
};
```

## Implementation Timeline

### Week 1: Basic FLTK Integration
- Set up FLTK build environment
- Create basic main window
- Implement control panel
- Test basic functionality

### Week 2: Event Display
- Create event visualization widget
- Integrate with DirectX rendering
- Implement real-time updates
- Add zoom and pan controls

### Week 3: Replay System
- Implement replay controls
- Add timeline slider
- Create playback speed control
- Test replay functionality

### Week 4: Advanced Features
- Add statistics panel
- Implement settings dialog
- Create menu system
- Add file operations

### Week 5: Polish and Testing
- Error handling and user feedback
- Performance optimization
- User interface testing
- Documentation and help system

## Benefits of FLTK Integration

1. **User-Friendly Interface**: Replace command-line with intuitive GUI
2. **Real-time Feedback**: Visual progress indicators and status updates
3. **Interactive Visualization**: Direct manipulation of event display
4. **Cross-platform Support**: Future expansion to Linux/macOS
5. **Professional Appearance**: Polished application interface
6. **Easier Testing**: Visual verification of functionality
7. **Better User Experience**: Intuitive controls and feedback

## Conclusion

FLTK integration will transform the neuromorphic screens project from a command-line tool into a professional, user-friendly application. The modular design ensures easy maintenance and future enhancements while providing a solid foundation for the first deliverable and beyond. 