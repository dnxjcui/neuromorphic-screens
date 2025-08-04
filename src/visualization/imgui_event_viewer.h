#pragma once

#include "../core/event_types.h"
#include "../core/timing.h"
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <d3d11.h>
#endif

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

namespace neuromorphic {

/**
 * ImGui-based event viewer for stable, high-performance neuromorphic event visualization
 * Replaces FLTK implementation to eliminate segmentation faults
 */
class ImGuiEventViewer {
public:
    ImGuiEventViewer();
    ~ImGuiEventViewer();
    
    /**
     * Initialize the viewer window and DirectX 11 context
     */
    bool Initialize(const char* title = "Neuromorphic Event Viewer", int width = 1280, int height = 720);
    
    /**
     * Load events from file
     */
    bool LoadEvents(const std::string& filename);
    
    /**
     * Main render loop - returns false when should exit
     */
    bool Render();
    
    /**
     * Cleanup resources
     */
    void Cleanup();
    
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

private:
    // Window and DirectX resources
    HWND m_hwnd;
    WNDCLASSEXW m_wc;
    ID3D11Device* m_d3dDevice;
    ID3D11DeviceContext* m_d3dDeviceContext;
    IDXGISwapChain* m_swapChain;
    ID3D11RenderTargetView* m_mainRenderTargetView;
    
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
     * Initialize DirectX 11
     */
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    
    /**
     * Window procedure
     */
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
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
    void RenderControlPanel();
    void RenderStatsPanel();
    
    /**
     * Coordinate transformation
     */
    ImVec2 ScreenToCanvas(uint16_t screenX, uint16_t screenY) const;
};

} // namespace neuromorphic