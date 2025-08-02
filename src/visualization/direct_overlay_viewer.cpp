#include "direct_overlay_viewer.h"
#include <iostream>
#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    m_threshold(15.0f), m_stride(6), m_maxEvents(constants::MAX_EVENT_CONTEXT_WINDOW),  // Default values for overlay: threshold=15.0, stride=6, maxEvents=1000000
    m_controlWindow(nullptr), m_d3dDevice(nullptr), m_d3dDeviceContext(nullptr), 
    m_swapChain(nullptr), m_mainRenderTargetView(nullptr), m_showControls(true),
    m_temporalIndex(100000, 10000) {  // 100ms time window, max 10000 recent events
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
    FrameRateLimiter limiter(60.0f); // Lower frame rate for stability
    
    while (m_threadRunning.load()) {
        // Update streaming app parameters with current overlay settings
        m_streamingApp.setThreshold(m_threshold);
        m_streamingApp.setStride(m_stride);
        m_streamingApp.setMaxEvents(m_maxEvents);
        
        // Get latest events from streaming app
        const EventStream& stream = m_streamingApp.getEventStream();
        
        // High-performance recent event processing using temporal index
        {
            uint64_t currentTime = HighResTimer::GetMicroseconds();
            
            // Update temporal index with latest events (O(k) where k = new events)
            if (stream.size() > 0) {
                m_temporalIndex.updateFromStream(stream, currentTime);
            }
            
            // Get recent events efficiently (O(k) where k = recent events, not total events)
            auto recentEvents = m_temporalIndex.getRecentEvents(currentTime);
            
            // Update active dots with recent events only
            {
                std::lock_guard<std::mutex> lock(m_activeDotsLock);
                m_activeDots.clear();
                m_activeDots.reserve(recentEvents.size());
                
                for (const auto& event : recentEvents) {
                    m_activeDots.push_back({event, 1.0f});
                }
            }
            
            // Debug output with performance stats
            static int frameCount = 0;
            if (frameCount++ % 30 == 0 && !recentEvents.empty()) {
                size_t totalProcessed, duplicatesSkipped, bufferSize;
                m_temporalIndex.getPerformanceStats(totalProcessed, duplicatesSkipped, bufferSize);
                std::cout << "\nOverlay: " << recentEvents.size() << " active dots, "
                         << "buffer: " << bufferSize << ", "
                         << "processed: " << totalProcessed << ", "
                         << "duplicates: " << duplicatesSkipped;
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
    
    uint32_t* pixels = static_cast<uint32_t*>(m_bitmapBits);
    DirtyRegion newDirtyRegion;
    
    // Calculate dirty regions and draw active dots
    {
        std::lock_guard<std::mutex> dotsLock(m_activeDotsLock);
        std::lock_guard<std::mutex> dirtyLock(m_dirtyRegionLock);
        
        // Clear previous frame's dirty region first
        if (m_previousDirtyRegion.isDirty) {
            m_previousDirtyRegion.clampTo(m_screenWidth, m_screenHeight);
            
            for (int y = m_previousDirtyRegion.minY; y <= m_previousDirtyRegion.maxY; y++) {
                for (int x = m_previousDirtyRegion.minX; x <= m_previousDirtyRegion.maxX; x++) {
                    pixels[y * m_screenWidth + x] = 0; // Transparent
                }
            }
        }
        
        // Draw active dots and calculate new dirty region
        for (const auto& dot : m_activeDots) {
            const Event& event = dot.event;
            
            // Select color based on polarity
            uint32_t color = (event.polarity > 0) ? 
                0xFF00FF00 : // Green with full alpha for positive events
                0xFFFF0000;  // Red with full alpha for negative events
            
            int radius = 2;
            int centerX = event.x;
            int centerY = event.y;
            
            // Track dirty region for this dot
            newDirtyRegion.addPoint(centerX, centerY, radius);
            
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
        
        // Update dirty regions for next frame
        m_previousDirtyRegion = m_currentDirtyRegion;
        m_currentDirtyRegion = newDirtyRegion;
    }
    
    // Update the layered window with alpha blending
    // Note: Still updating full window for simplicity with layered windows
    // For even better performance, could use smaller update regions
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
        // Cleanup ImGui
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        // Cleanup DirectX
        CleanupDeviceD3D();
        
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
    // Register control window class 
    WNDCLASSW wc = {};
    wc.lpfnWndProc = ControlWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DirectOverlayControlWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    if (!RegisterClassW(&wc)) {
        return false;
    }
    
    // Create control window - resizable like streaming GUI
    int windowWidth = 300;
    int windowHeight = 320;
    int x = GetSystemMetrics(SM_CXSCREEN) - windowWidth - 20;
    int y = 20;
    
    m_controlWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"DirectOverlayControlWindow",
        L"Overlay Controls",
        WS_OVERLAPPEDWINDOW,  // Make it resizable like streaming
        x, y, windowWidth, windowHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), this
    );
    
    if (!m_controlWindow) {
        return false;
    }
    
    // Initialize DirectX for ImGui
    if (!CreateDeviceD3D(m_controlWindow)) {
        std::cerr << "Failed to create DirectX device for overlay controls" << std::endl;
        return false;
    }
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup ImGui style - same as streaming
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_controlWindow);
    ImGui_ImplDX11_Init(m_d3dDevice, m_d3dDeviceContext);
    
    ShowWindow(m_controlWindow, SW_SHOW);
    UpdateWindow(m_controlWindow);
    
    // Set up a timer to continuously redraw the ImGui window
    SetTimer(m_controlWindow, 1, 16, nullptr); // ~60 FPS
    
    return true;
}

LRESULT WINAPI DirectOverlayViewer::ControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    
    DirectOverlayViewer* viewer = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        viewer = (DirectOverlayViewer*)cs->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)viewer);
    } else {
        viewer = (DirectOverlayViewer*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }
    
    switch (msg) {
    case WM_TIMER:
    case WM_PAINT:
        if (viewer) {
            // Start ImGui frame
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            
            // Render control panel with same style as streaming
            viewer->RenderControlPanel();
            
            // Rendering
            ImGui::Render();
            const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            viewer->m_d3dDeviceContext->OMSetRenderTargets(1, &viewer->m_mainRenderTargetView, nullptr);
            viewer->m_d3dDeviceContext->ClearRenderTargetView(viewer->m_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            
            viewer->m_swapChain->Present(1, 0);
        }
        return 0;
        
    case WM_SIZE:
        if (viewer && viewer->m_d3dDevice && wParam != SIZE_MINIMIZED) {
            viewer->CleanupRenderTarget();
            viewer->m_swapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            viewer->CreateRenderTarget();
        }
        return 0;
        
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
        
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
    
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void DirectOverlayViewer::RenderControlPanel() {
    // Full window ImGui rendering - exactly like streaming controls
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    
    if (ImGui::Begin("Overlay Controls", &m_showControls, 
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar)) {
        
        // Overlay status - equivalent to streaming status
        ImVec4 statusColor = IsRunning() ? 
            ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
        const char* status = IsRunning() ? "OVERLAY ACTIVE" : "STOPPED";
        
        ImGui::TextColored(statusColor, "Status: %s", status);
        
        ImGui::Separator();
        
        // Visualization options - same as streaming
        ImGui::Text("Visualization:");
        if (ImGui::Checkbox("Enable Dimming", &m_useDimming)) {
            SetDimmingEnabled(m_useDimming);
        }
        
        if (m_useDimming) {
            if (ImGui::SliderFloat("Dimming Rate", &m_dimmingRate, 0.1f, 3.0f, "%.1fx")) {
                SetDimmingRate(m_dimmingRate);
            }
        }
        
        ImGui::Separator();
        
        // Capture parameters - identical to streaming controls
        ImGui::Text("Capture Parameters:");
        
        if (ImGui::SliderFloat("Threshold", &m_threshold, 0.0f, 100.0f, "%.1f")) {
            SetThreshold(m_threshold);
        }
        
        int stride = static_cast<int>(m_stride);
        if (ImGui::SliderInt("Stride", &stride, 1, 30)) {
            SetStride(static_cast<uint32_t>(stride));
        }
        
        int maxEvents = static_cast<int>(m_maxEvents);
        if (ImGui::SliderInt("Max Events", &maxEvents, 1000, 100000)) {
            SetMaxEvents(static_cast<size_t>(maxEvents));
        }
        
        ImGui::Separator();
        
        // Additional overlay info
        ImGui::Text("Overlay Mode: Direct Screen");
        ImGui::TextWrapped("Events are displayed directly on your screen as colored dots.");
    }
    ImGui::End();
}

// DirectX implementation methods
bool DirectOverlayViewer::CreateDeviceD3D(HWND hWnd) {
    // Simplified DirectX 11 setup - same as streaming viewer
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_swapChain, &m_d3dDevice, &featureLevel, &m_d3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_swapChain, &m_d3dDevice, &featureLevel, &m_d3dDeviceContext);
    }
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void DirectOverlayViewer::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_d3dDeviceContext) { m_d3dDeviceContext->Release(); m_d3dDeviceContext = nullptr; }
    if (m_d3dDevice) { m_d3dDevice->Release(); m_d3dDevice = nullptr; }
}

void DirectOverlayViewer::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
    pBackBuffer->Release();
}

void DirectOverlayViewer::CleanupRenderTarget() {
    if (m_mainRenderTargetView) { m_mainRenderTargetView->Release(); m_mainRenderTargetView = nullptr; }
}

} // namespace neuromorphic