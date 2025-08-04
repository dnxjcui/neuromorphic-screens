#pragma once

#include "../core/event_types.h"
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    typedef int socket_t;
#endif

namespace neuromorphic {

/**
 * DVS Event structure compatible with event_stream library
 * Matches event_stream.dvs_dtype: ('t', '<u8'), ('x', '<u2'), ('y', '<u2'), ('on', '?')
 */
#pragma pack(push, 1)
struct DVSEvent {
    uint64_t timestamp;  // Microseconds
    uint16_t x;          // X coordinate
    uint16_t y;          // Y coordinate
    bool polarity;       // True for ON/positive, False for OFF/negative
    
    DVSEvent() : timestamp(0), x(0), y(0), polarity(false) {}
    DVSEvent(uint64_t t, uint16_t px, uint16_t py, bool pol) 
        : timestamp(t), x(px), y(py), polarity(pol) {}
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
     * Initialize the streamer with target parameters
     * @param targetIP Target IP address (default: "127.0.0.1")
     * @param targetPort Target UDP port (default: 9999)
     * @param eventsPerBatch Number of events per UDP packet (default: 100)
     * @param eventWidth Event space width for simulation (default: 128)
     * @param eventHeight Event space height for simulation (default: 128)
     */
    bool Initialize(const std::string& targetIP = "127.0.0.1", 
                   uint16_t targetPort = 9999,
                   uint32_t eventsPerBatch = 100,
                   uint16_t eventWidth = 128, 
                   uint16_t eventHeight = 128);
    
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
     * Get current configuration
     */
    std::string GetTargetIP() const { return m_targetIP; }
    uint16_t GetTargetPort() const { return m_targetPort; }
    uint32_t GetEventsPerBatch() const { return m_eventsPerBatch; }
    uint16_t GetEventWidth() const { return m_eventWidth; }
    uint16_t GetEventHeight() const { return m_eventHeight; }

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
    
    // Event source
    std::function<std::vector<DVSEvent>()> m_eventSource;
    bool m_simulationMode;
    uint64_t m_currentTimeUs;  // For simulation
    
    // Threading
    std::atomic<bool> m_isRunning;
    std::thread m_streamingThread;
    
    /**
     * Create and configure the UDP socket
     */
    bool CreateSocket();
    
    /**
     * Generate simulated DVS events for testing
     */
    std::vector<DVSEvent> GenerateSimulatedEvents();
    
    /**
     * Main streaming thread function
     */
    void StreamingThreadFunction();
};

} // namespace neuromorphic