#include "streaming_app.h"
#include <iostream>

namespace neuromorphic {

StreamingApp::StreamingApp() : m_isRunning(false), m_saveFormat(EventFileFormat::BINARY_AEDAT) {}

void StreamingApp::setSaveOptions(const std::string& filename, EventFileFormat format) {
    m_saveFilename = filename;
    m_saveFormat = format;
}

bool StreamingApp::initialize() {
    if (!m_capture.Initialize()) {
        std::cerr << "Failed to initialize screen capture" << std::endl;
        return false;
    }
    
    m_eventStream.width = m_capture.GetWidth();
    m_eventStream.height = m_capture.GetHeight();
    m_eventStream.start_time = HighResTimer::GetMicroseconds();
    
    return true;
}

void StreamingApp::startStreaming() {
    if (m_isRunning.load()) {
        std::cout << "Streaming already active" << std::endl;
        return;
    }
    
    m_isRunning.store(true);
    m_captureThread = std::thread(&StreamingApp::captureLoop, this);
    std::cout << "Started real-time event streaming" << std::endl;
}

void StreamingApp::stopStreaming() {
    if (!m_isRunning.load()) return;
    
    m_isRunning.store(false);
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
    
    m_capture.StopCapture();
    
    // Save events if filename specified
    if (!m_saveFilename.empty() && !m_eventStream.events.empty()) {
        if (EventFileFormats::WriteEvents(m_eventStream, m_saveFilename, m_saveFormat)) {
            std::cout << "Events saved to: " << m_saveFilename << std::endl;
            std::cout << "Total events captured: " << m_eventStream.events.size() << std::endl;
        } else {
            std::cerr << "Failed to save events" << std::endl;
        }
    }
    
    std::cout << "Stopped streaming" << std::endl;
}

void StreamingApp::captureLoop() {
    if (!m_capture.StartCapture()) {
        std::cerr << "Failed to start screen capture" << std::endl;
        m_isRunning.store(false);
        return;
    }
    
    FrameRateLimiter limiter(60.0f);
    
    while (m_isRunning.load()) {
        uint64_t currentTime = HighResTimer::GetMicroseconds();
        
        // Capture frame and generate events
        if (m_capture.CaptureFrame(m_eventStream, currentTime)) {
            // Events are automatically added to m_eventStream
        }
        
        limiter.WaitForNextFrame();
    }
}

} // namespace neuromorphic