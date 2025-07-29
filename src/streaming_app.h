#pragma once

#include "core/event_types.h"
#include "core/timing.h"
#include "core/event_file_formats.h"
#include "capture/screen_capture.h"
#include <atomic>
#include <thread>
#include <string>

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
    
public:
    StreamingApp();
    
    void setSaveOptions(const std::string& filename, EventFileFormat format);
    bool initialize();
    void startStreaming();
    void stopStreaming();
    
    const EventStream& getEventStream() const { return m_eventStream; }
    bool isRunning() const { return m_isRunning.load(); }
    
private:
    void captureLoop();
};

} // namespace neuromorphic