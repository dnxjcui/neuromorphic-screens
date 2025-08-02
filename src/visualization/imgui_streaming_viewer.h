#pragma once

#include "../core/event_types.h"
#include "../core/timing.h"
#include "../core/temporal_index.h"
#include "../streaming_app.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>

// Windows and DirectX includes
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

// ImGui includes
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

namespace neuromorphic {

/**
 * ImGui-based streaming event viewer for real-time visualization
 * Simplified interface without playback controls - designed for live streaming
 */
class ImGuiStreamingViewer {
public:
    ImGuiStreamingViewer(const std::string& title, StreamingApp& streamingApp);
    ~ImGuiStreamingViewer();
    
    /**
     * Initialize viewer and create window
     */
    bool Initialize();
    
    /**
     * Main render loop - returns false when should exit
     */
    void Run();
    
    /**
     * Cleanup resources
     */
    void Cleanup();
    
    /**
     * Export functionality
     */
    void ExportToGIF();
    
    /**
     * Dimming settings
     */
    void SetDimmingEnabled(bool enabled);
    void SetDimmingRate(float rate);
    
private:
    // Window and DirectX resources
    HWND m_hwnd;
    WNDCLASSEXW m_wc;
    ID3D11Device* m_d3dDevice;
    ID3D11DeviceContext* m_d3dDeviceContext;
    IDXGISwapChain* m_swapChain;
    ID3D11RenderTargetView* m_mainRenderTargetView;
    
    // Streaming reference
    StreamingApp& m_streamingApp;
    
    // Event visualization
    std::vector<std::pair<Event, float>> m_activeDots;
    mutable std::mutex m_activeDotsLock;
    
    // High-performance temporal indexing for recent events
    TemporalEventIndex m_temporalIndex;
    
    // Visualization thread
    std::thread m_visualizationThread;
    std::atomic<bool> m_threadRunning;
    
    // Display settings
    uint32_t m_canvasWidth;
    uint32_t m_canvasHeight;
    bool m_showStats;
    bool m_showControls;
    
    // Visualization options
    bool m_useDimming;           // Enable gradual dimming instead of instant removal
    float m_dimmingRate;         // Rate of dimming (0.1 = slow, 2.0 = fast)
    
    /**
     * DirectX initialization
     */
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    
    /**
     * Event visualization
     */
    void AddDot(const Event& event);
    void UpdateActiveDots();
    void RemoveExpiredDots();
    void VisualizationThreadFunction();
    
    /**
     * UI rendering
     */
    void RenderEventCanvas();
    void RenderControlPanel();
    void RenderStatsPanel();
    
    /**
     * Coordinate transformation
     */
    ImVec2 ScreenToCanvas(uint16_t screenX, uint16_t screenY) const;
    
    /**
     * Window procedure
     */
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace neuromorphic