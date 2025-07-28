#pragma once

#include <cstdint>
#include <chrono>
#include <thread>

namespace neuromorphic {

/**
 * High-resolution timing utilities for microsecond precision
 */
class HighResTimer {
private:
    static std::chrono::high_resolution_clock::time_point s_epoch;

public:
    /**
     * Get current time in microseconds since epoch
     */
    static uint64_t GetMicroseconds();
    
    /**
     * Sleep for specified milliseconds
     */
    static void SleepMilliseconds(uint32_t ms);
    
    /**
     * Sleep for specified microseconds
     */
    static void SleepMicroseconds(uint64_t us);
    
    /**
     * Initialize the timer epoch
     */
    static void Initialize();
};

/**
 * Frame rate limiter for consistent timing
 */
class FrameRateLimiter {
private:
    uint64_t m_lastFrameTime;
    uint64_t m_targetFrameTime;
    uint64_t m_frameCount;
    uint64_t m_startTime;

public:
    FrameRateLimiter(float targetFPS = 60.0f);
    
    /**
     * Wait until next frame should be rendered
     */
    void WaitForNextFrame();
    
    /**
     * Get current FPS
     */
    float GetCurrentFPS() const;
    
    /**
     * Reset frame counter
     */
    void Reset();
};

/**
 * Recording timer for burst captures
 */
class RecordingTimer {
private:
    uint64_t m_startTime;
    uint64_t m_duration;
    bool m_isRecording;

public:
    RecordingTimer();
    
    /**
     * Start recording for specified duration
     */
    void Start(uint64_t durationSeconds);
    
    /**
     * Check if recording should continue
     */
    bool ShouldContinue() const;
    
    /**
     * Get elapsed time in seconds
     */
    float GetElapsedSeconds() const;
    
    /**
     * Get remaining time in seconds
     */
    float GetRemainingSeconds() const;
    
    /**
     * Stop recording
     */
    void Stop();
    
    /**
     * Check if currently recording
     */
    bool IsRecording() const { return m_isRecording; }
};

} // namespace neuromorphic 