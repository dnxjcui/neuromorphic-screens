#include "imgui_event_viewer.h"
#include "../core/event_file_formats.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>

namespace neuromorphic {

ImGuiEventViewer::ImGuiEventViewer()
    : ImGuiViewerBase(), m_currentEventIndex(0), m_replayStartTime(0),
      m_isReplaying(false), m_isPaused(false), m_replaySpeed(0.5f),
      m_downsampleFactor(1), m_eventsProcessed(0),
      m_canvasWidth(800), m_canvasHeight(600),
      m_threadRunning(false), m_showStats(true), m_showControls(true),
      m_seekPosition(0.0f), m_useDimming(true), m_dimmingRate(1.0f), m_isLooping(false) {
}

ImGuiEventViewer::~ImGuiEventViewer() {
    StopReplay();
}


bool ImGuiEventViewer::LoadEvents(const std::string& filename) {
    std::cout << "Loading events from: " << filename << std::endl;
    
    if (!EventFileFormats::ReadEvents(m_events, filename)) {
        std::cerr << "Failed to load events from: " << filename << std::endl;
        return false;
    }
    
    // Sort events by timestamp
    std::sort(m_events.begin(), m_events.end(),
              [](const Event& a, const Event& b) { return a.timestamp < b.timestamp; });
    
    // CRITICAL: Normalize timestamps so first event starts at time 0
    if (m_events.size() > 0) {
        uint64_t firstTimestamp = m_events.begin()->timestamp;
        for (auto& event : m_events) {
            event.timestamp -= firstTimestamp;
        }
        m_events.start_time = 0; // Ensure start time is 0 for relative timing
        std::cout << "Normalized " << m_events.size() << " events with first timestamp at 0" << std::endl;
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
    std::thread([command = cmd.str()] {
        std::system(command.c_str());
    }).detach();
    
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

void ImGuiEventViewer::UpdateLogic() {
    // Update active dots
    UpdateActiveDots();
}

void ImGuiEventViewer::RenderMainContent() {
    // Calculate canvas area (most of the window)
    ImVec2 windowSize = ImGui::GetWindowSize();
    m_canvasWidth = static_cast<uint32_t>(windowSize.x * 0.75f); // 75% of window width
    m_canvasHeight = static_cast<uint32_t>(windowSize.y - 100); // Leave space for controls
    
    float canvasWidth = static_cast<float>(m_canvasWidth);
    float canvasHeight = static_cast<float>(m_canvasHeight);
    
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize(canvasWidth, canvasHeight);
    
    // Draw black background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                           IM_COL32(0, 0, 0, 255));
    
    // Draw border
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                     IM_COL32(100, 100, 100, 255));
    
    // Convert active dots to events for rendering
    std::vector<Event> currentEvents;
    {
        std::lock_guard<std::mutex> lock(m_activeDotsLock);
        for (const auto& dot : m_activeDots) {
            float alpha = dot.second / constants::DOT_FADE_DURATION;
            if (alpha > 0.0f) {
                currentEvents.push_back(dot.first);
            }
        }
    }
    
    // Use base class rendering with custom fade
    if (!currentEvents.empty() && m_events.width > 0 && m_events.height > 0) {
        uint64_t currentTime = HighResTimer::GetMicroseconds();
        RenderEventDotsWithFade(currentEvents, canvasWidth, canvasHeight, 
                               m_events.width, m_events.height, currentTime, 100.0f);
    }
    
    // Reserve space for canvas
    ImGui::Dummy(canvasSize);
}

void ImGuiEventViewer::RenderControlPanel() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.75f + 10, 50));
    ImGui::SetNextWindowSize(ImVec2(300, 280));
    
    if (ImGui::Begin("Controls", &m_showControls, ImGuiWindowFlags_NoResize)) {
        // Render statistics first
        RenderStatistics(m_stats.total_events, m_stats.events_per_second, 
                        static_cast<uint32_t>(m_activeDots.size()));
        
        ImGui::Separator();
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
        
        ImGui::Separator();
        
        // Show additional stats panel toggle
        if (ImGui::Button("Toggle Stats Panel")) {
            m_showStats = !m_showStats;
        }
    }
    ImGui::End();
    
    // Render separate stats panel if enabled
    if (m_showStats) {
        RenderStatsPanel();
    }
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




} // namespace neuromorphic