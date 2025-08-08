#include "udp_event_streamer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <random>
#include <fstream>
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
    , m_eventsPerBatch(1500)
    , m_eventWidth(1920)
    , m_eventHeight(1080)
    , m_targetThroughputMBps(20.0f)
    , m_maxDropRatio(0.1f)
    , m_currentThroughputMBps(0.0f)
    , m_totalEventsSent(0)
    , m_totalEventsDropped(0)
    , m_totalBytesSent(0) {
    
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
                                 uint32_t eventsPerBatch, uint16_t eventWidth, uint16_t eventHeight,
                                 float targetThroughputMBps, float maxDropRatio) {
    m_targetIP = targetIP;
    m_targetPort = targetPort;
    m_eventsPerBatch = eventsPerBatch;
    m_eventWidth = eventWidth;
    m_eventHeight = eventHeight;
    m_targetThroughputMBps = targetThroughputMBps;
    m_maxDropRatio = maxDropRatio;
    
    // Reset performance counters
    m_currentThroughputMBps.store(0.0f);
    m_totalEventsSent.store(0);
    m_totalEventsDropped.store(0);
    m_totalBytesSent.store(0);
    
    return CreateSocket();
}
bool UdpEventStreamer::CreateSocket() {
#ifdef _WIN32
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    // Set socket buffer size to increase throughput
    // int bufferSize = 1024 * 1024; // 1MB buffer
    int bufferSize = 1024 * 1024 * 20; // 20MB buffer
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufferSize, sizeof(bufferSize)) == SOCKET_ERROR) {
        std::cerr << "Failed to set send buffer size: " << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "Successfully set send buffer size to " << bufferSize << std::endl;
    }
#else
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        perror("Socket creation failed");
        return false;
    }
    
    // Set socket buffer size to increase throughput
    int bufferSize = 1024 * 1024; // 1MB buffer
    if (setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize)) < 0) {
        perror("Failed to set send buffer size");
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

void UdpEventStreamer::StreamingThreadFunction() {
    // Prepare packet buffer - optimized for high throughput
    size_t maxPacketSize = sizeof(uint64_t) + m_eventsPerBatch * sizeof(DVSEvent);
    std::vector<char> packetBuffer(maxPacketSize);
    
    // Performance tracking
    uint64_t packetsSent = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastStatsTime = startTime;
    auto lastThroughputTime = startTime;
    uint64_t lastBytesSent = 0;
    
    std::cout << "Starting high-throughput event streaming loop..." << std::endl;
    std::cout << "Target throughput: " << m_targetThroughputMBps << " MB/s" << std::endl;
    std::cout << "Max drop ratio: " << (m_maxDropRatio * 100.0f) << "%" << std::endl;
    
    while (m_isRunning.load()) {
        std::vector<DVSEvent> events;
        
        // Get events from real data source
        if (m_eventSource) {
            events = m_eventSource();
            if (events.empty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                continue;
            }
            
            // Debug output for event generation
            static int debugCounter = 0;
            if (++debugCounter % 100 == 0) {  // Print every 100th call
                std::cout << "UDP: Generated " << events.size() << " events" << std::endl;
            }
        } else {
            std::cerr << "No event source configured!" << std::endl;
            break;
        }
        
        // Implement adaptive event dropping for real-time performance
        size_t originalEventCount = events.size();
        size_t eventsToSend = events.size();
        
        // Check current throughput and drop events if necessary
        auto now = std::chrono::high_resolution_clock::now();
        auto throughputElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastThroughputTime);
        
        if (throughputElapsed.count() >= 100) { // Update throughput every 100ms
            uint64_t currentBytes = m_totalBytesSent.load();
            uint64_t bytesDelta = currentBytes - lastBytesSent;
            float currentThroughput = (bytesDelta * 10.0f) / (1024.0f * 1024.0f); // MB/s (100ms * 10 = 1s)
            
            m_currentThroughputMBps.store(currentThroughput);
            
            // If we're exceeding target throughput, drop some events
            if (currentThroughput > m_targetThroughputMBps * 1.1f) { // 10% tolerance
                float dropRatio = (std::min)(m_maxDropRatio, (currentThroughput - m_targetThroughputMBps) / m_targetThroughputMBps);
                eventsToSend = static_cast<size_t>(events.size() * (1.0f - dropRatio));
                eventsToSend = (std::max)(eventsToSend, static_cast<size_t>(1)); // Always send at least 1 event
            }
            
            lastThroughputTime = now;
            lastBytesSent = currentBytes;
        }
        
        // Drop events if necessary (keep most recent events)
        if (eventsToSend < originalEventCount) {
            events.resize(eventsToSend);
            m_totalEventsDropped.fetch_add(originalEventCount - eventsToSend);
        }
        
        // Split large event batches into multiple packets if needed
        size_t eventIndex = 0;
        while (eventIndex < events.size() && m_isRunning.load()) {
            size_t eventsInThisPacket = (std::min)(static_cast<size_t>(m_eventsPerBatch), events.size() - eventIndex);
            
            // Prepare packet: timestamp + event data
            uint64_t packetTimestamp = events[eventIndex].timestamp;
            memcpy(packetBuffer.data(), &packetTimestamp, sizeof(uint64_t));
            memcpy(packetBuffer.data() + sizeof(uint64_t), 
                   events.data() + eventIndex, 
                   eventsInThisPacket * sizeof(DVSEvent));
            
            size_t actualPacketSize = sizeof(uint64_t) + eventsInThisPacket * sizeof(DVSEvent);
            
            // Fast send with minimal retry logic for real-time performance
            bool sendSuccess = false;
            int retryCount = 0;
            const int maxRetries = 2; // Reduced retries for real-time
            
            while (!sendSuccess && retryCount < maxRetries && m_isRunning.load()) {
#ifdef _WIN32
                int sentBytes = sendto(m_socket, packetBuffer.data(), static_cast<int>(actualPacketSize), 
                                      0, reinterpret_cast<const sockaddr*>(&m_targetAddr), sizeof(m_targetAddr));
                if (sentBytes == static_cast<int>(actualPacketSize)) {
                    sendSuccess = true;
                } else {
                    retryCount++;
                    if (retryCount < maxRetries) {
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                    }
                }
#else
                ssize_t sentBytes = sendto(m_socket, packetBuffer.data(), actualPacketSize, 
                                          0, reinterpret_cast<const sockaddr*>(&m_targetAddr), sizeof(m_targetAddr));
                if (sentBytes == static_cast<ssize_t>(actualPacketSize)) {
                    sendSuccess = true;
                } else {
                    retryCount++;
                    if (retryCount < maxRetries) {
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                    }
                }
#endif
            }
            
            if (sendSuccess) {
                packetsSent++;
                m_totalEventsSent.fetch_add(eventsInThisPacket);
                m_totalBytesSent.fetch_add(actualPacketSize);
                
                // Debug output for successful sends
                static int sendCounter = 0;
                sendCounter++;
                if (sendCounter % 50 == 0) {  // Print every 50th successful send
                    std::cout << "UDP: Sent packet with " << eventsInThisPacket << " events (" << actualPacketSize << " bytes)" << std::endl;
                }            
                if (sendCounter == 100) {
                    std::ofstream dump("udp_packet_100.bin", std::ios::binary);
                    dump.write(packetBuffer.data(), actualPacketSize);
                    dump.close();
                    std::cout << "UDP: Debug packet dumped to udp_packet_100.bin" << std::endl;
                }
    
            } else {
                std::cout << "UDP: Failed to send packet" << std::endl;
                // Drop failed packet events for real-time performance
                m_totalEventsDropped.fetch_add(eventsInThisPacket);
            }
            
            eventIndex += eventsInThisPacket;
        }
        
        // Print performance statistics every 5 seconds (reduced frequency)
        auto statsElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatsTime);
        if (statsElapsed.count() >= 5000) {
            uint64_t totalSent = m_totalEventsSent.load();
            uint64_t totalDropped = m_totalEventsDropped.load();
            float currentThroughput = m_currentThroughputMBps.load();
            float dropRatio = GetDropRatio();
            
            auto totalElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            double avgEventsPerSec = totalElapsed > 0 ? static_cast<double>(totalSent) / totalElapsed : 0.0;
            
            std::cout << "=== UDP Streaming Performance ===" << std::endl;
            std::cout << "Throughput: " << currentThroughput << " MB/s (target: " << m_targetThroughputMBps << " MB/s)" << std::endl;
            std::cout << "Events/sec: " << static_cast<uint64_t>(avgEventsPerSec) << " | Packets sent: " << packetsSent << std::endl;
            std::cout << "Events sent: " << totalSent << " | Dropped: " << totalDropped << " (" << (dropRatio * 100.0f) << "%)" << std::endl;
            
            lastStatsTime = now;
        }
        
        // Minimal sleep for maximum throughput
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
    // Final statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    uint64_t totalSent = m_totalEventsSent.load();
    uint64_t totalDropped = m_totalEventsDropped.load();
    uint64_t totalBytes = m_totalBytesSent.load();
    
    std::cout << "\n=== Final UDP Streaming Results ===" << std::endl;
    std::cout << "Duration: " << totalDuration << " seconds" << std::endl;
    std::cout << "Packets sent: " << packetsSent << std::endl;
    std::cout << "Events sent: " << totalSent << std::endl;
    std::cout << "Events dropped: " << totalDropped << " (" << (GetDropRatio() * 100.0f) << "%)" << std::endl;
    std::cout << "Total data sent: " << (totalBytes / (1024.0f * 1024.0f)) << " MB" << std::endl;
    if (totalDuration > 0) {
        std::cout << "Average throughput: " << (totalBytes / (1024.0f * 1024.0f * totalDuration)) << " MB/s" << std::endl;
        std::cout << "Average events/sec: " << (totalSent / totalDuration) << std::endl;
    }
}
} // namespace neuromorphic