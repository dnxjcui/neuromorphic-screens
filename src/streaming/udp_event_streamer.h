#pragma once

#include "../core/event_types.h"
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    typedef struct sockaddr_in sockaddr_in_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    typedef int socket_t;
    typedef struct sockaddr_in sockaddr_in_t;
#endif

namespace neuromorphic {

/**
 * DVS Event structure compatible with event_stream library
 * Matches event_stream.dvs_dtype: ('t', '<u8'), ('x', '<u2'), ('y', '<u2'), ('on', '?')
 */
#pragma pack(push, 1)
struct DVSEvent : public Event {
    // Removed redundant 'on' field - polarity from base Event class is sufficient
    
    DVSEvent() : Event() {}
    DVSEvent(uint64_t t, uint16_t px, uint16_t py, bool pol) 
        : Event(t, px, py, pol ? 1 : 0) {}
    DVSEvent(const Event& event) 
        : Event(event) {}
};
#pragma pack(pop)

/**
 * UDP Event Streamer for neuromorphic data
 * Streams DVS events over UDP in format compatible with event_stream library
 * Can operate in simulation mode or with real event data source
 */
class UdpEventStreamer {
public:
    UdpEventStreamer();
    ~UdpEventStreamer();
    
    /**
     * Initialize the streamer with target parameters optimized for high throughput
     * @param targetIP Target IP address (default: "127.0.0.1")
     * @param targetPort Target UDP port (default: 9999)
     * @param eventsPerBatch Number of events per UDP packet (default: 1500 for ~20KB packets)
     * @param eventWidth Event space width (default: 1920)
     * @param eventHeight Event space height (default: 1080)
     * @param targetThroughputMBps Target throughput in MB/s (default: 20)
     * @param maxDropRatio Maximum ratio of events to drop for real-time performance (default: 0.1 = 10%)
     */
    bool Initialize(const std::string& targetIP = "127.0.0.1", 
                   uint16_t targetPort = 9999,
                   uint32_t eventsPerBatch = 1500,
                   uint16_t eventWidth = 1920, 
                   uint16_t eventHeight = 1080,
                   float targetThroughputMBps = 20.0f,
                   float maxDropRatio = 0.1f);
    
    /**
     * Set a custom event source function
     * If not set, the streamer will generate simulated events
     * @param eventSource Function that returns a vector of DVSEvent
     */
    void SetEventSource(std::function<std::vector<DVSEvent>()> eventSource);
    
    /**
     * Start the streaming thread
     */
    void Start();
    
    /**
     * Stop the streaming thread
     */
    void Stop();
    
    /**
     * Check if the streamer is currently running
     */
    bool IsRunning() const { return m_isRunning.load(); }
    
    /**
     * Get current configuration and performance metrics
     */
    std::string GetTargetIP() const { return m_targetIP; }
    uint16_t GetTargetPort() const { return m_targetPort; }
    uint32_t GetEventsPerBatch() const { return m_eventsPerBatch; }
    uint16_t GetEventWidth() const { return m_eventWidth; }
    uint16_t GetEventHeight() const { return m_eventHeight; }
    float GetCurrentThroughputMBps() const { return m_currentThroughputMBps; }
    float GetDropRatio() const { return m_totalEventsDropped > 0 ? (float)m_totalEventsDropped / (m_totalEventsSent + m_totalEventsDropped) : 0.0f; }
    uint64_t GetTotalEventsSent() const { return m_totalEventsSent; }
    uint64_t GetTotalEventsDropped() const { return m_totalEventsDropped; }

private:
    // Network configuration
    std::string m_targetIP;
    uint16_t m_targetPort;
    socket_t m_socket;
    sockaddr_in m_targetAddr;
    
    // Streaming configuration
    uint32_t m_eventsPerBatch;
    uint16_t m_eventWidth;
    uint16_t m_eventHeight;
    float m_targetThroughputMBps;
    float m_maxDropRatio;
    
    // Performance tracking
    mutable std::atomic<float> m_currentThroughputMBps;
    mutable std::atomic<uint64_t> m_totalEventsSent;
    mutable std::atomic<uint64_t> m_totalEventsDropped;
    mutable std::atomic<uint64_t> m_totalBytesSent;
    
    // Event source
    std::function<std::vector<DVSEvent>()> m_eventSource;
    
    // Threading
    std::atomic<bool> m_isRunning;
    std::thread m_streamingThread;
    
    /**
     * Create and configure the UDP socket
     */
    bool CreateSocket();
    
    
    /**
     * Main streaming thread function
     */
    void StreamingThreadFunction();
};

} // namespace neuromorphic