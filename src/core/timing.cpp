#include "timing.h"
#include <chrono>
#include <thread>
#include <iostream>

namespace neuromorphic {

// Static member initialization
std::chrono::high_resolution_clock::time_point HighResTimer::s_epoch;

void HighResTimer::Initialize() {
    s_epoch = std::chrono::high_resolution_clock::now();
}

uint64_t HighResTimer::GetMicroseconds() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - s_epoch);
    return duration.count();
}

void HighResTimer::SleepMilliseconds(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void HighResTimer::SleepMicroseconds(uint64_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

// FrameRateLimiter implementation
FrameRateLimiter::FrameRateLimiter(float targetFPS) 
    : m_lastFrameTime(0), m_frameCount(0), m_startTime(0) {
    m_targetFrameTime = static_cast<uint64_t>(1000000.0f / targetFPS);
    m_startTime = HighResTimer::GetMicroseconds();
}

void FrameRateLimiter::WaitForNextFrame() {
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    uint64_t elapsed = currentTime - m_lastFrameTime;
    
    if (elapsed < m_targetFrameTime) {
        HighResTimer::SleepMicroseconds(m_targetFrameTime - elapsed);
    }
    
    m_lastFrameTime = HighResTimer::GetMicroseconds();
    m_frameCount++;
}

float FrameRateLimiter::GetCurrentFPS() const {
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    uint64_t elapsed = currentTime - m_startTime;
    
    if (elapsed == 0) return 0.0f;
    
    return static_cast<float>(m_frameCount) * 1000000.0f / static_cast<float>(elapsed);
}

void FrameRateLimiter::Reset() {
    m_frameCount = 0;
    m_startTime = HighResTimer::GetMicroseconds();
    m_lastFrameTime = m_startTime;
}

// RecordingTimer implementation
RecordingTimer::RecordingTimer() 
    : m_startTime(0), m_duration(0), m_isRecording(false) {
}

void RecordingTimer::Start(uint64_t durationSeconds) {
    m_startTime = HighResTimer::GetMicroseconds();
    m_duration = durationSeconds * 1000000ULL;
    m_isRecording = true;
}

bool RecordingTimer::ShouldContinue() const {
    if (!m_isRecording) return false;
    
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    return (currentTime - m_startTime) < m_duration;
}

float RecordingTimer::GetElapsedSeconds() const {
    if (!m_isRecording) return 0.0f;
    
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    return static_cast<float>(currentTime - m_startTime) / 1000000.0f;
}

float RecordingTimer::GetRemainingSeconds() const {
    if (!m_isRecording) return 0.0f;
    
    uint64_t currentTime = HighResTimer::GetMicroseconds();
    uint64_t elapsed = currentTime - m_startTime;
    
    if (elapsed >= m_duration) return 0.0f;
    
    return static_cast<float>(m_duration - elapsed) / 1000000.0f;
}

void RecordingTimer::Stop() {
    m_isRecording = false;
}

} // namespace neuromorphic 