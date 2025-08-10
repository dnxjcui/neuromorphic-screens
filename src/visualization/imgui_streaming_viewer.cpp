#include "imgui_streaming_viewer.h"
#include "../core/event_file_formats.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace neuromorphic {

ImGuiStreamingViewer::ImGuiStreamingViewer(const std::string& title, StreamingApp& streamingApp) : 
      m_hwnd(nullptr), m_d3dDevice(nullptr), m_d3dDeviceContext(nullptr), 
      m_swapChain(nullptr), m_mainRenderTargetView(nullptr),
      m_streamingApp(streamingApp),
      m_canvasWidth(800), m_canvasHeight(600),
      m_threadRunning(false), m_showStats(true), m_showControls(true),
      m_useDimming(true), m_dimmingRate(1.0f),
      m_temporalIndex(100000, 10000), // 100ms time window, max 10000 recent events
      m_lastSecondEvents(0), m_lastSecondTimestamp(HighResTimer::GetMicroseconds()) {  
    ZeroMemory(&m_wc, sizeof(m_wc));
}

ImGuiStreamingViewer::~ImGuiStreamingViewer() {
    Cleanup();
}

bool ImGuiStreamingViewer::Initialize() {
    // Register window class
    m_wc.cbSize = sizeof(WNDCLASSEXW);
    m_wc.style = CS_CLASSDC;
    m_wc.lpfnWndProc = WndProc;
    m_wc.cbClsExtra = 0L;
    m_wc.cbWndExtra = 0L;
    m_wc.hInstance = GetModuleHandle(nullptr);
    m_wc.hIcon = nullptr;
    m_wc.hCursor = nullptr;
    m_wc.hbrBackground = nullptr;
    m_wc.lpszMenuName = nullptr;
    m_wc.lpszClassName = L"ImGuiStreamingViewer";
    m_wc.hIconSm = nullptr;
    RegisterClassExW(&m_wc);

    // Create window
    m_hwnd = CreateWindowW(m_wc.lpszClassName, L"Neuromorphic Event Streaming", 
                          WS_OVERLAPPEDWINDOW, 100, 100, 1200, 800, 
                          nullptr, nullptr, m_wc.hInstance, nullptr);
    if (!m_hwnd) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }

    // Store this pointer in window user data
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Initialize Direct3D
    if (!CreateDeviceD3D(m_hwnd)) {
        std::cerr << "Failed to create DirectX device" << std::endl;
        CleanupDeviceD3D();
        UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    // Show window
    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_d3dDevice, m_d3dDeviceContext);

    // Start visualization thread
    m_threadRunning = true;
    m_visualizationThread = std::thread(&ImGuiStreamingViewer::VisualizationThreadFunction, this);

    std::cout << "Streaming GUI initialized successfully" << std::endl;
    return true;
}

void ImGuiStreamingViewer::Run() {
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Start ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
        
        if (ImGui::Begin("Neuromorphic Event Streaming", nullptr, 
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar)) {
            
            // Render main content
            RenderEventCanvas();
            
            // Render control panels
            if (m_showControls) {
                RenderControlPanel();
            }
            
            if (m_showStats) {
                RenderStatsPanel();
            }
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_d3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_d3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_swapChain->Present(1, 0); // Present with vsync
    }
}

void ImGuiStreamingViewer::ExportToGIF() {
    // Generate automatic filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream filename;
    filename << "data/recordings/streaming_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".gif";
    
    std::cout << "Exporting streaming visualization to GIF: " << filename.str() << " (10 seconds, 30 fps)" << std::endl;
    
    // FFmpeg path
    const std::string ffmpegPath = "\"C:\\\\Users\\\\dnxjc\\\\AppData\\\\Local\\\\Microsoft\\\\WinGet\\\\Packages\\\\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\\\\ffmpeg-7.1-full_build\\\\bin\\\\ffmpeg.exe\"";
    
    // Create FFmpeg command for GIF export (two-pass for better quality)
    std::ostringstream cmd;
    cmd << ffmpegPath << " -f gdigrab -framerate 30 -t 10 "
        << " -i title=\"Neuromorphic Event Streaming\" "
        << "-vf \"scale=640:-1:flags=lanczos,palettegen\" -y palette.png && "
        << ffmpegPath << " -f gdigrab -framerate 30 -t 10 "
        << " -i title=\"Neuromorphic Event Streaming\" "
        << "-i palette.png -lavfi \"scale=640:-1:flags=lanczos[x];[x][1:v]paletteuse\" "
        << "-y \"" << filename.str() << "\"";
    
    std::cout << "Running FFmpeg command for GIF export..." << std::endl;
    
    // Run in separate thread to avoid blocking GUI
    std::thread([command = cmd.str()] {
        std::system(command.c_str());
    }).detach();
    
    std::cout << "GIF export started: " << filename.str() << std::endl;
}

void ImGuiStreamingViewer::SetDimmingEnabled(bool enabled) {
    m_useDimming = enabled;
}

void ImGuiStreamingViewer::SetDimmingRate(float rate) {
    m_dimmingRate = (rate < 0.1f) ? 0.1f : ((rate > 3.0f) ? 3.0f : rate);
}

void ImGuiStreamingViewer::VisualizationThreadFunction() {
    FrameRateLimiter limiter(60.0f);
    
    while (m_threadRunning) {
        uint64_t currentTime = HighResTimer::GetMicroseconds();
        const EventStream& stream = m_streamingApp.getEventStream();
        
        // High-performance recent event processing using temporal index
        if (stream.size() > 0) {
            // Update temporal index with latest events (O(k) where k = new events)
            m_temporalIndex.updateFromStream(stream, currentTime);
            
            // Get recent events efficiently (O(k) where k = recent events, not total events)
            auto recentEvents = m_temporalIndex.getRecentEvents(currentTime);
            
            // Replace the old O(n) AddDot calls with batch processing
            {
                std::lock_guard<std::mutex> lock(m_activeDotsLock);
                
                // Clear old dots and replace with current recent events
                m_activeDots.clear();
                m_activeDots.reserve(recentEvents.size());
                
                for (const auto& event : recentEvents) {
                    m_activeDots.push_back(std::make_pair(event, constants::DOT_FADE_DURATION));
                }
            }
            
            // Debug output with performance stats
            static int frameCount = 0;
            if (frameCount++ % 60 == 0 && !recentEvents.empty()) {
                size_t totalProcessed, duplicatesSkipped, bufferSize;
                m_temporalIndex.getPerformanceStats(totalProcessed, duplicatesSkipped, bufferSize);
                std::cout << "\nStreaming GUI: " << recentEvents.size() << " active dots, "
                         << "buffer: " << bufferSize << ", "
                         << "processed: " << totalProcessed << ", "
                         << "duplicates: " << duplicatesSkipped;
            }
        }
        
        // Update active dots (dimming) - now much smaller set to process
        UpdateActiveDots();
        
        limiter.WaitForNextFrame();
    }
}

void ImGuiStreamingViewer::AddDot(const Event& event) {
    // This method is now primarily for compatibility
    // Main event processing now uses batch updates in VisualizationThreadFunction
    std::lock_guard<std::mutex> lock(m_activeDotsLock);
    m_activeDots.push_back(std::make_pair(event, constants::DOT_FADE_DURATION));
}

void ImGuiStreamingViewer::UpdateActiveDots() {
    static uint64_t lastUpdateTime = 0;
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    
    if (lastUpdateTime == 0) {
        lastUpdateTime = currentTime;
        return;
    }
    
    float deltaTime = static_cast<float>(currentTime - lastUpdateTime) / 1000000.0f;
    lastUpdateTime = currentTime;
    
    // Thread-safe update of fade timers
    {
        std::lock_guard<std::mutex> lock(m_activeDotsLock);
        for (auto& dot : m_activeDots) {
            if (m_useDimming) {
                // Gradual dimming: reduce fade time based on dimming rate
                dot.second -= deltaTime * m_dimmingRate;
            } else {
                // Original behavior: standard fade
                dot.second -= deltaTime;
            }
        }
    }
    
    // Remove expired dots (only when they're completely faded)
    RemoveExpiredDots();
}

void ImGuiStreamingViewer::RemoveExpiredDots() {
    std::lock_guard<std::mutex> lock(m_activeDotsLock);
    m_activeDots.erase(
        std::remove_if(m_activeDots.begin(), m_activeDots.end(),
                      [](const std::pair<Event, float>& dot) {
                          return dot.second <= 0.0f;
                      }),
        m_activeDots.end()
    );
}

void ImGuiStreamingViewer::RenderEventCanvas() {
    // Dynamic canvas sizing based on available window space
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 canvasPos = ImVec2(50, 50);
    
    // Use most of the window space for canvas, leaving room for controls
    float controlsHeight = m_showStats ? 200 : 100; // Space for stats/controls
    ImVec2 canvasSize = ImVec2(
        windowSize.x - 100,  // Leave 50px margin on each side
        windowSize.y - canvasPos.y - controlsHeight  // Leave space for controls below
    );
    
    // Ensure minimum canvas size
    if (canvasSize.x < 400.0f) canvasSize.x = 400.0f;
    if (canvasSize.y < 300.0f) canvasSize.y = 300.0f;
    
    // Update canvas dimensions for coordinate mapping
    m_canvasWidth = static_cast<uint32_t>(canvasSize.x);
    m_canvasHeight = static_cast<uint32_t>(canvasSize.y);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Draw canvas background
    drawList->AddRectFilled(canvasPos, 
                           ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                           IM_COL32(20, 20, 20, 255));
    
    // Draw canvas border
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                     IM_COL32(100, 100, 100, 255));
    
    // Draw active dots
    {
        std::lock_guard<std::mutex> lock(m_activeDotsLock);
        for (const auto& dot : m_activeDots) {
            const Event& event = dot.first;
            float alpha = dot.second / constants::DOT_FADE_DURATION;
            
            ImVec2 dotPos = ScreenToCanvas(event.x, event.y);
            dotPos.x += canvasPos.x;
            dotPos.y += canvasPos.y;
            
            // Color based on polarity with alpha
            ImU32 color;
            if (event.polarity > 0) {
                color = IM_COL32(0, static_cast<int>(255 * alpha), 0, 255); // Green for positive
            } else {
                color = IM_COL32(static_cast<int>(255 * alpha), 0, 0, 255); // Red for negative
            }
            
            drawList->AddCircleFilled(dotPos, constants::DOT_SIZE, color);
        }
    }
    
    // Reserve space for canvas
    ImGui::Dummy(canvasSize);
}

void ImGuiStreamingViewer::RenderControlPanel() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.75f + 10, 50));
    ImGui::SetNextWindowSize(ImVec2(300, 320));
    
    if (ImGui::Begin("Streaming Controls", &m_showControls, ImGuiWindowFlags_NoResize)) {
        // Streaming status
        const char* status = m_streamingApp.isRunning() ? "STREAMING" : "STOPPED";
        ImVec4 statusColor = m_streamingApp.isRunning() ? 
            ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
        
        ImGui::TextColored(statusColor, "Status: %s", status);
        
        ImGui::Separator();
        
        // Visualization options
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
        
        // Capture parameters
        ImGui::Text("Capture Parameters:");
        
        float threshold = m_streamingApp.getThreshold();
        if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 100.0f, "%.1f")) {
            m_streamingApp.setThreshold(threshold);
        }
        
        int stride = static_cast<int>(m_streamingApp.getStride());
        if (ImGui::SliderInt("Stride", &stride, 1, 30)) {
            m_streamingApp.setStride(static_cast<uint32_t>(stride));
        }
        
        int maxEvents = static_cast<int>(m_streamingApp.getMaxEvents());
        if (ImGui::SliderInt("Max Events", &maxEvents, 1000, 100000)) {
            m_streamingApp.setMaxEvents(static_cast<size_t>(maxEvents));
        }
        
        ImGui::Separator();
        
        // Export functionality
        if (ImGui::Button("Export GIF (10s)", ImVec2(-1, 30))) {
            ExportToGIF();
        }
        ImGui::TextWrapped("Records 10 seconds of streaming visualization.");
    }
    ImGui::End();
}

void ImGuiStreamingViewer::RenderStatsPanel() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.75f + 10, 270));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    
    if (ImGui::Begin("Statistics", &m_showStats, ImGuiWindowFlags_NoResize)) {
        const EventStream& stream = m_streamingApp.getEventStream();
        
        ImGui::Text("Context Window: %zu / %zu", stream.size(), stream.max_events);
        ImGui::Text("Total Generated: %llu", stream.total_events_generated);
        ImGui::Text("Resolution: %ux%u", stream.width, stream.height);
        
        // Active dots count
        size_t activeDotCount;
        {
            std::lock_guard<std::mutex> lock(m_activeDotsLock);
            activeDotCount = m_activeDots.size();
        }
        ImGui::Text("Active Dots: %zu", activeDotCount);
        
        ImGui::Separator();
        
        // Streaming statistics
        if (m_streamingApp.isRunning()) {
            uint64_t currentTime = HighResTimer::GetMicroseconds();
            uint64_t streamingDuration = currentTime - stream.start_time;
            float durationSeconds = static_cast<float>(streamingDuration) / 1000000.0f;
            // float eventsPerSecond = stream.total_events_generated / (durationSeconds > 1.0f ? durationSeconds : 1.0f);

            // calculate events per second past second
            uint64_t eventsPastSecond = stream.total_events_generated - m_lastSecondEvents;

            float eventsPerSecond = eventsPastSecond / (durationSeconds > 1.0f ? durationSeconds : 1.0f);
            m_lastSecondEvents = stream.total_events_generated;
            m_lastSecondTimestamp = currentTime;

            ImGui::Text("Duration: %.1fs", durationSeconds);
            ImGui::Text("Events/sec: %.0f", eventsPerSecond);
        }
    }
    ImGui::End();
}

ImVec2 ImGuiStreamingViewer::ScreenToCanvas(uint16_t screenX, uint16_t screenY) const {
    const EventStream& stream = m_streamingApp.getEventStream();
    if (stream.width > 0 && stream.height > 0) {
        float scaleX = static_cast<float>(m_canvasWidth) / static_cast<float>(stream.width);
        float scaleY = static_cast<float>(m_canvasHeight) / static_cast<float>(stream.height);
        
        return ImVec2(screenX * scaleX, screenY * scaleY);
    } else {
        return ImVec2(screenX, screenY);
    }
}

// DirectX implementation methods (simplified versions from original)
bool ImGuiStreamingViewer::CreateDeviceD3D(HWND hWnd) {
    // Simplified DirectX 11 setup
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

void ImGuiStreamingViewer::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_d3dDeviceContext) { m_d3dDeviceContext->Release(); m_d3dDeviceContext = nullptr; }
    if (m_d3dDevice) { m_d3dDevice->Release(); m_d3dDevice = nullptr; }
}

void ImGuiStreamingViewer::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
    pBackBuffer->Release();
}

void ImGuiStreamingViewer::CleanupRenderTarget() {
    if (m_mainRenderTargetView) { m_mainRenderTargetView->Release(); m_mainRenderTargetView = nullptr; }
}

void ImGuiStreamingViewer::Cleanup() {
    // Stop visualization thread
    m_threadRunning = false;
    if (m_visualizationThread.joinable()) {
        m_visualizationThread.join();
    }
    
    // Cleanup ImGui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
}

LRESULT WINAPI ImGuiStreamingViewer::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    ImGuiStreamingViewer* viewer = reinterpret_cast<ImGuiStreamingViewer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (msg) {
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
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Virtual method implementations for base class
void ImGuiStreamingViewer::RenderMainContent() {
    RenderEventCanvas();
}

void ImGuiStreamingViewer::UpdateLogic() {
    // Update event visualization for real-time streaming
    // This gets called each frame to update the display
}

} // namespace neuromorphic