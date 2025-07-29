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
#include "capture/screen_capture.h"

using namespace neuromorphic;

/**
 * Command-line argument parser
 */
class CommandLineParser {
private:
    std::vector<std::string> m_args;
    
public:
    CommandLineParser(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            m_args.push_back(argv[i]);
        }
    }
    
    bool hasFlag(const std::string& flag) const {
        for (const auto& arg : m_args) {
            if (arg == flag) return true;
        }
        return false;
    }
    
    std::string getValue(const std::string& flag) const {
        for (size_t i = 0; i < m_args.size() - 1; i++) {
            if (m_args[i] == flag) {
                return m_args[i + 1];
            }
        }
        return "";
    }
    
    void printUsage() const {
        std::cout << "Neuromorphic Screens - Event-based Screen Capture\n";
        std::cout << "Usage:\n";
        std::cout << "  --capture --output <filename> [--duration <seconds>] [--format <aedat|csv|txt>]\n";
        std::cout << "  --replay --input <filename>\n";
        std::cout << "  --help\n";
        std::cout << "\nFor GUI visualization, use:\n";
        std::cout << "  neuromorphic_screens_imgui.exe --input <filename>\n";
        std::cout << "\nSupported formats:\n";
        std::cout << "  aedat  - AEDAT binary format (default, most efficient)\n";
        std::cout << "  csv    - CSV text format with headers\n";
        std::cout << "  txt    - Space-separated format (rpg_dvs_ros compatible)\n";
    }
};

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
            
            if (capture.CaptureFrame(events, currentTime)) {
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
        
        // Save events
        if (EventFileFormats::WriteEvents(events, outputFile, format)) {
            std::cout << "Events saved to: " << outputFile << std::endl;
            
            // Display statistics
            EventStats stats;
            stats.calculate(events);
            
            std::cout << "\nCapture Statistics:" << std::endl;
            std::cout << "Total Events: " << stats.total_events << std::endl;
            std::cout << "Positive Events: " << stats.positive_events << std::endl;
            std::cout << "Negative Events: " << stats.negative_events << std::endl;
            std::cout << "Duration: " << (stats.duration_us / 1000000.0f) << " seconds" << std::endl;
            std::cout << "Events/second: " << stats.events_per_second << std::endl;
        } else {
            std::cerr << "Failed to save events to file" << std::endl;
        }
    }
    
    void replayEvents(const std::string& inputFile) {
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
        
        // Show sample events
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
        
        std::cout << "\nUse neuromorphic_screens_imgui.exe --input " << inputFile 
                  << " for visual playback" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Neuromorphic Screens v1.0\n";
    std::cout << "Event-Based Screen Capture System\n\n";
    
    CommandLineParser parser(argc, argv);
    NeuromorphicApp app;
    
    if (parser.hasFlag("--help") || argc == 1) {
        parser.printUsage();
        return 0;
    }
    
    try {
        if (parser.hasFlag("--capture")) {
            std::string outputFile = parser.getValue("--output");
            if (outputFile.empty()) {
                std::cerr << "Output file required for capture mode" << std::endl;
                parser.printUsage();
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
                parser.printUsage();
                return 1;
            }
            
            app.replayEvents(inputFile);
            
        } else {
            std::cerr << "Unknown command" << std::endl;
            parser.printUsage();
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