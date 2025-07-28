#include "simple_viewer.h"
#include "../core/event_file.h"
#include "../core/event_file_formats.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace neuromorphic {

SimpleViewer::SimpleViewer()
    : m_hwnd(nullptr)
    , m_hdc(nullptr)
    , m_hInstance(nullptr)
    , m_currentEventIndex(0)
    , m_replayStartTime(0)
    , m_isReplaying(false)
    , m_isPaused(false)
    , m_replaySpeed(1.0f)
    , m_canvasWidth(1920)
    , m_canvasHeight(1080)
    , m_scaleFactor(1)
    , m_threadRunning(false)
    , m_eventsProcessed(0)
    , m_currentFPS(0.0f) {
}

SimpleViewer::~SimpleViewer() {
    StopReplay();
    if (m_replayThread.joinable()) {
        m_replayThread.join();
    }
}

bool SimpleViewer::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"NeuromorphicViewer";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    if (!RegisterClassExW(&wc)) {
        std::cerr << "Failed to register window class" << std::endl;
        return false;
    }
    
    // Calculate window size based on canvas and scale factor
    uint32_t windowWidth = m_canvasWidth * m_scaleFactor + 300; // Extra space for stats
    uint32_t windowHeight = m_canvasHeight * m_scaleFactor + 100;
    
    // Create window
    m_hwnd = CreateWindowExW(
        0,
        L"NeuromorphicViewer",
        L"Neuromorphic Event Viewer - Pixel Mode",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        nullptr, nullptr, hInstance, this
    );
    
    if (!m_hwnd) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }
    
    // Get device context
    m_hdc = GetDC(m_hwnd);
    if (!m_hdc) {
        std::cerr << "Failed to get device context" << std::endl;
        return false;
    }
    
    // Set up drawing mode for pixel-perfect rendering
    SetStretchBltMode(m_hdc, COLORONCOLOR);
    SetBkMode(m_hdc, OPAQUE);
    
    return true;
}

bool SimpleViewer::LoadEvents(const std::string& filename) {
    if (!EventFileFormats::ReadEvents(m_events, filename)) {
        std::cerr << "Failed to load events from " << filename << std::endl;
        return false;
    }
    
    // Calculate statistics
    m_stats.calculate(m_events);
    
    // Set canvas size based on event stream
    m_canvasWidth = m_events.width;
    m_canvasHeight = m_events.height;
    
    // Auto-adjust scale factor for large screens
    if (m_canvasWidth > 1920 || m_canvasHeight > 1080) {
        m_scaleFactor = 2; // 2x zoom for high-resolution screens
    }
    
    // Resize window to fit scaled canvas
    uint32_t windowWidth = m_canvasWidth * m_scaleFactor + 300;
    uint32_t windowHeight = m_canvasHeight * m_scaleFactor + 100;
    SetWindowPos(m_hwnd, nullptr, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER);
    
    std::cout << "Loaded " << m_events.events.size() << " events" << std::endl;
    std::cout << "Canvas size: " << m_canvasWidth << "x" << m_canvasHeight << std::endl;
    std::cout << "Scale factor: " << m_scaleFactor << "x" << std::endl;
    
    return true;
}

void SimpleViewer::Show() {
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void SimpleViewer::StartReplay() {
    if (m_events.events.empty()) {
        std::cout << "No events to replay" << std::endl;
        return;
    }
    // If previous thread finished, join it
    if (m_replayThread.joinable() && !m_threadRunning) {
        m_replayThread.join();
    }
    if (!m_isReplaying) {
        m_isReplaying = true;
        m_isPaused = false;
        m_currentEventIndex = 0;
        m_replayStartTime = HighResTimer::GetMicroseconds();
        m_eventsProcessed = 0;
        m_activePixels.clear();
        
        if (!m_threadRunning) {
            m_threadRunning = true;
            m_replayThread = std::thread(&SimpleViewer::ReplayThread, this);
        }
        
        std::cout << "Started replay of " << m_events.events.size() << " events" << std::endl;
    }
}

void SimpleViewer::PauseReplay() {
    if (m_isReplaying) {
        m_isPaused = !m_isPaused;
        std::cout << (m_isPaused ? "Paused replay" : "Resumed replay") << std::endl;
    }
}

void SimpleViewer::StopReplay() {
    m_isReplaying = false;
    m_isPaused = false;
    m_currentEventIndex = 0;
    m_activePixels.clear();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void SimpleViewer::SetReplaySpeed(float speed) {
    m_replaySpeed = speed;
}

void SimpleViewer::SetScaleFactor(uint32_t scale) {
    m_scaleFactor = scale;
    
    // Resize window to fit new scale
    uint32_t windowWidth = m_canvasWidth * m_scaleFactor + 300;
    uint32_t windowHeight = m_canvasHeight * m_scaleFactor + 100;
    SetWindowPos(m_hwnd, nullptr, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER);
    
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

LRESULT CALLBACK SimpleViewer::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SimpleViewer* viewer = nullptr;
    
    if (uMsg == WM_CREATE) {
        CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        viewer = reinterpret_cast<SimpleViewer*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(viewer));
    } else {
        viewer = reinterpret_cast<SimpleViewer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (viewer) {
        return viewer->HandleMessage(uMsg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT SimpleViewer::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            m_hdc = hdc;
            OnPaint();
            EndPaint(m_hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            OnKeyPress(wParam);
            return 0;
        case WM_DESTROY:
            m_threadRunning = false;
            if (m_replayThread.joinable()) {
                m_replayThread.join();
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void SimpleViewer::OnPaint() {
    // Clear background
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    FillRect(m_hdc, &clientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    
    // Draw active pixels
    for (const auto& pixelPair : m_activePixels) {
        const Event& event = pixelPair.first;
        float alpha = pixelPair.second;
        
        int canvasX, canvasY;
        ScreenToCanvas(event.x, event.y, canvasX, canvasY);
        DrawPixel(canvasX, canvasY, event.polarity, alpha);
    }
    
    // Draw statistics
    DrawStatistics();
}

void SimpleViewer::OnKeyPress(WPARAM key) {
    switch (key) {
        case VK_SPACE:
            PauseReplay();
            break;
        case VK_ESCAPE:
            StopReplay();
            break;
        case '1':
            SetReplaySpeed(1.0f);
            break;
        case '2':
            SetReplaySpeed(2.0f);
            break;
        case '3':
            SetReplaySpeed(3.0f);
            break;
        case 'Z':
            SetScaleFactor(1);
            break;
        case 'X':
            SetScaleFactor(2);
            break;
        case 'C':
            SetScaleFactor(4);
            break;
    }
}

void SimpleViewer::ReplayThread() {
    while (m_threadRunning) {
        if (!m_isPaused && m_isReplaying) {
            uint64_t currentTime = HighResTimer::GetMicroseconds();
            uint64_t elapsedTime = currentTime - m_replayStartTime;
            
            // Process events that should be visible now
            while (m_currentEventIndex < m_events.events.size()) {
                const Event& event = m_events.events[m_currentEventIndex];
                uint64_t eventTime = event.timestamp - m_events.start_time;
                uint64_t adjustedEventTime = static_cast<uint64_t>(eventTime / m_replaySpeed);
                
                if (adjustedEventTime <= elapsedTime) {
                    AddPixel(event);
                    m_currentEventIndex++;
                    m_eventsProcessed++;
                } else {
                    break;
                }
            }
            
            // Check if replay is complete
            if (m_currentEventIndex >= m_events.events.size()) {
                m_isReplaying = false;
                m_threadRunning = false; // Ensure thread can be restarted
                break;
            }
            
            // Update active pixels and trigger redraw
            UpdateActivePixels();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        
        Sleep(16); // ~60 FPS update rate
    }
    m_threadRunning = false; // Defensive: ensure flag is false on exit
}

void SimpleViewer::UpdateActivePixels() {
    // Update fade timers and remove expired pixels
    for (auto it = m_activePixels.begin(); it != m_activePixels.end();) {
        it->second -= 0.016f; // 16ms at 60 FPS
        if (it->second <= 0.0f) {
            it = m_activePixels.erase(it);
        } else {
            ++it;
        }
    }
}

void SimpleViewer::AddPixel(const Event& event) {
    // Add pixel with full alpha
    m_activePixels.emplace_back(event, 0.5f); // 500ms fade duration
}

void SimpleViewer::RemoveExpiredPixels() {
    // Handled in UpdateActivePixels
}

void SimpleViewer::DrawPixel(int x, int y, int8_t polarity, float alpha) {
    // Clamp alpha to valid range
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    
    // Determine color based on polarity
    COLORREF color;
    if (polarity > 0) {
        color = RGB(0, static_cast<int>(255 * alpha), 0); // Green for positive
    } else {
        color = RGB(static_cast<int>(255 * alpha), 0, 0); // Red for negative
    }
    
    // Create brush for the pixel color
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(m_hdc, brush);
    
    // Create pen for the outline (same color as fill)
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(m_hdc, pen);
    
    // Draw scaled pixel rectangle
    int size = (2 > static_cast<int>(4 * m_scaleFactor)) ? 2 : static_cast<int>(4 * m_scaleFactor); // At least 2x2 pixels for visibility
    RECT rect = {
        static_cast<LONG>(x * m_scaleFactor),
        static_cast<LONG>(y * m_scaleFactor),
        static_cast<LONG>(x * m_scaleFactor + size),
        static_cast<LONG>(y * m_scaleFactor + size)
    };
    
    // Fill the rectangle
    FillRect(m_hdc, &rect, brush);
    
    // Restore old objects and clean up
    SelectObject(m_hdc, oldBrush);
    SelectObject(m_hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void SimpleViewer::DrawStatistics() {
    // Set text properties
    SetTextColor(m_hdc, RGB(255, 255, 255));
    SetBkMode(m_hdc, TRANSPARENT);
    
    // Calculate FPS
    static uint64_t lastTime = 0;
    static uint32_t frameCount = 0;
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    frameCount++;
    
    if (currentTime - lastTime >= 1000000) { // Every second
        m_currentFPS = frameCount * 1000000.0f / (currentTime - lastTime);
        frameCount = 0;
        lastTime = currentTime;
    }
    
    // Build statistics text
    std::ostringstream oss;
    oss << "Neuromorphic Event Viewer - Pixel Mode\n";
    oss << "=====================================\n\n";
    oss << "Canvas: " << m_canvasWidth << "x" << m_canvasHeight << "\n";
    oss << "Scale: " << m_scaleFactor << "x\n";
    oss << "Total Events: " << m_events.events.size() << "\n";
    oss << "Processed: " << m_eventsProcessed << "\n";
    oss << "Active Pixels: " << m_activePixels.size() << "\n";
    oss << "FPS: " << std::fixed << std::setprecision(1) << m_currentFPS << "\n";
    oss << "Speed: " << m_replaySpeed << "x\n";
    oss << "Status: " << (m_isPaused ? "PAUSED" : (m_isReplaying ? "PLAYING" : "STOPPED")) << "\n\n";
    oss << "Controls:\n";
    oss << "Space - Play/Pause\n";
    oss << "Escape - Stop\n";
    oss << "1/2/3 - Speed\n";
    oss << "Z/X/C - Zoom (1x/2x/4x)\n\n";
    oss << "Statistics:\n";
    oss << "Positive: " << m_stats.positive_events << "\n";
    oss << "Negative: " << m_stats.negative_events << "\n";
    oss << "Duration: " << (m_stats.duration_us / 1000000.0f) << "s\n";
    oss << "Events/sec: " << m_stats.events_per_second << "\n";
    
    // Convert to wide string for Windows API
    std::string text = oss.str();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wtext(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], size_needed);
    
    // Draw text
    RECT textRect = {
        static_cast<LONG>(m_canvasWidth * m_scaleFactor + 10),
        10,
        static_cast<LONG>(m_canvasWidth * m_scaleFactor + 290),
        static_cast<LONG>(m_canvasHeight * m_scaleFactor - 10)
    };
    
    // DrawTextW(m_hdc, wtext.c_str(), -1, &textRect, DT_LEFT | DT_TOP);
    TextOutW(m_hdc, textRect.left, textRect.top, wtext.c_str(), static_cast<int>(wtext.length()));
}

void SimpleViewer::ScreenToCanvas(uint16_t screenX, uint16_t screenY, int& canvasX, int& canvasY) {
    // Scale screen coordinates to fit within canvas
    if (m_events.width > 0 && m_events.height > 0) {
        float scaleX = static_cast<float>(m_canvasWidth) / static_cast<float>(m_events.width);
        float scaleY = static_cast<float>(m_canvasHeight) / static_cast<float>(m_events.height);
        
        canvasX = static_cast<int>(screenX * scaleX);
        canvasY = static_cast<int>(screenY * scaleY);
    } else {
        // Fallback: direct mapping
        canvasX = screenX;
        canvasY = screenY;
    }
}

} // namespace neuromorphic 