#pragma once

#include "core/event_types.h"
#include "core/timing.h"
#include "core/event_file_formats.h"
#include "capture/screen_capture.h"
#include <atomic>
#include <thread>
#include <string>
#include <algorithm>

namespace neuromorphic {

/**
 * Streaming application class for real-time event capture
 */
class StreamingApp {
private:
    std::atomic<bool> m_isRunning;
    std::thread m_captureThread;
    ScreenCapture m_capture;
    EventStream m_eventStream;
    std::string m_saveFilename;
    EventFileFormat m_saveFormat;
    
    // Capture parameters
    float m_threshold;
    uint32_t m_stride;
    size_t m_maxEvents;
    
public:
    StreamingApp();
    
    void setSaveOptions(const std::string& filename, EventFileFormat format);
    bool initialize();
    void startStreaming();
    void stopStreaming();
    
    const EventStream& getEventStream() const { return m_eventStream; }
    bool isRunning() const { return m_isRunning.load(); }
    
    // Parameter control
    void setThreshold(float threshold) { m_threshold = (std::max)(0.0f, (std::min)(100.0f, threshold)); }
    void setStride(uint32_t stride) { m_stride = (std::max)(1u, (std::min)(30u, stride)); }
    void setMaxEvents(size_t maxEvents) { m_maxEvents = (std::max)(static_cast<size_t>(1000), (std::min)(static_cast<size_t>(100000), maxEvents)); m_eventStream.max_events = m_maxEvents; }
    float getThreshold() const { return m_threshold; }
    uint32_t getStride() const { return m_stride; }
    size_t getMaxEvents() const { return m_maxEvents; }
    
private:
    void captureLoop();
};

} // namespace neuromorphic