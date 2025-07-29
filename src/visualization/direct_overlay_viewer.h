#pragma once

#include "../streaming_app.h"
#include "../core/timing.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1.h>
#include <dwrite.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

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
    
    bool IsRunning() const { return m_threadRunning.load(); }
    
private:
    // Initialization
    bool CreateOverlayWindow();
    bool InitializeGDI();
    bool CreateBrushes();
    
    // Rendering
    void RenderThreadFunction();
    void UpdateActiveDots();
    void AddDot(const Event& event);
    void RemoveExpiredDots();
    void RenderOverlay();
    
    // Cleanup
    void CleanupGDI();
    void DestroyOverlayWindow();
    
    // Window procedure
    static LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace neuromorphic