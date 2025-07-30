#pragma once

#include "../streaming_app.h"
#include "../core/timing.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1.h>
#include <dwrite.h>
#include <commctrl.h>
#include <windows.h>
#include <dwmapi.h>

// ImGui includes
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

namespace neuromorphic {

/**
 * Direct overlay viewer for real-time event visualization
 * Renders events directly on the screen without a window
 */
class DirectOverlayViewer {
private:
    StreamingApp& m_streamingApp;
    
    
    // GDI resources for layered window drawing
    HDC m_memoryDC;
    HBITMAP m_bitmap;
    void* m_bitmapBits;
    
    // GDI brushes for drawing
    HBRUSH m_positiveBrush;  // Green for positive events
    HBRUSH m_negativeBrush;  // Red for negative events
    
    // Threading
    std::atomic<bool> m_threadRunning;
    std::thread m_renderThread;
    
    // Overlay state
    HWND m_overlayWindow;
    uint32_t m_screenWidth;
    uint32_t m_screenHeight;
    
    // Event visualization
    struct OverlayDot {
        Event event;
        float fadeTime;
    };
    std::vector<OverlayDot> m_activeDots;
    mutable std::mutex m_activeDotsLock;
    
    // Settings
    bool m_useDimming;
    float m_dimmingRate;
    
    // Event capture parameters
    float m_threshold;  // Event threshold (0-100)
    uint32_t m_stride;  // Pixel stride (1-12)
    size_t m_maxEvents; // Maximum events in context window
    
    // ImGui Control GUI
    HWND m_controlWindow;
    ID3D11Device* m_d3dDevice;
    ID3D11DeviceContext* m_d3dDeviceContext;
    IDXGISwapChain* m_swapChain;
    ID3D11RenderTargetView* m_mainRenderTargetView;
    bool m_showControls;
    
public:
    DirectOverlayViewer(StreamingApp& streamingApp);
    ~DirectOverlayViewer();
    
    bool Initialize();
    void StartOverlay();
    void StopOverlay();
    void Cleanup();
    
    
    // Settings
    void SetDimmingEnabled(bool enabled) { m_useDimming = enabled; }
    void SetDimmingRate(float rate) { m_dimmingRate = rate; }
    
    // Event capture parameters
    void SetThreshold(float threshold) { m_threshold = (std::max)(0.0f, (std::min)(100.0f, threshold)); }
    void SetStride(uint32_t stride) { m_stride = (std::max)(1u, (std::min)(30u, stride)); }
    void SetMaxEvents(size_t maxEvents) { m_maxEvents = (std::max)(static_cast<size_t>(1000), (std::min)(static_cast<size_t>(100000), maxEvents)); }
    float GetThreshold() const { return m_threshold; }
    uint32_t GetStride() const { return m_stride; }
    size_t GetMaxEvents() const { return m_maxEvents; }
    
    bool IsRunning() const { return m_threadRunning.load(); }
    
private:
    // Initialization
    bool CreateOverlayWindow();
    bool CreateControlWindow();
    bool InitializeGDI();
    bool CreateBrushes();
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    
    // Rendering
    void RenderThreadFunction();
    void UpdateActiveDots();
    void AddDot(const Event& event);
    void RemoveExpiredDots();
    void RenderOverlay();
    
    // Cleanup
    void CleanupGDI();
    void DestroyOverlayWindow();
    void DestroyControlWindow();
    
    // Window procedures
    static LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT WINAPI ControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // ImGui rendering
    void RenderControlPanel();
};

} // namespace neuromorphic