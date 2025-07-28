#pragma once

#include "../core/event_types.h"
#include "../core/timing.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace neuromorphic {

// Forward declaration
class EventCanvas;

/**
 * FLTK-based event viewer for visualizing neuromorphic events
 */
class EventViewer : public Fl_Window {
private:
    // Event data
    EventStream m_events;
    size_t m_currentEventIndex;
    uint64_t m_replayStartTime;
    bool m_isReplaying;
    bool m_isPaused;
    float m_replaySpeed;
    
    // UI components  
    EventCanvas* m_canvas;
    Fl_Button* m_playButton;
    Fl_Button* m_pauseButton;
    Fl_Button* m_stopButton;
    Fl_Slider* m_speedSlider;
    Fl_Slider* m_progressSlider;
    Fl_Slider* m_downsampleSlider;
    Fl_Text_Display* m_statsDisplay;
    Fl_Text_Buffer* m_statsBuffer;
    
    // Rendering
    std::vector<std::pair<Event, float>> m_activeDots; // Event and fade timer
    uint32_t m_canvasWidth, m_canvasHeight;
    int m_downsampleFactor;
    
    // Threading
    std::thread m_replayThread;
    std::atomic<bool> m_threadRunning;
    
    // Statistics
    EventStats m_stats;
    uint32_t m_eventsProcessed;
    float m_currentFPS;

public:
    EventViewer(int x, int y, int w, int h, const char* title = "Neuromorphic Event Viewer");
    ~EventViewer();
    
    /**
     * Load events from file
     */
    bool LoadEvents(const std::string& filename);
    
    /**
     * Start replay
     */
    void StartReplay();
    
    /**
     * Pause replay
     */
    void PauseReplay();
    
    /**
     * Stop replay
     */
    void StopReplay();
    
    /**
     * Set replay speed
     */
    void SetReplaySpeed(float speed);
    
    /**
     * Set downsample factor for visualization
     */
    void SetDownsampleFactor(int factor);
    
    /**
     * Seek to specific time
     */
    void SeekToTime(float timeSeconds);
    
    /**
     * Public methods for EventCanvas
     */
    void UpdateActiveDots();
    void UpdateStatsDisplay();
    void ScreenToCanvas(uint16_t screenX, uint16_t screenY, int& canvasX, int& canvasY);
    void DrawDot(int x, int y, int8_t polarity, float alpha);
    
    // Public access to active dots for canvas
    std::vector<std::pair<Event, float>>& getActiveDots() { return m_activeDots; }

private:
    /**
     * Initialize UI components
     */
    void InitializeUI();
    
    /**
     * Handle canvas drawing
     */
    static void DrawCanvas(Fl_Widget* widget, void* data);
    
    /**
     * Handle button callbacks
     */
    static void OnPlayButton(Fl_Widget* widget, void* data);
    static void OnPauseButton(Fl_Widget* widget, void* data);
    static void OnStopButton(Fl_Widget* widget, void* data);
    static void OnSpeedSlider(Fl_Widget* widget, void* data);
    static void OnProgressSlider(Fl_Widget* widget, void* data);
    static void OnDownsampleSlider(Fl_Widget* widget, void* data);
    
    /**
     * Replay thread function
     */
    void ReplayThread();
    
    /**
     * Add new dot for event
     */
    void AddDot(const Event& event);
    
    /**
     * Remove expired dots
     */
    void RemoveExpiredDots();
    
    /**
     * Handle window close
     */
    int handle(int event) override;
};

/**
 * Canvas widget for drawing events
 */
class EventCanvas : public Fl_Box {
private:
    EventViewer* m_viewer;

public:
    EventCanvas(int x, int y, int w, int h, EventViewer* viewer);
    
    /**
     * Handle drawing
     */
    void draw() override;
    
    // Allow EventViewer to be a friend class
    friend class EventViewer;
};

} // namespace neuromorphic 