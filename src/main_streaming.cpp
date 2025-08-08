#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "core/command_line_parser.h"
#include "core/streaming_app.h"
#include "core/event_file_formats.h"
#include "visualization/imgui_streaming_viewer.h"
#include "visualization/direct_overlay_viewer.h"
#include "streaming/udp_event_streamer.h"

// Platform-specific includes for message handling and sleep
#ifdef _WIN32
    // Windows.h is already included via the headers above
#endif

using namespace neuromorphic;

// Global flag for signal handling
std::atomic<bool> g_running(true);
UdpEventStreamer* g_udpStreamer = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping..." << std::endl;
    g_running.store(false);
    if (g_udpStreamer) {
        g_udpStreamer->Stop();
    }
}

void printUsage(const std::string& programName) {
    std::cout << "Neuromorphic Screens - Real-Time Event Streaming\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " [options]                    # Default: streaming with GUI\n";
    std::cout << "  " << programName << " --overlay [options]          # Direct overlay visualization\n";
    std::cout << "  " << programName << " --UDP [options]              # UDP event streaming\n";
    std::cout << "\nGeneral Options:\n";
    std::cout << "  --save <filename>     Save captured events to file (optional)\n";
    std::cout << "  --format <format>     File format: aedat, csv (default: aedat)\n";
    std::cout << "  --help                Show this help message\n";
    std::cout << "\nOverlay Options:\n";
    std::cout << "  --dimming <rate>      Dimming rate multiplier (0.1-3.0, default: 1.0)\n";
    std::cout << "  --no-dimming          Disable dimming effect\n";
    std::cout << "\nUDP Streaming Options:\n";
    std::cout << "  --ip <address>        Target IP address (default: 127.0.0.1)\n";
    std::cout << "  --port <port>         Target UDP port (default: 9999)\n";
    std::cout << "  --batch <size>        Events per UDP packet (default: 1500)\n";
    std::cout << "  --throughput <mbps>   Target throughput in MB/s (default: 20.0)\n";
    std::cout << "  --maxdrop <ratio>     Max event drop ratio 0.0-1.0 (default: 0.1)\n";
    std::cout << "  --duration <seconds>  Run for specified duration (default: unlimited)\n";
    std::cout << "  --novis               No visualization (UDP only)\n";
    std::cout << "\nModes:\n";
    std::cout << "  Default:              Real-time streaming with ImGui window\n";
    std::cout << "  --overlay:            Direct screen overlay visualization\n";
    std::cout << "  --UDP:                UDP event streaming (with optional visualization)\n";
}

class StreamingMode {
public:
    enum Type { DEFAULT_STREAMING, OVERLAY, UDP_STREAMING };
    
    static int runDefaultStreaming(const CommandLineParser& parser) {
        std::cout << "=== Real-Time Event Streaming with GUI ===" << std::endl;
        
        StreamingApp streamingApp;
        
        // Configure save options if specified
        std::string saveFile = parser.getValue("--save");
        
        if (!saveFile.empty()) {
            EventFileFormat format = EventFileFormat::BINARY_AEDAT;
            std::string formatStr = parser.getValue("--format");
            if (formatStr == "csv") {
                format = EventFileFormat::TEXT_CSV;
            }
            streamingApp.setSaveOptions(saveFile, format);
            std::cout << "Events will be saved to: " << saveFile << std::endl;
        }
        
        // Initialize streaming
        if (!streamingApp.initialize()) {
            std::cerr << "Failed to initialize streaming app" << std::endl;
            return 1;
        }
        
        // Create and run streaming GUI
        ImGuiStreamingViewer viewer("Neuromorphic Event Streaming", streamingApp);
        
        if (!viewer.Initialize()) {
            std::cerr << "Failed to initialize streaming GUI" << std::endl;
            return 1;
        }
        
        // Start streaming
        streamingApp.startStreaming();
        std::cout << "Streaming started. Use GUI controls to adjust parameters." << std::endl;
        
        // Run GUI main loop
        viewer.Run();
        
        // Cleanup
        streamingApp.stopStreaming();
        viewer.Cleanup();
        
        std::cout << "Streaming session completed." << std::endl;
        return 0;
    }
    
    static int runOverlayMode(const CommandLineParser& parser) {
        std::cout << "=== Direct Overlay Visualization ===" << std::endl;
        
        StreamingApp streamingApp;
        
        // Configure save options
        std::string saveFile = parser.getValue("--save");
        if (!saveFile.empty()) {
            EventFileFormat format = EventFileFormat::BINARY_AEDAT;
            std::string formatStr = parser.getValue("--format");
            if (formatStr == "csv") {
                format = EventFileFormat::TEXT_CSV;
            }
            streamingApp.setSaveOptions(saveFile, format);
            std::cout << "Events will be saved to: " << saveFile << std::endl;
        }
        
        // Initialize streaming
        if (!streamingApp.initialize()) {
            std::cerr << "Failed to initialize streaming app" << std::endl;
            return 1;
        }
        
        // Create and configure overlay viewer
        DirectOverlayViewer overlayViewer(streamingApp);
        if (!overlayViewer.Initialize()) {
            std::cerr << "Failed to initialize overlay viewer" << std::endl;
            return 1;
        }
        
        // Configure overlay settings
        float dimmingRate = parser.getFloatValue("--dimming", 1.0f);
        bool useDimming = !parser.hasFlag("--no-dimming");
        
        if (dimmingRate < 0.1f || dimmingRate > 3.0f) {
            dimmingRate = 1.0f;
            std::cout << "Warning: Invalid dimming rate, using default 1.0" << std::endl;
        }
        
        overlayViewer.SetDimmingEnabled(useDimming);
        overlayViewer.SetDimmingRate(dimmingRate);
        
        std::cout << "Dimming: " << (useDimming ? "enabled" : "disabled");
        if (useDimming) {
            std::cout << " (rate: " << dimmingRate << "x)";
        }
        std::cout << std::endl;
        
        // Start streaming and overlay
        streamingApp.startStreaming();
        overlayViewer.StartOverlay();
        
        std::cout << "\n=== Direct Overlay Active ===" << std::endl;
        std::cout << "Green dots = positive events, Red dots = negative events" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        // Main loop - wait for exit signal
        while (g_running.load()) {
            Sleep(100);
            
            if (!streamingApp.isRunning()) {
                std::cout << "Streaming stopped unexpectedly" << std::endl;
                break;
            }
            
            // Handle Windows messages
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    g_running.store(false);
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        // Cleanup
        overlayViewer.StopOverlay();
        streamingApp.stopStreaming();
        
        std::cout << "Overlay session completed." << std::endl;
        return 0;
    }
    
    static int runUDPMode(const CommandLineParser& parser) {
        std::cout << "=== UDP Event Streaming ===" << std::endl;
        
        // Parse UDP parameters
        std::string targetIP = parser.getValue("--ip");
        if (targetIP.empty()) targetIP = "127.0.0.1";
        
        int targetPort = parser.getIntValue("--port", 9999);
        int eventsPerBatch = parser.getIntValue("--batch", 1500);
        int durationSeconds = parser.getIntValue("--duration", 0);
        bool noVisualization = parser.hasFlag("--novis");
        bool showOverlay = parser.hasFlag("--overlay");
        
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Target: " << targetIP << ":" << targetPort << std::endl;
        std::cout << "  Events per batch: " << eventsPerBatch << std::endl;
        std::cout << "  Mode: Real screen events" << std::endl;
        if (durationSeconds > 0) {
            std::cout << "  Duration: " << durationSeconds << " seconds" << std::endl;
        } else {
            std::cout << "  Duration: Unlimited (Ctrl+C to stop)" << std::endl;
        }
        std::cout << "  Visualization: " << (noVisualization ? "None" : (showOverlay ? "Overlay" : "GUI Window")) << std::endl;
        
        // Create streaming app for real events
        StreamingApp streamingApp;
        
        // Configure save options
        std::string saveFile = parser.getValue("--save");
        if (!saveFile.empty()) {
            EventFileFormat format = EventFileFormat::BINARY_AEDAT;
            std::string formatStr = parser.getValue("--format");
            if (formatStr == "csv") {
                format = EventFileFormat::TEXT_CSV;
            }
            streamingApp.setSaveOptions(saveFile, format);
            std::cout << "  Save file: " << saveFile << std::endl;
        }
        
        if (!streamingApp.initialize()) {
            std::cerr << "Failed to initialize screen capture" << std::endl;
            return 1;
        }
        
        // Create and configure UDP streamer
        UdpEventStreamer streamer;
        g_udpStreamer = &streamer;
        
        // Parse additional UDP performance parameters
        float targetThroughput = parser.getFloatValue("--throughput", 20.0f);
        float maxDropRatio = parser.getFloatValue("--maxdrop", 0.1f);
        
        if (!streamer.Initialize(targetIP, static_cast<uint16_t>(targetPort), 
                               static_cast<uint32_t>(eventsPerBatch), 1920, 1080,
                               targetThroughput, maxDropRatio)) {
            std::cerr << "Failed to initialize UDP event streamer" << std::endl;
            return 1;
        }
        
        // Set up safe event source that uses thread-safe methods
        std::atomic<bool> eventSourceActive(true);
        
        streamer.SetEventSource([&streamingApp, &eventSourceActive]() -> std::vector<DVSEvent> {
            std::vector<DVSEvent> dvsEvents;
            
            // Check if we should still be processing events
            if (!eventSourceActive.load()) {
                return dvsEvents;
            }
            
            try {
                // Get a thread-safe copy of recent events
                const EventStream& stream = streamingApp.getEventStream();
                size_t streamSize = stream.size();
                
                if (streamSize > 0) {
                    uint64_t currentTime = HighResTimer::GetMicroseconds();
                    
                    // Get the recent events safely
                    auto eventsCopy = stream.getEventsCopy();
                    
                    // Use current stride value from StreamingApp to control event density
                    uint32_t currentStride = streamingApp.getStride();
                    size_t eventsToProcess = (std::min)(eventsCopy.size(), static_cast<size_t>(10000)); // Base limit (??)
                    // size_t eventsToProcess = eventsCopy.size();
                    
                    // Apply stride-based filtering to reduce event density when stride > 1
                    // if (currentStride > 1) {
                    //     eventsToProcess = eventsToProcess / currentStride;
                    // }
                    
                    for (size_t i = 0; i < eventsToProcess && eventSourceActive.load(); ++i) {
                        const auto& event = eventsCopy[i];
                        
                        // Use current time as timestamp and create DVSEvent from core Event
                        Event timedEvent = event;
                        timedEvent.timestamp = currentTime;
                        DVSEvent dvsEvent(timedEvent);
                        dvsEvents.push_back(dvsEvent);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Event source error: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown event source error" << std::endl;
            }
            
            return dvsEvents;
        });
        
        std::cout << "UDP Event Streamer configured with safe event source" << std::endl;
        
        // Start screen capture and UDP streaming
        streamingApp.startStreaming();
        streamer.Start();
        
        // Visualization setup
        DirectOverlayViewer* overlayViewer = nullptr;
        ImGuiStreamingViewer* guiViewer = nullptr;
        
        if (!noVisualization) {
            if (showOverlay) {
                overlayViewer = new DirectOverlayViewer(streamingApp);
                if (overlayViewer->Initialize()) {
                    overlayViewer->StartOverlay();
                    std::cout << "Overlay visualization active" << std::endl;
                } else {
                    delete overlayViewer;
                    overlayViewer = nullptr;
                    std::cout << "Warning: Failed to initialize overlay" << std::endl;
                }
            } else {
                // For now, disable GUI mode to avoid segfault - just log events
                // std::cout << "GUI visualization disabled - logging events to console instead" << std::endl;
                // guiViewer = nullptr;
                guiViewer = new ImGuiStreamingViewer("Neuromorphic Event Streaming", streamingApp);
                if (guiViewer->Initialize()) {
                    guiViewer->Run();
                } else {
                    delete guiViewer;
                    guiViewer = nullptr;
                }
            }
        }
        
        std::cout << "\nUDP streaming active. Press Ctrl+C to stop." << std::endl;
        
        // Main loop
        if (guiViewer) {
            // GUI mode - run GUI loop
            guiViewer->Run();
        } else {
            // Console or overlay mode - wait for duration or signal
            if (durationSeconds > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
                std::cout << "Duration elapsed, stopping..." << std::endl;
            } else {
                while (g_running.load() && streamer.IsRunning()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    // Handle Windows messages for overlay
                    if (overlayViewer) {
                        MSG msg;
                        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                            if (msg.message == WM_QUIT) {
                                g_running.store(false);
                                break;
                            }
                            TranslateMessage(&msg);
                            DispatchMessage(&msg);
                        }
                    }
                }
            }
        }
        
        // Cleanup - deactivate event source first, then stop components
        std::cout << "Deactivating event source..." << std::endl;
        eventSourceActive.store(false);
        
        std::cout << "Stopping UDP streamer..." << std::endl;
        streamer.Stop();
        
        std::cout << "Stopping screen capture..." << std::endl;
        streamingApp.stopStreaming();
        
        if (overlayViewer) {
            overlayViewer->StopOverlay();
            delete overlayViewer;
        }
        if (guiViewer) {
            guiViewer->Cleanup();
            delete guiViewer;
        }
        
        std::cout << "UDP streaming session completed." << std::endl;
        return 0;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Neuromorphic Screens - Real-Time Streaming v1.0\n";
    std::cout << "Event-Based Screen Capture with Visualization\n\n";
    
    CommandLineParser parser(argc, argv);
    
    if (parser.hasFlag("--help")) {
        printUsage(argv[0]);
        return 0;
    }
    
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Determine mode
        if (parser.hasFlag("--UDP")) {
            return StreamingMode::runUDPMode(parser);
        } else if (parser.hasFlag("--overlay")) {
            return StreamingMode::runOverlayMode(parser);
        } else {
            // Default: streaming with GUI
            return StreamingMode::runDefaultStreaming(parser);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}