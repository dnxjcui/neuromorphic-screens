#include "udp_event_streamer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <random>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

namespace neuromorphic {

UdpEventStreamer::UdpEventStreamer() 
    : m_isRunning(false)
    , m_socket(-1)
    , m_targetIP("127.0.0.1")
    , m_targetPort(9999)
    , m_eventsPerBatch(100)
    , m_eventWidth(128)
    , m_eventHeight(128)
    , m_simulationMode(true)
    , m_currentTimeUs(0) {
    
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
    }
#endif
}

UdpEventStreamer::~UdpEventStreamer() {
    Stop();
    
#ifdef _WIN32
    WSACleanup();
#endif
}

bool UdpEventStreamer::Initialize(const std::string& targetIP, uint16_t targetPort, 
                                 uint32_t eventsPerBatch, uint16_t eventWidth, uint16_t eventHeight) {
    m_targetIP = targetIP;
    m_targetPort = targetPort;
    m_eventsPerBatch = eventsPerBatch;
    m_eventWidth = eventWidth;
    m_eventHeight = eventHeight;
    
    return CreateSocket();
}

bool UdpEventStreamer::CreateSocket() {
#ifdef _WIN32
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        return false;
    }
#else
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        perror("Socket creation failed");
        return false;
    }
#endif

    // Setup target address
    memset(&m_targetAddr, 0, sizeof(m_targetAddr));
    m_targetAddr.sin_family = AF_INET;
    m_targetAddr.sin_port = htons(m_targetPort);
    
#ifdef _WIN32
    if (inet_pton(AF_INET, m_targetIP.c_str(), &m_targetAddr.sin_addr) <= 0) {
        std::cerr << "Invalid target IP address: " << m_targetIP << std::endl;
        closesocket(m_socket);
        return false;
    }
#else
    if (inet_pton(AF_INET, m_targetIP.c_str(), &m_targetAddr.sin_addr) <= 0) {
        perror("Invalid target IP address");
        close(m_socket);
        return false;
    }
#endif

    return true;
}

void UdpEventStreamer::SetEventSource(std::function<std::vector<DVSEvent>()> eventSource) {
    m_eventSource = eventSource;
    m_simulationMode = false;
}

void UdpEventStreamer::Start() {
    if (m_isRunning.load()) {
        std::cout << "Event streamer already running" << std::endl;
        return;
    }
    
    if (m_socket == -1 && !CreateSocket()) {
        std::cerr << "Failed to create socket for streaming" << std::endl;
        return;
    }
    
    m_isRunning.store(true);
    m_streamingThread = std::thread(&UdpEventStreamer::StreamingThreadFunction, this);
    
    std::cout << "UDP Event Streamer started, streaming to " << m_targetIP 
              << ":" << m_targetPort << std::endl;
    std::cout << "Events per batch: " << m_eventsPerBatch 
              << ", Resolution: " << m_eventWidth << "x" << m_eventHeight << std::endl;
}

void UdpEventStreamer::Stop() {
    if (!m_isRunning.load()) return;
    
    m_isRunning.store(false);
    if (m_streamingThread.joinable()) {
        m_streamingThread.join();
    }
    
    if (m_socket != -1) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        close(m_socket);
#endif
        m_socket = -1;
    }
    
    std::cout << "UDP Event Streamer stopped" << std::endl;
}

std::vector<DVSEvent> UdpEventStreamer::GenerateSimulatedEvents() {
    std::vector<DVSEvent> events;
    events.reserve(m_eventsPerBatch);
    
    // Initialize random number generator (thread-local)
    static thread_local std::random_device rd;
    static thread_local std::mt19937 rng(rd());
    std::uniform_int_distribution<uint16_t> distX(0, m_eventWidth - 1);
    std::uniform_int_distribution<uint16_t> distY(0, m_eventHeight - 1);
    std::uniform_int_distribution<int> distPolarity(0, 1);
    
    for (uint32_t i = 0; i < m_eventsPerBatch; ++i) {
        DVSEvent event;
        event.timestamp = m_currentTimeUs;
        event.x = distX(rng);
        event.y = distY(rng);
        event.polarity = (distPolarity(rng) == 1);
        
        events.push_back(event);
        m_currentTimeUs += 1; // Simulate 1 microsecond per event
    }
    
    return events;
}

void UdpEventStreamer::StreamingThreadFunction() {
    // Prepare packet buffer
    // Format: uint64_t timestamp + N * DVSEvent
    size_t packetSize = sizeof(uint64_t) + m_eventsPerBatch * sizeof(DVSEvent);
    std::vector<char> packetBuffer(packetSize);
    
    uint64_t packetssent = 0;
    uint64_t totalEventsSent = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Starting event streaming loop..." << std::endl;
    
    while (m_isRunning.load()) {
        std::vector<DVSEvent> events;
        
        // Get events from source (simulation or real data)
        if (m_simulationMode) {
            events = GenerateSimulatedEvents();
        } else if (m_eventSource) {
            events = m_eventSource();
            if (events.empty()) {
                // If no events available, wait a bit and continue
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
        } else {
            std::cerr << "No event source configured!" << std::endl;
            break;
        }
        
        if (events.empty()) continue;
        
        // Prepare packet: timestamp + event data
        // Use timestamp of first event as packet timestamp
        uint64_t packetTimestamp = events[0].timestamp;
        memcpy(packetBuffer.data(), &packetTimestamp, sizeof(uint64_t));
        memcpy(packetBuffer.data() + sizeof(uint64_t), events.data(), 
               events.size() * sizeof(DVSEvent));
        
        // Calculate actual packet size (might be less than max if fewer events)
        size_t actualPacketSize = sizeof(uint64_t) + events.size() * sizeof(DVSEvent);
        
        // Send packet
#ifdef _WIN32
        int sentBytes = sendto(m_socket, packetBuffer.data(), static_cast<int>(actualPacketSize), 
                              0, reinterpret_cast<const sockaddr*>(&m_targetAddr), sizeof(m_targetAddr));
        if (sentBytes == SOCKET_ERROR) {
            std::cerr << "sendto failed: " << WSAGetLastError() << std::endl;
        }
#else
        ssize_t sentBytes = sendto(m_socket, packetBuffer.data(), actualPacketSize, 
                                  0, reinterpret_cast<const sockaddr*>(&m_targetAddr), sizeof(m_targetAddr));
        if (sentBytes < 0) {
            perror("sendto failed");
        }
#endif
        else {
            packetssent++;
            totalEventsSent += events.size();
            
            // Debug output every 100 packets
            if (packetssent % 100 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
                if (duration > 0) {
                    double eventsPerSecond = static_cast<double>(totalEventsSent) / duration;
                    std::cout << "Sent " << packetssent << " packets, " << totalEventsSent 
                             << " events (" << eventsPerSecond << " events/sec)" << std::endl;
                }
            }
        }
        
        // Simulate real-time by waiting for the duration represented by the events
        // This assumes 1 event per microsecond simulation rate
        if (m_simulationMode) {
            std::this_thread::sleep_for(std::chrono::microseconds(events.size()));
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    
    std::cout << "Streaming completed:" << std::endl;
    std::cout << "  Total packets sent: " << packetssent << std::endl;
    std::cout << "  Total events sent: " << totalEventsSent << std::endl;
    std::cout << "  Duration: " << totalDuration << " seconds" << std::endl;
    if (totalDuration > 0) {
        std::cout << "  Average events/sec: " << (totalEventsSent / totalDuration) << std::endl;
    }
}

} // namespace neuromorphic