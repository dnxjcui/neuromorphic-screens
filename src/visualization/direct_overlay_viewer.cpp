#include "direct_overlay_viewer.h"
#include <iostream>
#include <algorithm>

// Constants for Windows capture exclusion (Windows 10 2004+)
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

#ifndef DWMWA_EXCLUDED_FROM_CAPTURE
#define DWMWA_EXCLUDED_FROM_CAPTURE 25
#endif

namespace neuromorphic {

DirectOverlayViewer::DirectOverlayViewer(StreamingApp& streamingApp) : 
    m_streamingApp(streamingApp),
    m_memoryDC(nullptr), m_bitmap(nullptr), m_bitmapBits(nullptr),
    m_positiveBrush(nullptr), m_negativeBrush(nullptr),
    m_threadRunning(false), m_overlayWindow(nullptr),
    m_screenWidth(0), m_screenHeight(0),
    m_useDimming(false), m_dimmingRate(1.0f),
    m_threshold(15.0f), m_stride(6),  // Default values for overlay: threshold=15.0, stride=6
    m_controlWindow(nullptr), m_thresholdSlider(nullptr), m_strideSlider(nullptr),
    m_thresholdLabel(nullptr), m_strideLabel(nullptr) {
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
    
    // Create control window
    if (!CreateControlWindow()) {
        std::cerr << "Failed to create control window" << std::endl;
        Cleanup();
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
    
    // Create layered window for true transparency with click-through capability
    m_overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
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
    
    // Exclude this window from ALL capture paths (Windows 10 2004+)
    BOOL affinity_ok = SetWindowDisplayAffinity(m_overlayWindow, WDA_EXCLUDEFROMCAPTURE);
    
    // Also use DWM attribute for future-proofing
    BOOL exclude = TRUE;
    HRESULT dwm_ok = DwmSetWindowAttribute(
        m_overlayWindow,
        DWMWA_EXCLUDED_FROM_CAPTURE,
        &exclude,
        sizeof(exclude)
    );
    
    if (!affinity_ok) {
        DWORD err = GetLastError();
        std::cerr << "SetWindowDisplayAffinity failed (" << err << "). Overlay may still recurse." << std::endl;
    }
    
    if (FAILED(dwm_ok)) {
        std::cerr << "DwmSetWindowAttribute failed (0x" << std::hex << dwm_ok << "). Using fallback exclusion." << std::endl;
    }
    
    std::cout << "Overlay window excluded from screen capture APIs" << std::endl;
    
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
        // Update streaming app parameters with current overlay settings
        m_streamingApp.setThreshold(m_threshold);
        m_streamingApp.setStride(m_stride);
        
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
                
                // Only show events from the last 100ms with proper event detection
                for (const auto& event : eventsCopy) {
                    uint64_t eventAbsoluteTime = stream.start_time + event.timestamp;
                    uint64_t eventAge = currentTime - eventAbsoluteTime;
                    
                    // Apply event detection logic similar to implementation guide
                    if (eventAge <= recentThreshold && event.polarity != 0) {
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
            
            // Select color based on polarity - Green for positive, Red for negative (per implementation guide)
            uint32_t color = (event.polarity > 0) ? 
                0xFF00FF00 : // Green with full alpha for positive events (ARGB format)
                0xFFFF0000;  // Red with full alpha for negative events
            
            // Draw event highlight with radius as recommended in implementation guide
            int radius = 2; // 2.0f radius per implementation guide
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
    DestroyControlWindow();
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

void DirectOverlayViewer::DestroyControlWindow() {
    if (m_controlWindow) {
        DestroyWindow(m_controlWindow);
        m_controlWindow = nullptr;
    }
    
    UnregisterClassW(L"DirectOverlayControlWindow", GetModuleHandle(nullptr));
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

bool DirectOverlayViewer::CreateControlWindow() {
    // Register control window class with dark styling
    WNDCLASSW wc = {};
    wc.lpfnWndProc = ControlWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DirectOverlayControlWindow";
    wc.hbrBackground = CreateSolidBrush(RGB(32, 32, 32)); // Dark background similar to ImGui
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    if (!RegisterClassW(&wc)) {
        return false;
    }
    
    // Create control window in top-right corner with improved size
    int windowWidth = 220;
    int windowHeight = 140;
    int x = GetSystemMetrics(SM_CXSCREEN) - windowWidth - 20;
    int y = 20;
    
    m_controlWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"DirectOverlayControlWindow",
        L"Neuromorphic Overlay Controls",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, windowWidth, windowHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), this
    );
    
    if (!m_controlWindow) {
        return false;
    }
    
    // Create labels with better styling
    m_thresholdLabel = CreateWindowW(L"STATIC", L"Threshold: 15.0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 15, 180, 20,
        m_controlWindow, nullptr, GetModuleHandle(nullptr), nullptr);
    
    // Create threshold slider with better positioning
    m_thresholdSlider = CreateWindowW(L"msctls_trackbar32", L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_TOOLTIPS,
        15, 35, 180, 30,
        m_controlWindow, (HMENU)1001, GetModuleHandle(nullptr), nullptr);
    
    m_strideLabel = CreateWindowW(L"STATIC", L"Stride: 6",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 75, 180, 20,
        m_controlWindow, nullptr, GetModuleHandle(nullptr), nullptr);
    
    // Create stride slider with better positioning  
    m_strideSlider = CreateWindowW(L"msctls_trackbar32", L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_TOOLTIPS,
        15, 95, 180, 30,
        m_controlWindow, (HMENU)1002, GetModuleHandle(nullptr), nullptr);
    
    // Set slider ranges
    SendMessage(m_thresholdSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(m_thresholdSlider, TBM_SETPOS, TRUE, (LPARAM)m_threshold);
    
    SendMessage(m_strideSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 12));
    SendMessage(m_strideSlider, TBM_SETPOS, TRUE, (LPARAM)m_stride);
    
    ShowWindow(m_controlWindow, SW_SHOW);
    return true;
}

LRESULT WINAPI DirectOverlayViewer::ControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DirectOverlayViewer* viewer = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        viewer = (DirectOverlayViewer*)cs->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)viewer);
    } else {
        viewer = (DirectOverlayViewer*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }
    
    switch (msg) {
    case WM_CTLCOLORSTATIC:
        {
            // Make static controls (labels) look like ImGui - white text on dark background
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(255, 255, 255)); // White text
            SetBkColor(hdcStatic, RGB(32, 32, 32)); // Dark background
            return (LRESULT)CreateSolidBrush(RGB(32, 32, 32));
        }
    case WM_HSCROLL:
        if (viewer) {
            HWND control = (HWND)lParam;
            int pos = SendMessage(control, TBM_GETPOS, 0, 0);
            
            if (control == viewer->m_thresholdSlider) {
                viewer->SetThreshold((float)pos);
                viewer->UpdateSliderLabels();
            } else if (control == viewer->m_strideSlider) {
                viewer->SetStride((uint32_t)pos);
                viewer->UpdateSliderLabels();
            }
        }
        return 0;
        
    case WM_CLOSE:
        // When control window is closed, stop the entire overlay and application
        if (viewer) {
            viewer->StopOverlay();
            // Signal the main application to exit
            PostQuitMessage(0);
        }
        return 0;
        
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

void DirectOverlayViewer::UpdateSliderLabels() {
    if (m_thresholdLabel) {
        wchar_t buffer[50];
        swprintf_s(buffer, L"Threshold: %.1f", m_threshold);
        SetWindowTextW(m_thresholdLabel, buffer);
    }
    
    if (m_strideLabel) {
        wchar_t buffer[50];
        swprintf_s(buffer, L"Stride: %u", m_stride);
        SetWindowTextW(m_strideLabel, buffer);
    }
}

} // namespace neuromorphic