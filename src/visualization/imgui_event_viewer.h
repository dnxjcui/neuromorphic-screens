#pragma once

#include "imgui_viewer_base.h"
#include "../core/event_types.h"
#include "../core/timing.h"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace neuromorphic {

/**
 * ImGui-based event viewer for stable, high-performance neuromorphic event visualization
 * Replaces FLTK implementation to eliminate segmentation faults
 */
class ImGuiEventViewer : public ImGuiViewerBase {
public:
    ImGuiEventViewer();
    ~ImGuiEventViewer();
    
    /**
     * Load events from file
     */
    bool LoadEvents(const std::string& filename);
    
    /**
     * Event playback controls
     */
    void StartReplay();
    void PauseReplay();
    void StopReplay();
    void SetReplaySpeed(float speed);
    void SetDownsampleFactor(int factor);
    void SeekToTime(float timeSeconds);
    
    /**
     * Visualization options
     */
    void SetDimmingEnabled(bool enabled);
    void SetDimmingRate(float rate);
    
    /**
     * Export functionality
     */
    void ExportToGIF();

protected:
    // Override base class virtual methods
    void RenderMainContent() override;
    void RenderControlPanel() override;
    void HandleInput() override {}
    void UpdateLogic() override;

private:
    
    // Event data
    EventStream m_events;
    EventStats m_stats;
    size_t m_currentEventIndex;
    uint64_t m_replayStartTime;
    std::atomic<bool> m_isReplaying;
    std::atomic<bool> m_isPaused;
    float m_replaySpeed;
    int m_downsampleFactor;
    uint32_t m_eventsProcessed;
    
    // Visualization
    std::vector<std::pair<Event, float>> m_activeDots; // Event and fade timer
    mutable std::mutex m_activeDotsLock;
    uint32_t m_canvasWidth, m_canvasHeight;
    
    // Threading
    std::thread m_replayThread;
    std::atomic<bool> m_threadRunning;
    
    // UI state
    bool m_showStats;
    bool m_showControls;
    float m_seekPosition;
    
    // Visualization options
    bool m_useDimming;           // Enable gradual dimming instead of instant removal
    float m_dimmingRate;         // Rate of dimming (0.1 = slow, 2.0 = fast)
    bool m_isLooping;            // Enable continuous looping of replay
    
    
    /**
     * Replay thread function
     */
    void ReplayThreadFunction();
    
    /**
     * Event visualization
     */
    void UpdateActiveDots();
    void AddDot(const Event& event);
    void RemoveExpiredDots();
    void RenderEventCanvas();
    void UpdateStatsDisplay();
    
    /**
     * UI rendering
     */
    void RenderStatsPanel();
    
};

} // namespace neuromorphic