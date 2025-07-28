#include "event_viewer.h"
#include "../core/event_file.h"
#include "../core/event_file_formats.h"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace neuromorphic {

EventViewer::EventViewer(int x, int y, int w, int h, const char* title)
    : Fl_Window(x, y, w, h, title),
      m_currentEventIndex(0), m_replayStartTime(0),
      m_isReplaying(false), m_isPaused(false), m_replaySpeed(0.5f),
      m_canvas(nullptr), m_playButton(nullptr), m_pauseButton(nullptr),
      m_stopButton(nullptr), m_speedSlider(nullptr), m_progressSlider(nullptr),
      m_downsampleSlider(nullptr), m_statsDisplay(nullptr), m_statsBuffer(nullptr), 
      m_canvasWidth(800), m_canvasHeight(600), m_downsampleFactor(1),
      m_threadRunning(false), m_eventsProcessed(0), m_currentFPS(0.0f) {
    
    InitializeUI();
}

EventViewer::~EventViewer() {
    StopReplay();
    if (m_replayThread.joinable()) {
        m_threadRunning = false;
        m_replayThread.join();
    }
    
    // Clean up text buffer
    delete m_statsBuffer;
}

bool EventViewer::LoadEvents(const std::string& filename) {
    if (!EventFileFormats::ReadEvents(m_events, filename)) {
        std::cerr << "Failed to load events from: " << filename << std::endl;
        return false;
    }
    
    // Sort events by timestamp
    EventFile::SortEventsByTime(m_events);
    
    // Calculate statistics
    m_stats.calculate(m_events);
    
    // Reset replay state
    m_currentEventIndex = 0;
    m_isReplaying = false;
    m_isPaused = false;
    m_eventsProcessed = 0;
    
    // Update UI
    UpdateStatsDisplay();
    
    std::cout << "Loaded " << m_events.events.size() << " events" << std::endl;
    
    // Auto-start playback for debugging
    std::cout << "Auto-starting playback..." << std::endl;
    StartReplay();
    
    // Add timer for continuous UI updates
    Fl::add_timeout(1.0/60.0, TimerCallback, this);
    
    return true;
}

void EventViewer::StartReplay() {
    std::cout << "StartReplay() called with " << m_events.events.size() << " events" << std::endl;
    
    if (m_events.events.empty()) {
        std::cerr << "No events to replay" << std::endl;
        return;
    }
    
    if (m_isReplaying && !m_isPaused) {
        std::cout << "Already playing, ignoring" << std::endl;
        return; // Already playing
    }
    
    if (m_isPaused) {
        std::cout << "Resuming from pause" << std::endl;
        m_isPaused = false;
        return; // Resume from pause
    }
    
    // Start new replay
    std::cout << "Starting new replay..." << std::endl;
    m_isReplaying = true;
    m_isPaused = false;
    m_currentEventIndex = 0;
    m_eventsProcessed = 0;
    m_replayStartTime = HighResTimer::GetMicroseconds();
    m_threadRunning = true;
    
    // Start replay thread
    if (m_replayThread.joinable()) {
        m_replayThread.join();
    }
    m_replayThread = std::thread(&EventViewer::ReplayThread, this);
    
    std::cout << "Started replay thread" << std::endl;
}

void EventViewer::PauseReplay() {
    m_isPaused = true;
    std::cout << "Paused replay" << std::endl;
}

void EventViewer::StopReplay() {
    m_isReplaying = false;
    m_isPaused = false;
    m_threadRunning = false;
    
    if (m_replayThread.joinable()) {
        m_replayThread.join();
    }
    
    // Clear active dots
    m_activeDots.clear();
    
    std::cout << "Stopped replay" << std::endl;
}

void EventViewer::SetReplaySpeed(float speed) {
    m_replaySpeed = speed;
}

void EventViewer::SetDownsampleFactor(int factor) {
    m_downsampleFactor = factor;
    std::cout << "Visualization downsampling set to " << factor << "x" << std::endl;
}

void EventViewer::SeekToTime(float timeSeconds) {
    if (m_events.events.empty()) return;
    
    uint64_t targetTime = m_events.start_time + static_cast<uint64_t>(timeSeconds * 1000000.0f);
    
    // Find closest event
    for (size_t i = 0; i < m_events.events.size(); i++) {
        if (m_events.events[i].timestamp >= targetTime) {
            m_currentEventIndex = i;
            break;
        }
    }
    
    // Update progress slider
    if (m_progressSlider) {
        float progress = static_cast<float>(m_currentEventIndex) / static_cast<float>(m_events.events.size());
        m_progressSlider->value(progress);
    }
}

void EventViewer::InitializeUI() {
    // Use smaller, more reasonable canvas size
    m_canvasWidth = 600;
    m_canvasHeight = 400;
    
    // Calculate control panel position
    int controlX = m_canvasWidth + 20;
    int controlWidth = 200;
    
    // Create canvas
    m_canvas = new EventCanvas(10, 10, m_canvasWidth, m_canvasHeight, this);
    
    // Create control buttons - arrange vertically for better fit
    m_playButton = new Fl_Button(controlX, 20, 60, 30, "Play");
    m_playButton->callback(OnPlayButton, this);
    
    m_pauseButton = new Fl_Button(controlX + 70, 20, 60, 30, "Pause");
    m_pauseButton->callback(OnPauseButton, this);
    
    m_stopButton = new Fl_Button(controlX + 140, 20, 50, 30, "Stop");
    m_stopButton->callback(OnStopButton, this);
    
    // Create sliders
    m_speedSlider = new Fl_Slider(controlX, 70, controlWidth, 20, "Speed:");
    m_speedSlider->type(FL_HORIZONTAL);
    m_speedSlider->range(0.01, 5.0);  // Allow much slower speeds
    m_speedSlider->value(0.5);        // Start slower for dense events
    m_speedSlider->callback(OnSpeedSlider, this);
    
    m_progressSlider = new Fl_Slider(controlX, 110, controlWidth, 20, "Progress:");
    m_progressSlider->type(FL_HORIZONTAL);
    m_progressSlider->range(0.0, 1.0);
    m_progressSlider->value(0.0);
    m_progressSlider->callback(OnProgressSlider, this);
    
    m_downsampleSlider = new Fl_Slider(controlX, 150, controlWidth, 20, "Downsample:");
    m_downsampleSlider->type(FL_HORIZONTAL);
    m_downsampleSlider->range(1.0, 8.0);  // 1x to 8x downsampling
    m_downsampleSlider->value(1.0);       // Start at no downsampling
    m_downsampleSlider->step(1.0);        // Integer steps only
    m_downsampleSlider->callback(OnDownsampleSlider, this);
    
    // Create statistics display
    m_statsDisplay = new Fl_Text_Display(controlX, 190, controlWidth, 160, "Statistics:");
    m_statsDisplay->textfont(FL_COURIER);
    m_statsDisplay->textsize(10);
    
    // Create and assign text buffer
    m_statsBuffer = new Fl_Text_Buffer();
    m_statsDisplay->buffer(m_statsBuffer);
    
    // Set window size to fit everything with margin
    int totalWidth = m_canvasWidth + controlWidth + 40;
    int totalHeight = (m_canvasHeight > 390) ? m_canvasHeight + 40 : 430;
    size(totalWidth, totalHeight);
    
    // Call end() to finish widget setup
    end();
}

void EventViewer::ReplayThread() {
    FrameRateLimiter limiter(60.0f);
    
    while (m_threadRunning && m_isReplaying && !m_isPaused) {
        uint64_t currentTime = HighResTimer::GetMicroseconds();
        uint64_t elapsedTime = currentTime - m_replayStartTime;
        
        // Process events that should be displayed now
        while (m_currentEventIndex < m_events.events.size() && m_threadRunning) {
            const Event& event = m_events.events[m_currentEventIndex];
            // event.timestamp is already relative to start time (from CSV reading)
            uint64_t eventTime = event.timestamp;
            uint64_t adjustedEventTime = static_cast<uint64_t>(eventTime / m_replaySpeed);
            
            if (adjustedEventTime <= elapsedTime) {
                // Apply downsampling during visualization
                if (m_downsampleFactor == 1 || 
                    (event.x % m_downsampleFactor == 0 && event.y % m_downsampleFactor == 0)) {
                    AddDot(event);
                }
                m_currentEventIndex++;
                m_eventsProcessed++;
            } else {
                break;
            }
        }
        
        // Check if replay is complete
        if (m_currentEventIndex >= m_events.events.size()) {
            m_isReplaying = false;
            break;
        }
        
        // Update UI from thread - force redraw
        if (m_canvas) {
            m_canvas->redraw();
        }
        
        // Update progress slider
        if (m_progressSlider && !m_events.events.empty()) {
            float progress = static_cast<float>(m_currentEventIndex) / static_cast<float>(m_events.events.size());
            m_progressSlider->value(progress);
            m_progressSlider->redraw();
        }
        
        // Wake up main thread for UI update
        Fl::awake();
        
        // Wait for next frame
        limiter.WaitForNextFrame();
    }
}

void EventViewer::UpdateActiveDots() {
    static uint64_t lastUpdateTime = 0;
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    
    if (lastUpdateTime == 0) {
        lastUpdateTime = currentTime;
        return;
    }
    
    float deltaTime = static_cast<float>(currentTime - lastUpdateTime) / 1000000.0f;
    lastUpdateTime = currentTime;
    
    // Update fade timers
    for (auto& dot : m_activeDots) {
        dot.second -= deltaTime;
    }
    
    // Remove expired dots
    RemoveExpiredDots();
}

void EventViewer::AddDot(const Event& event) {
    m_activeDots.push_back(std::make_pair(event, constants::DOT_FADE_DURATION));
    
    // Debug output for first few events
    static int dotCount = 0;
    if (dotCount < 10) {
        std::cout << "Adding dot " << dotCount << " at (" << event.x << "," << event.y << ") polarity=" << (int)event.polarity << std::endl;
        dotCount++;
    }
}

void EventViewer::RemoveExpiredDots() {
    m_activeDots.erase(
        std::remove_if(m_activeDots.begin(), m_activeDots.end(),
                      [](const std::pair<Event, float>& dot) {
                          return dot.second <= 0.0f;
                      }),
        m_activeDots.end()
    );
}

void EventViewer::UpdateStatsDisplay() {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Total Events: " << m_stats.total_events << "\n";
    oss << "Positive: " << m_stats.positive_events << "\n";
    oss << "Negative: " << m_stats.negative_events << "\n";
    oss << "Duration: " << (m_stats.duration_us / 1000000.0f) << "s\n";
    oss << "Events/sec: " << m_stats.events_per_second << "\n";
    oss << "Processed: " << m_eventsProcessed << "\n";
    oss << "Current FPS: " << m_currentFPS << "\n";
    oss << "Replay Speed: " << m_replaySpeed << "x\n";
    oss << "Downsample: " << m_downsampleFactor << "x\n";
    oss << "Active Dots: " << m_activeDots.size() << "\n";
    
    m_statsDisplay->buffer()->text(oss.str().c_str());
}

void EventViewer::ScreenToCanvas(uint16_t screenX, uint16_t screenY, int& canvasX, int& canvasY) {
    if (m_events.width > 0 && m_events.height > 0) {
        float scaleX = static_cast<float>(m_canvasWidth) / static_cast<float>(m_events.width);
        float scaleY = static_cast<float>(m_canvasHeight) / static_cast<float>(m_events.height);
        
        canvasX = static_cast<int>(screenX * scaleX);
        canvasY = static_cast<int>(screenY * scaleY);
        
        // Ensure coordinates are within canvas bounds
        canvasX = (canvasX < 0) ? 0 : ((canvasX >= static_cast<int>(m_canvasWidth)) ? static_cast<int>(m_canvasWidth) - 1 : canvasX);
        canvasY = (canvasY < 0) ? 0 : ((canvasY >= static_cast<int>(m_canvasHeight)) ? static_cast<int>(m_canvasHeight) - 1 : canvasY);
    } else {
        canvasX = screenX;
        canvasY = screenY;
    }
}

void EventViewer::DrawDot(int x, int y, int8_t polarity, float alpha) {
    // Set color based on polarity
    if (polarity > 0) {
        fl_color(static_cast<uchar>(0), static_cast<uchar>(255 * alpha), static_cast<uchar>(0)); // Green
    } else {
        fl_color(static_cast<uchar>(255 * alpha), static_cast<uchar>(0), static_cast<uchar>(0)); // Red
    }
    
    // Draw dot
    fl_rectf(x - constants::DOT_SIZE/2, y - constants::DOT_SIZE/2, 
             constants::DOT_SIZE, constants::DOT_SIZE);
}

int EventViewer::handle(int event) {
    if (event == FL_CLOSE) {
        StopReplay();
        hide();
        return 1;
    }
    return Fl_Window::handle(event);
}

// Static callback functions
void EventViewer::OnPlayButton(Fl_Widget* widget, void* data) {
    std::cout << "Play button pressed!" << std::endl;
    EventViewer* viewer = static_cast<EventViewer*>(data);
    viewer->StartReplay();
}

void EventViewer::OnPauseButton(Fl_Widget* widget, void* data) {
    EventViewer* viewer = static_cast<EventViewer*>(data);
    viewer->PauseReplay();
}

void EventViewer::OnStopButton(Fl_Widget* widget, void* data) {
    EventViewer* viewer = static_cast<EventViewer*>(data);
    viewer->StopReplay();
}

void EventViewer::OnSpeedSlider(Fl_Widget* widget, void* data) {
    EventViewer* viewer = static_cast<EventViewer*>(data);
    Fl_Slider* slider = static_cast<Fl_Slider*>(widget);
    viewer->SetReplaySpeed(static_cast<float>(slider->value()));
}

void EventViewer::OnProgressSlider(Fl_Widget* widget, void* data) {
    EventViewer* viewer = static_cast<EventViewer*>(data);
    Fl_Slider* slider = static_cast<Fl_Slider*>(widget);
    float progress = static_cast<float>(slider->value());
    
    if (viewer->m_events.events.size() > 0) {
        float totalDuration = static_cast<float>(viewer->m_stats.duration_us) / 1000000.0f;
        float timeSeconds = progress * totalDuration;
        viewer->SeekToTime(timeSeconds);
    }
}

void EventViewer::OnDownsampleSlider(Fl_Widget* widget, void* data) {
    EventViewer* viewer = static_cast<EventViewer*>(data);
    Fl_Slider* slider = static_cast<Fl_Slider*>(widget);
    int factor = static_cast<int>(slider->value());
    viewer->SetDownsampleFactor(factor);
}

void EventViewer::TimerCallback(void* data) {
    EventViewer* viewer = static_cast<EventViewer*>(data);
    
    // Force canvas redraw if playing
    if (viewer->m_isReplaying && !viewer->m_isPaused) {
        if (viewer->m_canvas) {
            viewer->m_canvas->redraw();
        }
    }
    
    // Schedule next timer callback
    Fl::repeat_timeout(1.0/60.0, TimerCallback, data);
}

// EventCanvas implementation
EventCanvas::EventCanvas(int x, int y, int w, int h, EventViewer* viewer)
    : Fl_Box(x, y, w, h), m_viewer(viewer) {
    box(FL_DOWN_BOX);
    color(FL_BLACK);
}

void EventCanvas::draw() {
    Fl_Box::draw();
    
    if (!m_viewer) return;
    
    // Update active dots
    m_viewer->UpdateActiveDots();
    
    // Draw active dots
    for (const auto& dot : m_viewer->getActiveDots()) {
        const Event& event = dot.first;
        float alpha = dot.second / constants::DOT_FADE_DURATION;
        
        int canvasX, canvasY;
        m_viewer->ScreenToCanvas(event.x, event.y, canvasX, canvasY);
        m_viewer->DrawDot(canvasX, canvasY, event.polarity, alpha);
    }
    
    // Update statistics - do this less frequently to avoid UI lag
    static int updateCounter = 0;
    if (++updateCounter % 10 == 0) {  // Update every 10 frames
        m_viewer->UpdateStatsDisplay();
    }
}

} // namespace neuromorphic 