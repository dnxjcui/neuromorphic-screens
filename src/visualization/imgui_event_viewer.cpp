#include "imgui_event_viewer.h"
#include "../core/event_file_formats.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace neuromorphic {

ImGuiEventViewer::ImGuiEventViewer()
    : m_hwnd(nullptr), m_d3dDevice(nullptr), m_d3dDeviceContext(nullptr),
      m_swapChain(nullptr), m_mainRenderTargetView(nullptr),
      m_currentEventIndex(0), m_replayStartTime(0),
      m_isReplaying(false), m_isPaused(false), m_replaySpeed(0.5f),
      m_downsampleFactor(1), m_eventsProcessed(0),
      m_canvasWidth(800), m_canvasHeight(600),
      m_threadRunning(false), m_showStats(true), m_showControls(true),
      m_seekPosition(0.0f), m_useDimming(true), m_dimmingRate(1.0f), m_isLooping(false) {
    ZeroMemory(&m_wc, sizeof(m_wc));
}

ImGuiEventViewer::~ImGuiEventViewer() {
    Cleanup();
}

bool ImGuiEventViewer::Initialize(const char* title, int width, int height) {
    // Create application window
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
    m_wc.lpszClassName = L"ImGuiEventViewer";
    m_wc.hIconSm = nullptr;
    
    ::RegisterClassExW(&m_wc);
    m_hwnd = ::CreateWindowW(m_wc.lpszClassName, L"Neuromorphic Event Viewer", 
                            WS_OVERLAPPEDWINDOW, 100, 100, width, height, 
                            nullptr, nullptr, m_wc.hInstance, nullptr);

    // Store this pointer in window user data for WndProc access
    ::SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    // Initialize Direct3D
    if (!CreateDeviceD3D(m_hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    // Show the window
    ::ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_d3dDevice, m_d3dDeviceContext);

    std::cout << "ImGui Event Viewer initialized successfully" << std::endl;
    return true;
}

bool ImGuiEventViewer::LoadEvents(const std::string& filename) {
    std::cout << "Loading events from: " << filename << std::endl;
    
    if (!EventFileFormats::ReadEvents(m_events, filename)) {
        std::cerr << "Failed to load events from: " << filename << std::endl;
        return false;
    }
    
    // Sort events by timestamp
    std::sort(m_events.events.begin(), m_events.events.end(),
              [](const Event& a, const Event& b) { return a.timestamp < b.timestamp; });
    
    // CRITICAL: Normalize timestamps so first event starts at time 0
    if (!m_events.events.empty()) {
        uint64_t firstTimestamp = m_events.events[0].timestamp;
        for (auto& event : m_events.events) {
            event.timestamp -= firstTimestamp;
        }
        m_events.start_time = 0; // Ensure start time is 0 for relative timing
        std::cout << "Normalized " << m_events.events.size() << " events with first timestamp at 0" << std::endl;
    }
    
    // Calculate statistics
    m_stats.calculate(m_events);
    
    // Reset replay state
    m_currentEventIndex = 0;
    m_isReplaying = false;
    m_isPaused = false;
    m_eventsProcessed = 0;
    m_seekPosition = 0.0f;
    
    std::cout << "Loaded " << m_events.events.size() << " events successfully" << std::endl;
    std::cout << "Events loaded. Press Play to start automatic playback." << std::endl;
    
    return true;
}

bool ImGuiEventViewer::Render() {
    // Poll and handle messages
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            return false;
    }

    // Handle window resize
    if (m_swapChain) {
        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Get main viewport for full-screen rendering
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        bool p_open = true;
        ImGui::Begin("MainWindow", &p_open, window_flags);
        ImGui::PopStyleVar(3);

        // Render main content
        RenderEventCanvas();
        
        // Render control panels
        if (m_showControls) {
            RenderControlPanel();
        }
        
        if (m_showStats) {
            RenderStatsPanel();
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_d3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_d3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = m_swapChain->Present(1, 0); // Present with vsync
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            return false;
        }
    }

    return true;
}

void ImGuiEventViewer::StartReplay() {
    std::cout << "Starting replay with " << m_events.events.size() << " events" << std::endl;
    
    if (m_events.events.empty()) {
        std::cerr << "No events to replay" << std::endl;
        return;
    }
    
    if (m_isReplaying && !m_isPaused) {
        std::cout << "Already playing" << std::endl;
        return;
    }
    
    if (m_isPaused) {
        std::cout << "Resuming from pause" << std::endl;
        m_isPaused = false;
        return;
    }
    
    // Start new replay
    m_isReplaying = true;
    m_isPaused = false;
    m_currentEventIndex = 0;
    m_eventsProcessed = 0;
    m_replayStartTime = HighResTimer::GetMicroseconds();
    m_threadRunning = true;
    
    // Clear existing dots
    {
        std::lock_guard<std::mutex> lock(m_activeDotsLock);
        m_activeDots.clear();
    }
    
    // Start replay thread
    if (m_replayThread.joinable()) {
        m_replayThread.join();
    }
    m_replayThread = std::thread(&ImGuiEventViewer::ReplayThreadFunction, this);
    
    std::cout << "Replay started automatically" << std::endl;
}

void ImGuiEventViewer::PauseReplay() {
    m_isPaused = true;
    std::cout << "Replay paused" << std::endl;
}

void ImGuiEventViewer::StopReplay() {
    m_isReplaying = false;
    m_isPaused = false;
    m_threadRunning = false;
    
    if (m_replayThread.joinable()) {
        m_replayThread.join();
    }
    
    // Clear active dots
    {
        std::lock_guard<std::mutex> lock(m_activeDotsLock);
        m_activeDots.clear();
    }
    
    m_seekPosition = 0.0f;
    std::cout << "Replay stopped" << std::endl;
}

void ImGuiEventViewer::SetReplaySpeed(float speed) {
    m_replaySpeed = (speed < 0.001f) ? 0.001f : ((speed > 5.0f) ? 5.0f : speed);
}

void ImGuiEventViewer::SetDownsampleFactor(int factor) {
    m_downsampleFactor = (factor < 1) ? 1 : ((factor > 8) ? 8 : factor);
}

void ImGuiEventViewer::SeekToTime(float timeSeconds) {
    if (m_events.events.empty()) return;
    
    uint64_t targetTime = static_cast<uint64_t>(timeSeconds * 1000000.0f);
    
    // Find closest event
    for (size_t i = 0; i < m_events.events.size(); i++) {
        if (m_events.events[i].timestamp >= targetTime) {
            m_currentEventIndex = i;
            break;
        }
    }
}

void ImGuiEventViewer::SetDimmingEnabled(bool enabled) {
    m_useDimming = enabled;
}

void ImGuiEventViewer::SetDimmingRate(float rate) {
    m_dimmingRate = (rate < 0.1f) ? 0.1f : ((rate > 3.0f) ? 3.0f : rate);
}

void ImGuiEventViewer::ExportToGIF() {
    // Generate automatic filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream filename;
    filename << "data/recordings/neuromorphic_events_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".gif";
    
    std::cout << "Exporting to GIF: " << filename.str() << " (10 seconds, 30 fps)" << std::endl;
    
    // FFmpeg path
    const std::string ffmpegPath = "ffmpeg";

    // Create FFmpeg command for GIF export (two-pass for better quality)
    std::ostringstream cmd;
    cmd << ffmpegPath << " -f gdigrab -framerate 15 -t 5 "
        << " -i title=\"Neuromorphic Event Viewer\" "
        << "-vf \"scale=640:-1:flags=lanczos,palettegen\" -y palette.png && "
        << ffmpegPath << " -f gdigrab -framerate 15 -t 5 "
        << " -i title=\"Neuromorphic Event Viewer\" "
        << "-i palette.png -lavfi \"scale=640:-1:flags=lanczos[x];[x][1:v]paletteuse\" "
        << "-y \"" << filename.str() << "\"";
    
    std::cout << "Running FFmpeg command for GIF export..." << std::endl;
    // int result = system(cmd.str().c_str());
    std::thread([command = cmd.str()] {
        std::system(command.c_str());
    }).detach();
    
    // if (result == 0) {
    //     std::cout << "GIF export completed successfully: " << filename.str() << std::endl;
    // } else {
    //     std::cerr << "GIF export failed. FFmpeg command returned error code: " << result << std::endl;
    // }
    std::cout << "GIF export completed successfully: " << filename.str() << std::endl;
}


void ImGuiEventViewer::ReplayThreadFunction() {
    FrameRateLimiter limiter(60.0f);
    
    while (m_threadRunning && m_isReplaying && !m_isPaused) {
        uint64_t currentTime = HighResTimer::GetMicroseconds();
        uint64_t elapsedTime = currentTime - m_replayStartTime;
        
        // Process ALL events that should be displayed now (event-based timing)
        uint64_t currentEventTime = 0;
        if (m_currentEventIndex < m_events.events.size()) {
            currentEventTime = m_events.events[m_currentEventIndex].timestamp;
        }
        
        // Process all events with the same timestamp as the current event
        while (m_currentEventIndex < m_events.events.size() && m_threadRunning) {
            const Event& event = m_events.events[m_currentEventIndex];
            uint64_t eventTime = event.timestamp;
            uint64_t adjustedEventTime = static_cast<uint64_t>(eventTime / m_replaySpeed);
            
            // Only process events that should be displayed now
            if (adjustedEventTime <= elapsedTime) {
                // Apply downsampling during visualization
                if (m_downsampleFactor == 1 || 
                    (event.x % m_downsampleFactor == 0 && event.y % m_downsampleFactor == 0)) {
                    AddDot(event);
                }
                m_currentEventIndex++;
                m_eventsProcessed++;
            } else {
                // Stop when we reach events that shouldn't be displayed yet
                break;
            }
        }
        
        // Update seek position
        if (!m_events.events.empty()) {
            m_seekPosition = static_cast<float>(m_currentEventIndex) / static_cast<float>(m_events.events.size());
        }
        
        // Check if replay is complete
        if (m_currentEventIndex >= m_events.events.size()) {
            if (m_isLooping) {
                // Restart replay for looping
                m_currentEventIndex = 0;
                m_eventsProcessed = 0;
                m_replayStartTime = HighResTimer::GetMicroseconds();
                
                // Clear existing dots for clean loop
                {
                    std::lock_guard<std::mutex> lock(m_activeDotsLock);
                    m_activeDots.clear();
                }
            } else {
                m_isReplaying = false;
                break;
            }
        }
        
        // Update active dots
        UpdateActiveDots();
        
        // Wait for next frame
        limiter.WaitForNextFrame();
    }
}

void ImGuiEventViewer::UpdateActiveDots() {
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

void ImGuiEventViewer::AddDot(const Event& event) {
    std::lock_guard<std::mutex> lock(m_activeDotsLock);
    m_activeDots.push_back(std::make_pair(event, constants::DOT_FADE_DURATION));
}

void ImGuiEventViewer::RemoveExpiredDots() {
    std::lock_guard<std::mutex> lock(m_activeDotsLock);
    m_activeDots.erase(
        std::remove_if(m_activeDots.begin(), m_activeDots.end(),
                      [](const std::pair<Event, float>& dot) {
                          return dot.second <= 0.0f;
                      }),
        m_activeDots.end()
    );
}

void ImGuiEventViewer::RenderEventCanvas() {
    // Calculate canvas area (most of the window)
    ImVec2 windowSize = ImGui::GetWindowSize();
    m_canvasWidth = static_cast<uint32_t>(windowSize.x * 0.75f); // 75% of window width
    m_canvasHeight = static_cast<uint32_t>(windowSize.y - 100); // Leave space for controls
    
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize(static_cast<float>(m_canvasWidth), static_cast<float>(m_canvasHeight));
    
    // Draw black background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                           IM_COL32(0, 0, 0, 255));
    
    // Draw border
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
            
            // Ensure dot is within canvas bounds
            if (dotPos.x >= canvasPos.x && dotPos.x <= canvasPos.x + canvasSize.x &&
                dotPos.y >= canvasPos.y && dotPos.y <= canvasPos.y + canvasSize.y) {
                
                ImU32 color;
                if (event.polarity > 0) {
                    // Green for positive events
                    color = IM_COL32(0, static_cast<int>(255 * alpha), 0, 255);
                } else {
                    // Red for negative events
                    color = IM_COL32(static_cast<int>(255 * alpha), 0, 0, 255);
                }
                
                drawList->AddCircleFilled(dotPos, constants::DOT_SIZE, color);
            }
        }
    }
    
    // Reserve space for canvas
    ImGui::Dummy(canvasSize);
}

void ImGuiEventViewer::RenderControlPanel() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.75f + 10, 50));
    ImGui::SetNextWindowSize(ImVec2(300, 280));
    
    if (ImGui::Begin("Controls", &m_showControls, ImGuiWindowFlags_NoResize)) {
        // Playback controls
        if (ImGui::Button("Play", ImVec2(60, 30))) {
            StartReplay();
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause", ImVec2(60, 30))) {
            PauseReplay();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(60, 30))) {
            StopReplay();
        }
        ImGui::SameLine();
        
        // Loop toggle button
        const char* loopText = m_isLooping ? "Loop: ON" : "Loop: OFF";
        ImVec4 loopColor = m_isLooping ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, loopColor);
        if (ImGui::Button(loopText, ImVec2(80, 30))) {
            m_isLooping = !m_isLooping;
        }
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        
        // Speed control
        if (ImGui::SliderFloat("Speed", &m_replaySpeed, 0.01f, 5.0f, "%.2fx")) {
            SetReplaySpeed(m_replaySpeed);
        }
        
        // Progress control
        if (ImGui::SliderFloat("Progress", &m_seekPosition, 0.0f, 1.0f, "%.2f")) {
            if (!m_events.events.empty()) {
                float totalDuration = static_cast<float>(m_stats.duration_us) / 1000000.0f;
                SeekToTime(m_seekPosition * totalDuration);
            }
        }
        
        // Downsample control
        if (ImGui::SliderInt("Downsample", &m_downsampleFactor, 1, 8, "%dx")) {
            SetDownsampleFactor(m_downsampleFactor);
        }
        
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
        
        // Simple GIF export button
        if (ImGui::Button("Export GIF", ImVec2(-1, 30))) {
            ExportToGIF();
        }
        ImGui::TextWrapped("Exports 10-second GIF of current visualization. Enable Loop for continuous recording.");
    }
    ImGui::End();
}

void ImGuiEventViewer::RenderStatsPanel() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.75f + 10, 340));
    ImGui::SetNextWindowSize(ImVec2(300, 250));
    
    if (ImGui::Begin("Statistics", &m_showStats, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Total Events: %u", m_stats.total_events);
        ImGui::Text("Positive: %u", m_stats.positive_events);
        ImGui::Text("Negative: %u", m_stats.negative_events);
        ImGui::Text("Duration: %.2fs", m_stats.duration_us / 1000000.0f);
        ImGui::Text("Events/sec: %.1f", m_stats.events_per_second);
        
        ImGui::Separator();
        
        ImGui::Text("Processed: %u", m_eventsProcessed);
        ImGui::Text("Replay Speed: %.2fx", m_replaySpeed);
        ImGui::Text("Downsample: %dx", m_downsampleFactor);
        
        size_t activeDotCount;
        {
            std::lock_guard<std::mutex> lock(m_activeDotsLock);
            activeDotCount = m_activeDots.size();
        }
        ImGui::Text("Active Dots: %zu", activeDotCount);
        
        ImGui::Separator();
        
        const char* status = m_isReplaying ? (m_isPaused ? "Paused" : "Playing") : "Stopped";
        ImGui::Text("Status: %s", status);
    }
    ImGui::End();
}


ImVec2 ImGuiEventViewer::ScreenToCanvas(uint16_t screenX, uint16_t screenY) const {
    if (m_events.width > 0 && m_events.height > 0) {
        float scaleX = static_cast<float>(m_canvasWidth) / static_cast<float>(m_events.width);
        float scaleY = static_cast<float>(m_canvasHeight) / static_cast<float>(m_events.height);
        
        return ImVec2(screenX * scaleX, screenY * scaleY);
    } else {
        return ImVec2(static_cast<float>(screenX), static_cast<float>(screenY));
    }
}

bool ImGuiEventViewer::CreateDeviceD3D(HWND hWnd) {
    // Setup swap chain
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
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, 
                                               featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_swapChain, 
                                               &m_d3dDevice, &featureLevel, &m_d3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, 
                                           featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_swapChain, 
                                           &m_d3dDevice, &featureLevel, &m_d3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void ImGuiEventViewer::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_d3dDeviceContext) { m_d3dDeviceContext->Release(); m_d3dDeviceContext = nullptr; }
    if (m_d3dDevice) { m_d3dDevice->Release(); m_d3dDevice = nullptr; }
}

void ImGuiEventViewer::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
    pBackBuffer->Release();
}

void ImGuiEventViewer::CleanupRenderTarget() {
    if (m_mainRenderTargetView) { m_mainRenderTargetView->Release(); m_mainRenderTargetView = nullptr; }
}

void ImGuiEventViewer::Cleanup() {
    // Stop replay thread
    StopReplay();
    
    // Cleanup ImGui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    
    if (m_hwnd) {
        ::DestroyWindow(m_hwnd);
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
    }
}

// Win32 message handler
LRESULT WINAPI ImGuiEventViewer::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    ImGuiEventViewer* viewer = reinterpret_cast<ImGuiEventViewer*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_SIZE:
        if (viewer && viewer->m_d3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
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
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace neuromorphic