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
        std::cout << "  --test\n";
        std::cout << "  --generate-test-data --output <filename> [--format <binary|csv|txt>]\n";
        std::cout << "  --capture --output <filename> [--duration <seconds>] [--format <binary|csv|txt>]\n";
        std::cout << "  --replay --input <filename>\n";
        std::cout << "  --help\n";
        std::cout << "\nFor GUI visualization, use:\n";
        std::cout << "  neuromorphic_screens_gui.exe --input <filename>\n";
        std::cout << "\nSupported formats:\n";
        std::cout << "  binary - Binary .evt format (default, most efficient)\n";
        std::cout << "  csv    - CSV text format with headers\n";
        std::cout << "  txt    - Space-separated format (rpg_dvs_ros compatible)\n";
    }
};

/**
 * Helper function to parse format parameter
 */
EventFileFormat ParseFormat(const std::string& formatStr) {
    if (formatStr == "csv") {
        return EventFileFormat::TEXT_CSV;
    } else if (formatStr == "txt") {
        return EventFileFormat::TEXT_SPACE;
    } else if (formatStr == "binary" || formatStr.empty()) {
        return EventFileFormat::BINARY_NEVS;
    } else {
        throw std::invalid_argument("Invalid format: " + formatStr + ". Use binary, csv, or txt");
    }
}

/**
 * Main application class
 */
class NeuromorphicScreens {
private:
    std::unique_ptr<ScreenCapture> m_capture;

public:
    NeuromorphicScreens() {
        HighResTimer::Initialize();
    }
    
    void runTests() {
        std::cout << "Running tests..." << std::endl;
        
        // Test timing
        uint64_t start = HighResTimer::GetMicroseconds();
        HighResTimer::SleepMilliseconds(100);
        uint64_t end = HighResTimer::GetMicroseconds();
        uint64_t elapsed = end - start;
        std::cout << "Timing test: " << elapsed << " microseconds (expected ~100000)" << std::endl;
        
        // Test event generation
        EventStream testStream;
        testStream.width = 1920;
        testStream.height = 1080;
        testStream.start_time = HighResTimer::GetMicroseconds();
        
        // Generate some test events
        for (int i = 0; i < 100; i++) {
            Event event(testStream.start_time + i * 1000, 
                       static_cast<uint16_t>(i * 10), 
                       static_cast<uint16_t>(i * 5), 
                       (i % 2 == 0) ? 1 : -1);
            testStream.events.push_back(event);
        }
        
        // Test file I/O
        std::string testFile = "test_events.evt";
        if (EventFile::WriteEvents(testStream, testFile)) {
            std::cout << "Event file write test: PASSED" << std::endl;
            
            EventStream loadedStream;
            if (EventFile::ReadEvents(loadedStream, testFile)) {
                std::cout << "Event file read test: PASSED" << std::endl;
                std::cout << "Loaded " << loadedStream.events.size() << " events" << std::endl;
            } else {
                std::cout << "Event file read test: FAILED" << std::endl;
            }
        } else {
            std::cout << "Event file write test: FAILED" << std::endl;
        }
        
        std::cout << "Tests completed" << std::endl;
    }
    
    void generateTestData(const std::string& outputFile, EventFileFormat format = EventFileFormat::BINARY_NEVS) {
        std::cout << "Generating test event data..." << std::endl;
        
        EventStream testStream;
        testStream.width = 1920;
        testStream.height = 1080;
        testStream.start_time = HighResTimer::GetMicroseconds();
        
        // Generate simulated screen events
        uint64_t currentTime = testStream.start_time;
        
        // Simulate a moving object (like a window being dragged)
        for (int frame = 0; frame < 300; frame++) { // 5 seconds at 60 FPS
            currentTime = testStream.start_time + frame * 16667; // 60 FPS
            
            // Simulate a moving rectangle
            int x = 100 + (frame * 2) % 1600; // Move horizontally
            int y = 100 + (frame * 1) % 800;  // Move vertically
            
            // Generate events around the moving object
            for (int dx = -20; dx <= 20; dx += 5) {
                for (int dy = -20; dy <= 20; dy += 5) {
                    int eventX = x + dx;
                    int eventY = y + dy;
                    
                    if (eventX >= 0 && eventX < 1920 && eventY >= 0 && eventY < 1080) {
                        Event event(currentTime, 
                                   static_cast<uint16_t>(eventX), 
                                   static_cast<uint16_t>(eventY), 
                                   (frame % 2 == 0) ? 1 : -1);
                        testStream.events.push_back(event);
                    }
                }
            }
        }
        
        // Save to file in specified format
        if (EventFileFormats::WriteEvents(testStream, outputFile, format)) {
            std::cout << "Generated " << testStream.events.size() << " events" << std::endl;
            std::cout << "Saved to: " << outputFile << std::endl;
            
            // Show statistics
            EventStats stats;
            stats.calculate(testStream);
            std::cout << "Statistics:\n";
            std::cout << "  Total events: " << stats.total_events << "\n";
            std::cout << "  Positive events: " << stats.positive_events << "\n";
            std::cout << "  Negative events: " << stats.negative_events << "\n";
            std::cout << "  Events per second: " << stats.events_per_second << "\n";
            std::cout << "  Duration: " << (stats.duration_us / 1000000.0) << " seconds\n";
        } else {
            std::cerr << "Failed to save test data" << std::endl;
        }
    }
    
    void captureScreen(const std::string& outputFile, int durationSeconds = 5, EventFileFormat format = EventFileFormat::BINARY_NEVS) {
        std::cout << "Initializing real screen capture..." << std::endl;
        
        // Initialize screen capture
        m_capture = std::make_unique<ScreenCapture>();
        if (!m_capture->Initialize()) {
            std::cerr << "Failed to initialize screen capture" << std::endl;
            return;
        }
        
        std::cout << "Starting screen capture for " << durationSeconds << " seconds..." << std::endl;
        std::cout << "Move your mouse or open/close windows to generate events" << std::endl;
        
        // Start capture first
        if (!m_capture->StartCapture()) {
            std::cerr << "Failed to start capture" << std::endl;
            return;
        }
        
        EventStream events;
        events.width = m_capture->GetWidth();
        events.height = m_capture->GetHeight();
        events.start_time = HighResTimer::GetMicroseconds(); // Record start time after capture is ready
        
        RecordingTimer timer;
        timer.Start(durationSeconds);
        
        FrameRateLimiter limiter(60.0f); // 60 FPS capture
        
        std::cout << "Recording... (Press Ctrl+C to stop early)" << std::endl;
        
        uint32_t frameCount = 0;
        uint32_t successfulFrames = 0;
        uint32_t lastEventCount = 0;
        
        while (timer.ShouldContinue()) {
            uint64_t currentTime = HighResTimer::GetMicroseconds();
            frameCount++;
            
            // Capture frame and generate events
            uint32_t eventsBefore = events.events.size();
            if (m_capture->CaptureFrame(events, currentTime)) {
                successfulFrames++;
                uint32_t newEvents = events.events.size() - eventsBefore;
                if (newEvents > 0) {
                    std::cout << "\nFrame " << frameCount << ": Generated " << newEvents << " events (total: " << events.events.size() << ")" << std::endl;
                }
            }
            
            // Show progress every 60 frames or when events are generated
            if (frameCount % 60 == 0 || events.events.size() != lastEventCount) {
                float elapsed = timer.GetElapsedSeconds();
                float remaining = timer.GetRemainingSeconds();
                std::cout << "\rRecording... " << elapsed << "s / " << durationSeconds << "s (frames: " << successfulFrames << "/" << frameCount << ", events: " << events.events.size() << ")" << std::flush;
                lastEventCount = events.events.size();
            }
            
            limiter.WaitForNextFrame();
        }
        
        std::cout << "\nCapture completed. Saving to: " << outputFile << std::endl;
        
        // Stop capture
        m_capture->StopCapture();
        
        if (EventFileFormats::WriteEvents(events, outputFile, format)) {
            std::cout << "Successfully saved " << events.events.size() << " events" << std::endl;
            
            // Show statistics
            EventStats stats;
            stats.calculate(events);
            std::cout << "Statistics:\n";
            std::cout << "  Total events: " << stats.total_events << "\n";
            std::cout << "  Positive events: " << stats.positive_events << "\n";
            std::cout << "  Negative events: " << stats.negative_events << "\n";
            std::cout << "  Events per second: " << stats.events_per_second << "\n";
            std::cout << "  Duration: " << (stats.duration_us / 1000000.0) << " seconds\n";
        } else {
            std::cerr << "Failed to save capture data" << std::endl;
        }
    }
    
    void replayEvents(const std::string& inputFile) {
        std::cout << "Loading events from: " << inputFile << std::endl;
        
        EventStream events;
        if (!EventFileFormats::ReadEvents(events, inputFile)) {
            std::cerr << "Failed to load events" << std::endl;
            return;
        }
        
        std::cout << "Loaded " << events.events.size() << " events" << std::endl;
        std::cout << "Screen size: " << events.width << "x" << events.height << std::endl;
        
        // Show statistics
        EventStats stats;
        stats.calculate(events);
        std::cout << "Statistics:\n";
        std::cout << "  Total events: " << stats.total_events << "\n";
        std::cout << "  Positive events: " << stats.positive_events << "\n";
        std::cout << "  Negative events: " << stats.negative_events << "\n";
        std::cout << "  Events per second: " << stats.events_per_second << "\n";
        std::cout << "  Duration: " << (stats.duration_us / 1000000.0) << " seconds\n";
        
        // Show first 10 events
        std::cout << "\nFirst 10 events:\n";
        size_t numToShow = (events.events.size() < 10) ? events.events.size() : 10;
        for (size_t i = 0; i < numToShow; i++) {
            const auto& event = events.events[i];
            std::cout << "  Event " << i << ": (" << event.x << ", " << event.y 
                      << ") polarity=" << (int)event.polarity 
                      << " time=" << (event.timestamp - events.start_time) / 1000 << "ms\n";
        }
        
        // Show last 10 events
        if (events.events.size() > 10) {
            std::cout << "\nLast 10 events:\n";
            for (size_t i = events.events.size() - 10; i < events.events.size(); i++) {
                const auto& event = events.events[i];
                std::cout << "  Event " << i << ": (" << event.x << ", " << event.y 
                          << ") polarity=" << (int)event.polarity 
                          << " time=" << (event.timestamp - events.start_time) / 1000 << "ms\n";
            }
        }
    }
    
};

int main(int argc, char* argv[]) {
    CommandLineParser parser(argc, argv);
    
    if (parser.hasFlag("--help")) {
        parser.printUsage();
        return 0;
    }
    
    NeuromorphicScreens app;
    
    try {
        if (parser.hasFlag("--test")) {
            app.runTests();
        } else if (parser.hasFlag("--generate-test-data")) {
            std::string outputFile = parser.getValue("--output");
            
            if (outputFile.empty()) {
                std::cerr << "Missing --output parameter" << std::endl;
                parser.printUsage();
                return 1;
            }
            
            std::string formatStr = parser.getValue("--format");
            EventFileFormat format = ParseFormat(formatStr);
            
            app.generateTestData(outputFile, format);
            
        } else if (parser.hasFlag("--capture")) {
            std::string outputFile = parser.getValue("--output");
            
            if (outputFile.empty()) {
                std::cerr << "Missing --output parameter" << std::endl;
                parser.printUsage();
                return 1;
            }
            
            std::string durationStr = parser.getValue("--duration");
            int duration = 5; // Default 5 seconds
            if (!durationStr.empty()) {
                try {
                    duration = std::stoi(durationStr);
                    if (duration <= 0 || duration > 60) {
                        std::cerr << "Duration must be between 1 and 60 seconds" << std::endl;
                        return 1;
                    }
                } catch (const std::exception&) {
                    std::cerr << "Invalid duration value" << std::endl;
                    return 1;
                }
            }
            
            std::string formatStr = parser.getValue("--format");
            EventFileFormat format = ParseFormat(formatStr);
            
            app.captureScreen(outputFile, duration, format);
            
        } else if (parser.hasFlag("--replay")) {
            std::string inputFile = parser.getValue("--input");
            
            if (inputFile.empty()) {
                std::cerr << "Missing --input parameter" << std::endl;
                parser.printUsage();
                return 1;
            }
            
            app.replayEvents(inputFile);
            
        } else {
            std::cout << "No action specified. Use --help for usage information." << std::endl;
            parser.printUsage();
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 