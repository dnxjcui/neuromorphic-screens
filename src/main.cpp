#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <windows.h>

#include "core/event_types.h"
#include "core/timing.h"
#include "core/event_file.h"
#include "core/event_file_formats.h"
#include "core/command_line_parser.h"
#include "capture/screen_capture.h"
#include "visualization/imgui_event_viewer.h"

using namespace neuromorphic;

void printUsage(const std::string& programName) {
    std::cout << "Neuromorphic Screens - Event-based Screen Capture\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " --capture --output <filename> [--duration <seconds>] [--format <aedat|csv|txt>]\n";
    std::cout << "  " << programName << " --replay --input <filename> [--gui]\n";
    std::cout << "  " << programName << " --help\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --gui                Use ImGui visualization for replay\n";
    std::cout << "  --duration <seconds> Recording duration (1-60 seconds, default: 5)\n";
    std::cout << "  --format <format>    Output format: aedat, csv, txt (default: aedat)\n";
    std::cout << "\nSupported formats:\n";
    std::cout << "  aedat  - AEDAT binary format (default, most efficient)\n";
    std::cout << "  csv    - CSV text format with headers\n";
    std::cout << "  txt    - Space-separated format (rpg_dvs_ros compatible)\n";
    std::cout << "\nNote: File capture uses unlimited buffer - all events are saved to file.\n";
}

/**
 * Main application class
 */
class NeuromorphicApp {
public:
    void captureEvents(const std::string& outputFile, int duration, EventFileFormat format = EventFileFormat::BINARY_AEDAT) {
        std::cout << "Starting neuromorphic screen capture for " << duration << " seconds..." << std::endl;
        
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
        
        std::cout << "Recording... Move your mouse or open/close applications to generate events." << std::endl;
        std::cout << "Screen resolution: " << events.width << "x" << events.height << std::endl;
        
        uint32_t totalEvents = 0;
        FrameRateLimiter limiter(60.0f);
        
        while (timer.IsRecording() && timer.ShouldContinue()) {
            limiter.WaitForNextFrame();
            
            uint64_t currentTime = HighResTimer::GetMicroseconds();
            uint32_t frameEvents = static_cast<uint32_t>(events.events.size());
            
            // bool ScreenCapture::CaptureFrame(EventStream& events, uint64_t timestamp, float threshold, uint32_t stride, size_t maxEvents) {

            // if (capture.CaptureFrame(events, currentTime)) {
            if (capture.CaptureFrame(events, currentTime, 30.0f, 3, 3072 * 1920)) {
                uint32_t newEvents = static_cast<uint32_t>(events.events.size()) - frameEvents;
                totalEvents += newEvents;
                
                if (newEvents > 0) {
                    float progress = timer.GetElapsedSeconds() / duration;
                    std::cout << "Events: " << totalEvents << " (+" << newEvents << " this frame) - " 
                             << progress * 100.0f << "% complete\r" << std::flush;
                }
            }
        }
        
        capture.StopCapture();
        
        std::cout << "\nCapture completed. Total events: " << events.events.size() << std::endl;
        
        if (events.events.empty()) {
            std::cout << "No events captured. Try moving your mouse or opening applications during recording." << std::endl;
            return;
        }
        
        // Show buffer status
        if (events.isUnlimited()) {
            std::cout << "Buffer: Unlimited (all events captured)" << std::endl;
        } else {
            std::cout << "Buffer: Limited to " << events.max_events << " events" << std::endl;
        }
        
        // Save events
        if (EventFileFormats::WriteEvents(events, outputFile, format)) {
            std::cout << "Events saved to: " << outputFile << std::endl;
            
            // Display statistics
            EventStats stats;
            stats.calculate(events);
            
            std::cout << "\nCapture Statistics:" << std::endl;
            std::cout << "Events Stored: " << stats.total_events << std::endl;
            std::cout << "Total Events Generated: " << events.total_events_generated << std::endl;
            std::cout << "Positive Events: " << stats.positive_events << std::endl;
            std::cout << "Negative Events: " << stats.negative_events << std::endl;
            std::cout << "Duration: " << (stats.duration_us / 1000000.0f) << " seconds" << std::endl;
            std::cout << "Events/second: " << stats.events_per_second << std::endl;
        } else {
            std::cerr << "Failed to save events to file" << std::endl;
        }
    }
    
    void replayEvents(const std::string& inputFile, bool useGui = false) {
        std::cout << "Loading events from: " << inputFile << std::endl;
        
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
        
        // Calculate and display statistics
        EventStats stats;
        stats.calculate(events);
        
        std::cout << "\nEvent Statistics:" << std::endl;
        std::cout << "Total Events: " << stats.total_events << std::endl;
        std::cout << "Positive Events: " << stats.positive_events << std::endl;
        std::cout << "Negative Events: " << stats.negative_events << std::endl;
        std::cout << "Duration: " << (stats.duration_us / 1000000.0f) << " seconds" << std::endl;
        std::cout << "Events/second: " << stats.events_per_second << std::endl;
        std::cout << "Screen dimensions: " << events.width << "x" << events.height << std::endl;
        
        if (useGui) {
            // Launch ImGui visualization
            std::cout << "\nLaunching ImGui visualization..." << std::endl;
            
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
                
                std::cout << "GUI launched successfully! Use Play button for automatic playback." << std::endl;
                
                // Main render loop
                while (viewer.Render()) {
                    // The render loop handles all events and drawing
                }
                
                std::cout << "ImGui application closed successfully." << std::endl;
                
            } catch (const std::exception& e) {
                std::cerr << "GUI Error: " << e.what() << std::endl;
            }
        } else {
            // Show sample events in console
            std::cout << "\nFirst 10 events:" << std::endl;
            size_t maxEvents = (events.events.size() < 10) ? events.events.size() : 10;
            for (size_t i = 0; i < maxEvents; i++) {
                const Event& event = events.events[i];
                std::cout << "  [" << i << "] t=" << event.timestamp << " x=" << event.x 
                         << " y=" << event.y << " pol=" << (int)event.polarity << std::endl;
            }
            
            if (events.events.size() > 10) {
                std::cout << "\nLast 10 events:" << std::endl;
                for (size_t i = events.events.size() - 10; i < events.events.size(); i++) {
                    const Event& event = events.events[i];
                    std::cout << "  [" << i << "] t=" << event.timestamp << " x=" << event.x 
                             << " y=" << event.y << " pol=" << (int)event.polarity << std::endl;
                }
            }
            
            std::cout << "\nUse --gui flag for visual playback" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Neuromorphic Screens v1.0\n";
    std::cout << "Event-Based Screen Capture System\n\n";
    
    CommandLineParser parser(argc, argv);
    NeuromorphicApp app;
    
    if (parser.hasFlag("--help") || argc == 1) {
        printUsage(argv[0]);
        return 0;
    }
    
    try {
        if (parser.hasFlag("--capture")) {
            std::string outputFile = parser.getValue("--output");
            if (outputFile.empty()) {
                std::cerr << "Output file required for capture mode" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            
            int duration = 5; // Default 5 seconds
            std::string durationStr = parser.getValue("--duration");
            if (!durationStr.empty()) {
                duration = std::stoi(durationStr);
                duration = (duration < 1) ? 1 : ((duration > 60) ? 60 : duration); // Clamp between 1-60 seconds
            }
            
            EventFileFormat format = EventFileFormat::BINARY_AEDAT;
            std::string formatStr = parser.getValue("--format");
            if (formatStr == "csv") {
                format = EventFileFormat::TEXT_CSV;
            } else if (formatStr == "txt") {
                format = EventFileFormat::TEXT_SPACE;
            } else if (formatStr == "aedat") {
                format = EventFileFormat::BINARY_AEDAT;
            }
            
            app.captureEvents(outputFile, duration, format);
            
        } else if (parser.hasFlag("--replay")) {
            std::string inputFile = parser.getValue("--input");
            if (inputFile.empty()) {
                std::cerr << "Input file required for replay mode" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
            
            bool useGui = parser.hasFlag("--gui");
            app.replayEvents(inputFile, useGui);
            
        } else {
            std::cerr << "Unknown command" << std::endl;
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