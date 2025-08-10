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
 * Base class for ImGui-based neuromorphic event viewers
 * Provides common DirectX 11 setup, event rendering, and ImGui functionality
 * Eliminates code duplication across different viewer types
 */
class ImGuiViewerBase {
public:
    ImGuiViewerBase();
    virtual ~ImGuiViewerBase();
    
    /**
     * Initialize the viewer window and DirectX 11 context
     */
    bool Initialize(const char* title = "Neuromorphic Event Viewer", int width = 1280, int height = 720);
    
    /**
     * Main render loop - calls virtual methods for specialized rendering
     * Returns false when window should close
     */
    bool Render();
    
    /**
     * Cleanup resources
     */
    void Cleanup();

protected:
    // Virtual methods for specialized rendering - override in derived classes
    virtual void RenderMainContent() = 0;  // Pure virtual - main content area
    virtual void RenderControlPanel() {}   // Optional - control panel/sidebar
    virtual void HandleInput() {}          // Optional - keyboard/mouse input handling
    virtual void UpdateLogic() {}          // Optional - per-frame logic updates

    // Common event rendering utilities
    void RenderEventDots(const std::vector<Event>& events, float canvasWidth, float canvasHeight, 
                        uint32_t screenWidth, uint32_t screenHeight, float fadeAlpha = 1.0f);
    
    void RenderEventDotsWithFade(const std::vector<Event>& events, float canvasWidth, float canvasHeight,
                               uint32_t screenWidth, uint32_t screenHeight, uint64_t currentTime, 
                               float fadeDuration = 100.0f); // 100ms default fade
    
    // Coordinate transformation utilities
    ImVec2 ScreenToCanvas(uint16_t screenX, uint16_t screenY, float canvasWidth, float canvasHeight,
                         uint32_t screenWidth, uint32_t screenHeight);
    
    // ImGui utility methods
    void BeginMainWindow(const char* title, bool* p_open = nullptr);
    void EndMainWindow();
    void RenderStatistics(uint32_t totalEvents, float eventsPerSec, uint32_t activeEvents = 0);
    
    // Common color scheme
    ImU32 GetEventColor(int8_t polarity, float alpha = 1.0f);
    
    // Window and DirectX state
    bool m_initialized;
    HWND m_hwnd;
    WNDCLASSEX m_wc;
    ID3D11Device* m_pd3dDevice;
    ID3D11DeviceContext* m_pd3dDeviceContext;
    IDXGISwapChain* m_pSwapChain;
    ID3D11RenderTargetView* m_pMainRenderTargetView;
    
    // Window properties
    int m_windowWidth;
    int m_windowHeight;
    std::string m_windowTitle;

private:
    // DirectX 11 setup/cleanup methods
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    
    // Window procedure
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace neuromorphic