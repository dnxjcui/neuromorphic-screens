#pragma once

#include "../core/event_types.h"
#include "../core/timing.h"
#include <windows.h>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace neuromorphic {

/**
 * Simple Windows GDI-based event viewer with pixel-by-pixel rendering
 */
class SimpleViewer {
private:
    // Window handle
    HWND m_hwnd;
    HDC m_hdc;
    HINSTANCE m_hInstance;
    
    // Event data
    EventStream m_events;
    size_t m_currentEventIndex;
    uint64_t m_replayStartTime;
    bool m_isReplaying;
    bool m_isPaused;
    float m_replaySpeed;
    
    // Rendering - pixel-by-pixel
    std::vector<std::pair<Event, float>> m_activePixels; // Event and fade timer
    uint32_t m_canvasWidth, m_canvasHeight;
    uint32_t m_scaleFactor; // Scale factor for display (1 = 1:1, 2 = 2x zoom, etc.)
    
    // Threading
    std::thread m_replayThread;
    std::atomic<bool> m_threadRunning;
    
    // Statistics
    EventStats m_stats;
    uint32_t m_eventsProcessed;
    float m_currentFPS;

public:
    SimpleViewer();
    ~SimpleViewer();
    
    /**
     * Initialize the viewer
     */
    bool Initialize(HINSTANCE hInstance);
    
    /**
     * Load events from file
     */
    bool LoadEvents(const std::string& filename);
    
    /**
     * Show the viewer window
     */
    void Show();
    
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
     * Set display scale factor
     */
    void SetScaleFactor(uint32_t scale);

private:
    /**
     * Window procedure
     */
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    /**
     * Handle window messages
     */
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    /**
     * Handle paint
     */
    void OnPaint();
    
    /**
     * Handle key press
     */
    void OnKeyPress(WPARAM key);
    
    /**
     * Replay thread function
     */
    void ReplayThread();
    
    /**
     * Update active pixels
     */
    void UpdateActivePixels();
    
    /**
     * Add pixel event
     */
    void AddPixel(const Event& event);
    
    /**
     * Remove expired pixels
     */
    void RemoveExpiredPixels();
    
    /**
     * Draw pixel (1x1 pixel)
     */
    void DrawPixel(int x, int y, int8_t polarity, float alpha);
    
    /**
     * Draw statistics panel
     */
    void DrawStatistics();
    
    /**
     * Convert screen coordinates to canvas coordinates
     */
    void ScreenToCanvas(uint16_t screenX, uint16_t screenY, int& canvasX, int& canvasY);
};

} // namespace neuromorphic 