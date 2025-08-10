#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "core/event_types.h"
#include "core/timing.h"
#include "core/event_file_formats.h"
#include "core/command_line_parser.h"
#include "core/streaming_app.h"
#include "capture/screen_capture.h"
#include "visualization/imgui_event_viewer.h"
#include "visualization/imgui_streaming_viewer.h"
#include "visualization/direct_overlay_viewer.h"
#include "streaming/udp_event_streamer.h"

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
    std::cout << "Neuromorphic Screens - Event-based Screen Capture System\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " --mode <mode> [options]\n\n";
    
    std::cout << "Modes:\n";
    std::cout << "  capture    Simple capture and save to file\n";
    std::cout << "  replay     Replay events from file (with optional GUI)\n";
    std::cout << "  stream     Real-time capture with GUI visualization\n";
    std::cout << "  overlay    Direct screen overlay with lightweight controls\n";
    std::cout << "  udp        UDP streaming with real-time visualization\n\n";
    
    std::cout << "Capture Mode Options:\n";
    std::cout << "  --output <filename>      Output file (required)\n";
    std::cout << "  --duration <seconds>     Recording duration (1-60, default: 5)\n";
    std::cout << "  --format <format>        File format: aedat|csv|txt (default: aedat)\n\n";
    
    std::cout << "Replay Mode Options:\n";
    std::cout << "  --input <filename>       Input file (required)\n";
    std::cout << "  --gui                    Use ImGui visualization\n\n";
    
    std::cout << "Stream Mode Options:\n";
    std::cout << "  --save <filename>        Save captured events to file (optional)\n";
    std::cout << "  --format <format>        File format: aedat|csv (default: aedat)\n\n";
    
    std::cout << "Overlay Mode Options:\n";
    std::cout << "  --save <filename>        Save captured events to file (optional)\n";
    std::cout << "  --dimming <rate>         Dimming rate multiplier (0.1-3.0, default: 1.0)\n";
    std::cout << "  --no-dimming             Disable dimming effect\n\n";
    
    std::cout << "UDP Mode Options:\n";
    std::cout << "  --ip <address>           Target IP address (default: 127.0.0.1)\n";
    std::cout << "  --port <port>            Target UDP port (default: 9999)\n";
    std::cout << "  --batch <size>           Events per UDP packet (default: 10000)\n";
    std::cout << "  --throughput <mbps>      Target throughput in MB/s (default: 20.0)\n";
    std::cout << "  --maxdrop <ratio>        Max event drop ratio 0.0-1.0 (default: 0.1)\n";
    std::cout << "  --duration <seconds>     Run for specified duration (default: unlimited)\n";
    std::cout << "  --novis                  No visualization (UDP only)\n";
    std::cout << "  --overlay                Use overlay visualization instead of GUI\n\n";
    
    std::cout << "General Options:\n";
    std::cout << "  --help                   Show this help message\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " --mode capture --output recording.aedat --duration 10\n";
    std::cout << "  " << programName << " --mode replay --input recording.aedat --gui\n";
    std::cout << "  " << programName << " --mode stream --save live_capture.aedat\n";
    std::cout << "  " << programName << " --mode overlay --dimming 1.5\n";
    std::cout << "  " << programName << " --mode udp --port 9999 --batch 2000\n";
}

/**
 * Unified Neuromorphic Application with four core modes
 */
class NeuromorphicApp {
public:
    // Mode 1: Simple capture and save
    void captureMode(const CommandLineParser& parser) {
        std::string outputFile = parser.getValue("--output");
        if (outputFile.empty()) {
            std::cerr << "Output file required for capture mode" << std::endl;
            return;
        }
        
        int duration = parser.getIntValue("--duration", 5);
        duration = (std::max)(1, (std::min)(60, duration)); // Clamp 1-60 seconds
        
        EventFileFormat format = EventFileFormat::BINARY_AEDAT;
        std::string formatStr = parser.getValue("--format");
        if (formatStr == "csv") format = EventFileFormat::TEXT_CSV;
        else if (formatStr == "txt") format = EventFileFormat::TEXT_SPACE;
        
        std::cout << "=== Simple Capture Mode ===" << std::endl;
        std::cout << "Output: " << outputFile << std::endl;
        std::cout << "Duration: " << duration << " seconds" << std::endl;
        std::cout << "Format: " << formatStr << std::endl;
        
        ScreenCapture capture;
        if (!capture.Initialize()) {
            std::cerr << "Failed to initialize screen capture" << std::endl;
            return;
        }
        
        if (!capture.StartCapture()) {
            std::cerr << "Failed to start screen capture" << std::endl;
            return;
        }
        
        EventStream events;
        events.width = capture.GetWidth();
        events.height = capture.GetHeight();
        events.start_time = HighResTimer::GetMicroseconds();
        events.max_events = constants::UNLIMITED_BUFFER; // Unlimited buffer for file capture
        
        RecordingTimer timer;
        timer.Start(duration);
        
        std::cout << "Recording... Screen: " << events.width << "x" << events.height << std::endl;
        
        uint32_t totalEvents = 0;
        FrameRateLimiter limiter(60.0f);
        
        while (timer.IsRecording() && timer.ShouldContinue()) {
            limiter.WaitForNextFrame();
            
            uint64_t currentTime = HighResTimer::GetMicroseconds();
            uint32_t frameEvents = static_cast<uint32_t>(events.events.size());
            
            if (capture.CaptureFrame(events, currentTime, 30.0f, 3, 3072 * 1920)) {
                uint32_t newEvents = static_cast<uint32_t>(events.events.size()) - frameEvents;
                totalEvents += newEvents;
                
                if (newEvents > 0) {
                    float progress = timer.GetElapsedSeconds() / duration;
                    std::cout << "Events: " << totalEvents << " (+" << newEvents << ") - " 
                             << static_cast<int>(progress * 100.0f) << "%\\r" << std::flush;
                }
            }
        }
        
        capture.StopCapture();
        std::cout << "\\nCapture completed. Total events: " << events.events.size() << std::endl;
        
        if (events.events.empty()) {
            std::cout << "No events captured. Try moving your mouse during recording." << std::endl;
            return;
        }
        
        // Save events
        if (EventFileFormats::WriteEvents(events, outputFile, format)) {
            std::cout << "Events saved to: " << outputFile << std::endl;
            
            EventStats stats;
            stats.calculate(events);
            std::cout << "Events/second: " << stats.events_per_second << std::endl;
        } else {
            std::cerr << "Failed to save events to file" << std::endl;
        }
    }
    
    // Mode 2: Simple replay with optional GUI
    void replayMode(const CommandLineParser& parser) {
        std::string inputFile = parser.getValue("--input");
        if (inputFile.empty()) {
            std::cerr << "Input file required for replay mode" << std::endl;
            return;
        }
        
        bool useGui = parser.hasFlag("--gui");
        
        std::cout << "=== Replay Mode ===" << std::endl;
        std::cout << "Input: " << inputFile << std::endl;
        std::cout << "GUI: " << (useGui ? "enabled" : "disabled") << std::endl;
        
        EventStream events;
        if (!EventFileFormats::ReadEvents(events, inputFile)) {
            std::cerr << "Failed to load events from: " << inputFile << std::endl;
            return;
        }
        
        std::cout << "Loaded " << events.events.size() << " events" << std::endl;
        
        if (events.events.empty()) {
            std::cout << "No events found in file" << std::endl;
            return;
        }
        
        EventStats stats;
        stats.calculate(events);
        std::cout << "Duration: " << (stats.duration_us / 1000000.0f) << " seconds" << std::endl;
        std::cout << "Events/second: " << stats.events_per_second << std::endl;
        std::cout << "Screen: " << events.width << "x" << events.height << std::endl;
        
        if (useGui) {
            std::cout << "Launching ImGui visualization..." << std::endl;
            
            try {
                ImGuiEventViewer viewer;
                
                if (!viewer.Initialize("Neuromorphic Event Viewer", 1280, 720)) {
                    std::cerr << "Failed to initialize ImGui event viewer" << std::endl;
                    return;
                }
                
                if (!viewer.LoadEvents(inputFile)) {
                    std::cerr << "Failed to load events in ImGui viewer" << std::endl;
                    return;
                }
                
                while (viewer.Render()) {
                    // Main render loop
                }
                
                std::cout << "GUI closed successfully." << std::endl;
                
            } catch (const std::exception& e) {
                std::cerr << "GUI Error: " << e.what() << std::endl;
            }
        } else {
            // Show sample events in console
            std::cout << "\\nFirst 10 events:" << std::endl;
            size_t maxEvents = (std::min)(static_cast<size_t>(10), events.events.size());
            for (size_t i = 0; i < maxEvents; i++) {
                const Event& event = events.events[i];
                std::cout << "  [" << i << "] t=" << event.timestamp << " x=" << event.x 
                         << " y=" << event.y << " pol=" << static_cast<int>(event.polarity) << std::endl;
            }
            std::cout << "\\nUse --gui flag for visual playback" << std::endl;
        }
    }
    
    // Mode 3: Real-time streaming with GUI visualization
    void streamMode(const CommandLineParser& parser) {
        std::cout << "=== Real-Time Streaming Mode ===" << std::endl;
        
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
            return;
        }
        
        // Create and run streaming GUI
        ImGuiStreamingViewer viewer("Neuromorphic Event Streaming", streamingApp);
        
        if (!viewer.Initialize()) {
            std::cerr << "Failed to initialize streaming GUI" << std::endl;
            return;
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
    }
    
    // Mode 4: Direct overlay with lightweight controls
    void overlayMode(const CommandLineParser& parser) {
        std::cout << "=== Direct Overlay Mode ===" << std::endl;
        
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
            return;
        }
        
        // Create and configure overlay viewer
        DirectOverlayViewer overlayViewer(streamingApp);
        if (!overlayViewer.Initialize()) {
            std::cerr << "Failed to initialize overlay viewer" << std::endl;
            return;
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
        
        std::cout << "\\n=== Direct Overlay Active ===" << std::endl;
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
    }
    
    // Mode 5: UDP streaming with real-time visualization
    void udpMode(const CommandLineParser& parser) {
        std::cout << "=== UDP Streaming Mode ===" << std::endl;
        
        // Parse UDP parameters
        std::string targetIP = parser.getValue("--ip");
        if (targetIP.empty()) targetIP = "127.0.0.1";
        
        int targetPort = parser.getIntValue("--port", 9999);
        int eventsPerBatch = parser.getIntValue("--batch", 10000);
        int durationSeconds = parser.getIntValue("--duration", 0);
        bool noVisualization = parser.hasFlag("--novis");
        bool showOverlay = parser.hasFlag("--overlay");
        
        std::cout << "Target: " << targetIP << ":" << targetPort << std::endl;
        std::cout << "Events per batch: " << eventsPerBatch << std::endl;
        std::cout << "Mode: Real screen events" << std::endl;
        if (durationSeconds > 0) {
            std::cout << "Duration: " << durationSeconds << " seconds" << std::endl;
        } else {
            std::cout << "Duration: Unlimited (Ctrl+C to stop)" << std::endl;
        }
        std::cout << "Visualization: " << (noVisualization ? "None" : (showOverlay ? "Overlay" : "GUI Window")) << std::endl;
        
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
            std::cout << "Save file: " << saveFile << std::endl;
        }
        
        if (!streamingApp.initialize()) {
            std::cerr << "Failed to initialize screen capture" << std::endl;
            return;
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
            return;
        }
        
        // Set up safe event source that uses thread-safe methods with incremental processing
        std::atomic<bool> eventSourceActive(true);
        
        streamer.SetEventSource([&streamingApp, &eventSourceActive]() -> std::vector<DVSEvent> {
            std::vector<DVSEvent> dvsEvents;
            
            // Check if we should still be processing events
            if (!eventSourceActive.load()) {
                return dvsEvents;
            }
            
            // Static variables to track incremental processing
            static uint64_t lastProcessedCount = 0;
            static uint64_t debugCounter = 0;
            
            try {
                // Get a thread-safe copy of recent events
                const EventStream& stream = streamingApp.getEventStream();
                size_t currentStreamSize = stream.size();
                uint64_t totalEventsGenerated = stream.total_events_generated;
                
                if (currentStreamSize > 0) {
                    // Only process NEW events since last call
                    uint64_t newEventsCount = totalEventsGenerated - lastProcessedCount;
                    
                    if (newEventsCount > 0) {
                        uint64_t currentTime = HighResTimer::GetMicroseconds();
                        
                        // Get the recent events safely
                        auto eventsCopy = stream.getEventsCopy();
                        
                        // Note: Stride is controlled at capture level, not UDP transmission level
                        
                        // Calculate how many events from the end to process (only new ones)
                        size_t startIndex = 0;
                        if (eventsCopy.size() > newEventsCount) {
                            startIndex = eventsCopy.size() - newEventsCount;
                        }
                        
                        // Process ALL new events without spatial filtering
                        size_t eventsSelected = 0;
                        
                        for (size_t i = startIndex; i < eventsCopy.size() && eventSourceActive.load(); ++i) {
                            const auto& event = eventsCopy[i];
                            
                            // Use current time as timestamp and create DVSEvent from core Event
                            Event timedEvent = event;
                            timedEvent.timestamp = currentTime;
                            DVSEvent dvsEvent(timedEvent);
                            dvsEvents.push_back(dvsEvent);
                            eventsSelected++;
                        }
                        
                        // Update our tracking counter
                        lastProcessedCount = totalEventsGenerated;
                        
                        // Debug output for incremental processing
                        if (++debugCounter % 50 == 0 && newEventsCount > 0) {
                            std::cout << "UDP Event Source: NEW events=" << newEventsCount 
                                      << ", buffer_size=" << eventsCopy.size() 
                                      << ", transmitted=" << eventsSelected << std::endl;
                        }
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
                guiViewer = new ImGuiStreamingViewer("Neuromorphic Event Streaming", streamingApp);
                if (guiViewer->Initialize()) {
                    // GUI will run in main loop below
                    std::cout << "GUI visualization ready" << std::endl;
                } else {
                    delete guiViewer;
                    guiViewer = nullptr;
                    std::cout << "Warning: Failed to initialize GUI" << std::endl;
                }
            }
        }
        
        std::cout << "\\nUDP streaming active. Press Ctrl+C to stop." << std::endl;
        
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
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Neuromorphic Screens - Unified Application v2.0\\n";
    std::cout << "Event-Based Screen Capture with Multiple Modes\\n\\n";
    
    CommandLineParser parser(argc, argv);
    NeuromorphicApp app;
    
    if (parser.hasFlag("--help") || argc == 1) {
        printUsage(argv[0]);
        return 0;
    }
    
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::string mode = parser.getValue("--mode");
        
        if (mode == "capture") {
            app.captureMode(parser);
        } else if (mode == "replay") {
            app.replayMode(parser);
        } else if (mode == "stream") {
            app.streamMode(parser);
        } else if (mode == "overlay") {
            app.overlayMode(parser);
        } else if (mode == "udp") {
            app.udpMode(parser);
        } else {
            std::cerr << "Invalid mode: " << mode << std::endl;
            std::cerr << "Valid modes: capture, replay, stream, overlay, udp" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
    
    return 0;
}