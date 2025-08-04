#include "streaming/udp_event_streamer.h"
#include "streaming_app.h"
#include "core/timing.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <signal.h>

using namespace neuromorphic;

// Global streamer for signal handling
UdpEventStreamer* g_streamer = nullptr;

void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping streamer..." << std::endl;
    if (g_streamer) {
        g_streamer->Stop();
    }
}

void PrintUsage(const char* programName) {
    std::cout << "UDP Event Streamer - Streams neuromorphic events over UDP" << std::endl;
    std::cout << "Usage: " << programName << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --ip <address>        Target IP address (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port <port>         Target UDP port (default: 9999)" << std::endl;
    std::cout << "  --batch <size>        Events per UDP packet (default: 100)" << std::endl;
    std::cout << "  --width <pixels>      Event space width (default: 128)" << std::endl;
    std::cout << "  --height <pixels>     Event space height (default: 128)" << std::endl;
    std::cout << "  --real-events         Use real screen capture events (default: simulation)" << std::endl;
    std::cout << "  --duration <seconds>  Run for specified duration (default: unlimited)" << std::endl;
    std::cout << "  --help                Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << "                                    # Stream simulated events to localhost:9999" << std::endl;
    std::cout << "  " << programName << " --ip 192.168.1.100 --port 8888    # Stream to remote host" << std::endl;
    std::cout << "  " << programName << " --real-events --batch 50           # Stream real screen events" << std::endl;
    std::cout << "  " << programName << " --duration 30                      # Run for 30 seconds" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default configuration
    std::string targetIP = "127.0.0.1";
    uint16_t targetPort = 9999;
    uint32_t eventsPerBatch = 100;
    uint16_t eventWidth = 128;
    uint16_t eventHeight = 128;
    bool useRealEvents = false;
    int durationSeconds = 0; // 0 = unlimited
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (arg == "--ip" && i + 1 < argc) {
            targetIP = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc) {
            targetPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--batch" && i + 1 < argc) {
            eventsPerBatch = static_cast<uint32_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--width" && i + 1 < argc) {
            eventWidth = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--height" && i + 1 < argc) {
            eventHeight = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--real-events") {
            useRealEvents = true;
        }
        else if (arg == "--duration" && i + 1 < argc) {
            durationSeconds = std::stoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "UDP Event Streamer Configuration:" << std::endl;
    std::cout << "  Target: " << targetIP << ":" << targetPort << std::endl;
    std::cout << "  Events per batch: " << eventsPerBatch << std::endl;
    std::cout << "  Event space: " << eventWidth << "x" << eventHeight << std::endl;
    std::cout << "  Mode: " << (useRealEvents ? "Real screen events" : "Simulated events") << std::endl;
    if (durationSeconds > 0) {
        std::cout << "  Duration: " << durationSeconds << " seconds" << std::endl;
    } else {
        std::cout << "  Duration: Unlimited (Ctrl+C to stop)" << std::endl;
    }
    std::cout << std::endl;
    
    // Create and configure streamer
    UdpEventStreamer streamer;
    g_streamer = &streamer;
    
    if (!streamer.Initialize(targetIP, targetPort, eventsPerBatch, eventWidth, eventHeight)) {
        std::cerr << "Failed to initialize UDP event streamer" << std::endl;
        return 1;
    }
    
    // Set up real event source if requested
    StreamingApp* streamingApp = nullptr;
    if (useRealEvents) {
        streamingApp = new StreamingApp();
        if (!streamingApp->initialize()) {
            std::cerr << "Failed to initialize screen capture for real events" << std::endl;
            delete streamingApp;
            return 1;
        }
        
        // Set up event source function
        streamer.SetEventSource([streamingApp]() -> std::vector<DVSEvent> {
            std::vector<DVSEvent> dvsEvents;
            
            const EventStream& stream = streamingApp->getEventStream();
            if (stream.size() > 0) {
                // Get recent events (last 100ms)
                uint64_t currentTime = HighResTimer::GetMicroseconds();
                uint64_t recentThreshold = 100000; // 100ms
                
                auto eventsCopy = stream.getEventsCopy();
                
                for (const auto& event : eventsCopy) {
                    uint64_t eventAbsoluteTime = stream.start_time + event.timestamp;
                    uint64_t eventAge = currentTime - eventAbsoluteTime;
                    
                    if (eventAge <= recentThreshold) {
                        DVSEvent dvsEvent;
                        dvsEvent.timestamp = eventAbsoluteTime;
                        dvsEvent.x = event.x;
                        dvsEvent.y = event.y;
                        dvsEvent.polarity = (event.polarity > 0);
                        dvsEvents.push_back(dvsEvent);
                    }
                }
            }
            
            return dvsEvents;
        });
        
        // Start screen capture
        streamingApp->startStreaming();
    }
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    // Start streaming
    streamer.Start();
    
    // Run for specified duration or until interrupted
    if (durationSeconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        std::cout << "Duration elapsed, stopping streamer..." << std::endl;
        streamer.Stop();
    } else {
        // Wait for interruption
        std::cout << "Streaming... Press Ctrl+C to stop." << std::endl;
        while (streamer.IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // Cleanup
    if (streamingApp) {
        streamingApp->stopStreaming();
        delete streamingApp;
    }
    
    std::cout << "UDP Event Streamer finished." << std::endl;
    return 0;
}