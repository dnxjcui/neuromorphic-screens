#include "direct_overlay_viewer.h"
#include <iostream>
#include <algorithm>

namespace neuromorphic {

DirectOverlayViewer::DirectOverlayViewer(StreamingApp& streamingApp) : 
    m_streamingApp(streamingApp),
    m_memoryDC(nullptr), m_bitmap(nullptr), m_bitmapBits(nullptr),
    m_positiveBrush(nullptr), m_negativeBrush(nullptr),
    m_threadRunning(false), m_overlayWindow(nullptr),
    m_screenWidth(0), m_screenHeight(0),
    m_useDimming(false), m_dimmingRate(1.0f) {
}

DirectOverlayViewer::~DirectOverlayViewer() {
    Cleanup();
}

bool DirectOverlayViewer::Initialize() {
    // Get screen dimensions
    m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    std::cout << "Initializing direct overlay on " << m_screenWidth << "x" << m_screenHeight << " screen" << std::endl;
    
    // Create overlay window
    if (!CreateOverlayWindow()) {
        std::cerr << "Failed to create overlay window" << std::endl;
        return false;
    }
    
    // Initialize GDI
    if (!InitializeGDI()) {
        std::cerr << "Failed to initialize GDI" << std::endl;
        Cleanup();
        return false;
    }
    
    // Create brushes
    if (!CreateBrushes()) {
        std::cerr << "Failed to create brushes" << std::endl;
        Cleanup();
        return false;
    }
    
    std::cout << "Direct overlay initialized successfully" << std::endl;
    return true;
}

void DirectOverlayViewer::StartOverlay() {
    if (m_threadRunning.load()) {
        std::cout << "Overlay already running" << std::endl;
        return;
    }
    
    m_threadRunning.store(true);
    m_renderThread = std::thread(&DirectOverlayViewer::RenderThreadFunction, this);
    std::cout << "Direct overlay started" << std::endl;
}

void DirectOverlayViewer::StopOverlay() {
    if (!m_threadRunning.load()) return;
    
    m_threadRunning.store(false);
    if (m_renderThread.joinable()) {
        m_renderThread.join();
    }
    
    std::cout << "Direct overlay stopped" << std::endl;
}

bool DirectOverlayViewer::CreateOverlayWindow() {
    // Register window class for overlay
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DirectOverlayWindow";
    
    if (!RegisterClassExW(&wc)) {
        std::cerr << "Failed to register overlay window class" << std::endl;
        return false;
    }
    
    // Create layered window for true transparency (remove WS_EX_TRANSPARENT for visibility)
    m_overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"DirectOverlayWindow",
        L"Neuromorphic Event Overlay",
        WS_POPUP,
        0, 0, m_screenWidth, m_screenHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), this
    );
    
    if (!m_overlayWindow) {
        std::cerr << "Failed to create overlay window" << std::endl;
        return false;
    }
    
    // Show the overlay window - transparency handled by UpdateLayeredWindow
    ShowWindow(m_overlayWindow, SW_SHOWNOACTIVATE);
    UpdateWindow(m_overlayWindow);
    
    return true;
}


bool DirectOverlayViewer::InitializeGDI() {
    // Get desktop DC
    HDC desktopDC = GetDC(nullptr);
    if (!desktopDC) {
        std::cerr << "Failed to get desktop DC" << std::endl;
        return false;
    }
    
    // Create memory DC compatible with desktop
    m_memoryDC = CreateCompatibleDC(desktopDC);
    if (!m_memoryDC) {
        std::cerr << "Failed to create memory DC" << std::endl;
        ReleaseDC(nullptr, desktopDC);
        return false;
    }
    
    // Create DIB for alpha blending
    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = m_screenWidth;
    bih.biHeight = -static_cast<int>(m_screenHeight); // Negative for top-down DIB
    bih.biPlanes = 1;
    bih.biBitCount = 32; // ARGB
    bih.biCompression = BI_RGB;
    
    BITMAPINFO bi = {};
    bi.bmiHeader = bih;
    
    m_bitmap = CreateDIBSection(m_memoryDC, &bi, DIB_RGB_COLORS, &m_bitmapBits, nullptr, 0);
    if (!m_bitmap) {
        std::cerr << "Failed to create DIB section" << std::endl;
        ReleaseDC(nullptr, desktopDC);
        return false;
    }
    
    // Select bitmap into memory DC
    SelectObject(m_memoryDC, m_bitmap);
    
    ReleaseDC(nullptr, desktopDC);
    return true;
}

bool DirectOverlayViewer::CreateBrushes() {
    // Create green brush for positive events
    m_positiveBrush = CreateSolidBrush(RGB(0, 255, 0));
    if (!m_positiveBrush) {
        std::cerr << "Failed to create positive brush" << std::endl;
        return false;
    }
    
    // Create red brush for negative events
    m_negativeBrush = CreateSolidBrush(RGB(255, 0, 0));
    if (!m_negativeBrush) {
        std::cerr << "Failed to create negative brush" << std::endl;
        return false;
    }
    
    return true;
}

void DirectOverlayViewer::RenderThreadFunction() {
    FrameRateLimiter limiter(30.0f); // Lower frame rate for stability
    
    while (m_threadRunning.load()) {
        // Get latest events from streaming app
        const EventStream& stream = m_streamingApp.getEventStream();
        
        // Show only very recent events based on actual time, not just buffer position
        {
            std::lock_guard<std::mutex> lock(m_activeDotsLock);
            m_activeDots.clear();
            
            if (stream.size() > 0) {
                uint64_t currentTime = HighResTimer::GetMicroseconds();
                uint64_t recentThreshold = 100000; // 100ms window for events
                
                // Get thread-safe copy of events
                auto eventsCopy = stream.getEventsCopy();
                
                // Only show events from the last 100ms
                for (const auto& event : eventsCopy) {
                    uint64_t eventAbsoluteTime = stream.start_time + event.timestamp;
                    uint64_t eventAge = currentTime - eventAbsoluteTime;
                    
                    if (eventAge <= recentThreshold) {
                        m_activeDots.push_back({event, 1.0f});
                    }
                }
                
                // Debug output
                static int frameCount = 0;
                if (frameCount++ % 30 == 0 && !m_activeDots.empty()) {
                    std::cout << "\nOverlay: " << m_activeDots.size() << " active dots";
                }
            }
        }
        
        // Render overlay
        RenderOverlay();
        
        limiter.WaitForNextFrame();
    }
}

void DirectOverlayViewer::AddDot(const Event& event) {
    std::lock_guard<std::mutex> lock(m_activeDotsLock);
    m_activeDots.push_back({event, 1.0f}); // No fade, instant flash
}

void DirectOverlayViewer::UpdateActiveDots() {
    static uint64_t lastUpdateTime = 0;
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    
    if (lastUpdateTime == 0) {
        lastUpdateTime = currentTime;
        return;
    }
    
    float deltaTime = static_cast<float>(currentTime - lastUpdateTime) / 1000000.0f;
    lastUpdateTime = currentTime;
    
    // Thread-safe update of fade timers (no dimming - keep dots visible longer)
    {
        std::lock_guard<std::mutex> lock(m_activeDotsLock);
        for (auto& dot : m_activeDots) {
            // Reduce fade time much slower for better visibility
            dot.fadeTime -= deltaTime * 0.2f; // Even slower fade rate for visibility
        }
    }
    
    // Remove expired dots
    RemoveExpiredDots();
}

void DirectOverlayViewer::RemoveExpiredDots() {
    std::lock_guard<std::mutex> lock(m_activeDotsLock);
    m_activeDots.erase(
        std::remove_if(m_activeDots.begin(), m_activeDots.end(),
                      [](const OverlayDot& dot) {
                          return dot.fadeTime <= 0.0f;
                      }),
        m_activeDots.end()
    );
}

void DirectOverlayViewer::RenderOverlay() {
    if (!m_memoryDC || !m_bitmapBits) return;
    
    // Clear bitmap with transparent pixels (alpha = 0)
    uint32_t* pixels = static_cast<uint32_t*>(m_bitmapBits);
    size_t pixelCount = m_screenWidth * m_screenHeight;
    memset(pixels, 0, pixelCount * sizeof(uint32_t)); // All transparent
    
    // Draw active dots directly to bitmap
    {
        std::lock_guard<std::mutex> lock(m_activeDotsLock);
        for (const auto& dot : m_activeDots) {
            const Event& event = dot.event;
            
            // Select color based on polarity (1=green increase, 0=red decrease)
            uint32_t color = (event.polarity == 1) ? 
                0xFF00FF00 : // Green with full alpha (ARGB format)
                0xFFFF0000;  // Red with full alpha
            
            // Draw a larger, more visible dot
            int radius = 3; // Larger dot for visibility
            int centerX = event.x;
            int centerY = event.y;
            
            // Bounds check before drawing
            if (centerX >= 0 && centerX < static_cast<int>(m_screenWidth) && 
                centerY >= 0 && centerY < static_cast<int>(m_screenHeight)) {
                
                for (int dy = -radius; dy <= radius; dy++) {
                    for (int dx = -radius; dx <= radius; dx++) {
                        if (dx*dx + dy*dy <= radius*radius) {
                            int x = centerX + dx;
                            int y = centerY + dy;
                            
                            if (x >= 0 && x < static_cast<int>(m_screenWidth) && 
                                y >= 0 && y < static_cast<int>(m_screenHeight)) {
                                pixels[y * m_screenWidth + x] = color;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Update the layered window with alpha blending
    POINT ptSrc = {0, 0};
    POINT ptDst = {0, 0};
    SIZE sizeWnd = {static_cast<LONG>(m_screenWidth), static_cast<LONG>(m_screenHeight)};
    BLENDFUNCTION blendFunc = {};
    blendFunc.BlendOp = AC_SRC_OVER;
    blendFunc.SourceConstantAlpha = 255;
    blendFunc.AlphaFormat = AC_SRC_ALPHA;
    
    UpdateLayeredWindow(m_overlayWindow, nullptr, &ptDst, &sizeWnd,
                       m_memoryDC, &ptSrc, 0, &blendFunc, ULW_ALPHA);
}

void DirectOverlayViewer::Cleanup() {
    StopOverlay();
    
    CleanupGDI();
    DestroyOverlayWindow();
}


void DirectOverlayViewer::CleanupGDI() {
    if (m_positiveBrush) {
        DeleteObject(m_positiveBrush);
        m_positiveBrush = nullptr;
    }
    
    if (m_negativeBrush) {
        DeleteObject(m_negativeBrush);
        m_negativeBrush = nullptr;
    }
    
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
        m_bitmapBits = nullptr; // Don't delete - freed with bitmap
    }
    
    if (m_memoryDC) {
        DeleteDC(m_memoryDC);
        m_memoryDC = nullptr;
    }
}

void DirectOverlayViewer::DestroyOverlayWindow() {
    if (m_overlayWindow) {
        DestroyWindow(m_overlayWindow);
        m_overlayWindow = nullptr;
    }
    
    UnregisterClassW(L"DirectOverlayWindow", GetModuleHandle(nullptr));
}

LRESULT WINAPI DirectOverlayViewer::OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        }
        return 0;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

} // namespace neuromorphic