#include "imgui_viewer_base.h"
#include <iostream>
#include <algorithm>

// Forward declare for Win32 integration  
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace neuromorphic {

ImGuiViewerBase::ImGuiViewerBase() 
    : m_initialized(false)
    , m_hwnd(nullptr)
    , m_pd3dDevice(nullptr)
    , m_pd3dDeviceContext(nullptr)  
    , m_pSwapChain(nullptr)
    , m_pMainRenderTargetView(nullptr)
    , m_windowWidth(1280)
    , m_windowHeight(720)
{
    memset(&m_wc, 0, sizeof(m_wc));
}

ImGuiViewerBase::~ImGuiViewerBase() {
    Cleanup();
}

bool ImGuiViewerBase::Initialize(const char* title, int width, int height) {
    if (m_initialized) {
        return true;
    }
    
    m_windowTitle = title;
    m_windowWidth = width;
    m_windowHeight = height;
    
    // Create application window
    m_wc.cbSize = sizeof(WNDCLASSEX);
    m_wc.style = CS_CLASSDC;
    m_wc.lpfnWndProc = WndProc;
    m_wc.cbClsExtra = 0L;
    m_wc.cbWndExtra = 0L;
    m_wc.hInstance = GetModuleHandle(nullptr);
    m_wc.hIcon = nullptr;
    m_wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    m_wc.hbrBackground = nullptr;
    m_wc.lpszMenuName = nullptr;
    m_wc.lpszClassName = "NeuromorphicImGuiViewer";
    m_wc.hIconSm = nullptr;
    
    if (!RegisterClassEx(&m_wc)) {
        std::cerr << "Failed to register window class" << std::endl;
        return false;
    }
    
    m_hwnd = CreateWindow(m_wc.lpszClassName, 
                         "Neuromorphic Event Viewer", 
                         WS_OVERLAPPEDWINDOW,
                         100, 100, 
                         m_windowWidth, m_windowHeight,
                         nullptr, nullptr, 
                         m_wc.hInstance, nullptr);
    
    if (!m_hwnd) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }
    
    // Initialize Direct3D
    if (!CreateDeviceD3D(m_hwnd)) {
        std::cerr << "Failed to create Direct3D device" << std::endl;
        CleanupDeviceD3D();
        UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }
    
    // Show the window
    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    if (!ImGui_ImplWin32_Init(m_hwnd)) {
        std::cerr << "Failed to initialize ImGui Win32 backend" << std::endl;
        return false;
    }
    
    if (!ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext)) {
        std::cerr << "Failed to initialize ImGui DirectX11 backend" << std::endl;
        return false;
    }
    
    m_initialized = true;
    std::cout << "ImGui viewer initialized successfully (" << m_windowWidth << "x" << m_windowHeight << ")" << std::endl;
    
    return true;
}

bool ImGuiViewerBase::Render() {
    if (!m_initialized) {
        return false;
    }
    
    // Poll and handle messages
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) {
            return false;
        }
    }
    
    // Handle input
    HandleInput();
    
    // Update logic
    UpdateLogic();
    
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // Render specialized content
    RenderMainContent();
    RenderControlPanel();
    
    // Rendering
    ImGui::Render();
    const float clear_color[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    m_pd3dDeviceContext->OMSetRenderTargets(1, &m_pMainRenderTargetView, nullptr);
    m_pd3dDeviceContext->ClearRenderTargetView(m_pMainRenderTargetView, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    
    m_pSwapChain->Present(1, 0); // Present with vsync
    
    return true;
}

void ImGuiViewerBase::Cleanup() {
    if (!m_initialized) {
        return;
    }
    
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    CleanupDeviceD3D();
    
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
    
    m_initialized = false;
    std::cout << "ImGui viewer cleaned up successfully" << std::endl;
}

void ImGuiViewerBase::RenderEventDots(const std::vector<Event>& events, float canvasWidth, float canvasHeight, 
                                    uint32_t screenWidth, uint32_t screenHeight, float fadeAlpha) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    
    for (const auto& event : events) {
        ImVec2 dot_pos = ScreenToCanvas(event.x, event.y, canvasWidth, canvasHeight, screenWidth, screenHeight);
        dot_pos.x += canvas_p0.x;
        dot_pos.y += canvas_p0.y;
        
        ImU32 color = GetEventColor(event.polarity, fadeAlpha);
        draw_list->AddCircleFilled(dot_pos, 2.0f, color);
    }
}

void ImGuiViewerBase::RenderEventDotsWithFade(const std::vector<Event>& events, float canvasWidth, float canvasHeight,
                                            uint32_t screenWidth, uint32_t screenHeight, uint64_t currentTime, 
                                            float fadeDuration) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    
    for (const auto& event : events) {
        // Calculate fade based on time
        float age = (currentTime - event.timestamp) / 1000.0f; // Convert to milliseconds
        if (age > fadeDuration) continue; // Skip events that are too old
        
        float alpha = (std::max)(0.0f, 1.0f - (age / fadeDuration));
        
        ImVec2 dot_pos = ScreenToCanvas(event.x, event.y, canvasWidth, canvasHeight, screenWidth, screenHeight);
        dot_pos.x += canvas_p0.x;
        dot_pos.y += canvas_p0.y;
        
        ImU32 color = GetEventColor(event.polarity, alpha);
        draw_list->AddCircleFilled(dot_pos, 2.0f, color);
    }
}

ImVec2 ImGuiViewerBase::ScreenToCanvas(uint16_t screenX, uint16_t screenY, float canvasWidth, float canvasHeight,
                                     uint32_t screenWidth, uint32_t screenHeight) {
    float scaleX = canvasWidth / static_cast<float>(screenWidth);
    float scaleY = canvasHeight / static_cast<float>(screenHeight);
    
    return ImVec2(screenX * scaleX, screenY * scaleY);
}

void ImGuiViewerBase::BeginMainWindow(const char* title, bool* p_open) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(m_windowWidth), static_cast<float>(m_windowHeight)), ImGuiCond_FirstUseEver);
    
    ImGui::Begin(title, p_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
}

void ImGuiViewerBase::EndMainWindow() {
    ImGui::End();
}

void ImGuiViewerBase::RenderStatistics(uint32_t totalEvents, float eventsPerSec, uint32_t activeEvents) {
    if (ImGui::CollapsingHeader("Statistics")) {
        ImGui::Text("Total Events: %u", totalEvents);
        ImGui::Text("Events/sec: %.1f", eventsPerSec);
        if (activeEvents > 0) {
            ImGui::Text("Active Events: %u", activeEvents);
        }
    }
}

ImU32 ImGuiViewerBase::GetEventColor(int8_t polarity, float alpha) {
    if (polarity > 0) {
        // Green for positive events
        return ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, alpha));
    } else {
        // Red for negative events  
        return ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.0f, 0.0f, alpha));
    }
}

bool ImGuiViewerBase::CreateDeviceD3D(HWND hWnd) {
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
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, 
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, 
        &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
    
    if (res == DXGI_ERROR_UNSUPPORTED) {
        // Try WARP device
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, 
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, 
            &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
    }
    
    if (res != S_OK) {
        return false;
    }

    CreateRenderTarget();
    return true;
}

void ImGuiViewerBase::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
    if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
    if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
}

void ImGuiViewerBase::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pMainRenderTargetView);
    pBackBuffer->Release();
}

void ImGuiViewerBase::CleanupRenderTarget() {
    if (m_pMainRenderTargetView) { 
        m_pMainRenderTargetView->Release(); 
        m_pMainRenderTargetView = nullptr; 
    }
}

LRESULT WINAPI ImGuiViewerBase::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            ImGuiViewerBase* viewer = reinterpret_cast<ImGuiViewerBase*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            if (viewer && viewer->m_pd3dDevice) {
                viewer->CleanupRenderTarget();
                viewer->m_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                viewer->CreateRenderTarget();
            }
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
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

} // namespace neuromorphic