#include "streaming_app.h"
#include "visualization/direct_overlay_viewer.h"
#include "core/event_file_formats.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

using namespace neuromorphic;

// Global flag for signal handling
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping overlay..." << std::endl;
    g_running.store(false);
}

void printUsage(const std::string& programName) {
    std::cout << "Neuromorphic Screen Capture - Direct Overlay Mode" << std::endl;
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --save <filename>    Save captured events to file (optional)" << std::endl;
    std::cout << "  --format <format>    File format: aedat, csv, space (default: aedat)" << std::endl;
    std::cout << "  --dimming <rate>     Dimming rate multiplier (0.1-3.0, default: 1.0)" << std::endl;
    std::cout << "  --no-dimming         Disable dimming effect" << std::endl;
    std::cout << "  --help               Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Ctrl+C              Stop overlay and exit" << std::endl;
    std::cout << std::endl;
    std::cout << "The overlay will display neuromorphic events directly on your screen." << std::endl;
    std::cout << "Green dots = positive events (brightness increase)" << std::endl;
    std::cout << "Red dots = negative events (brightness decrease)" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string saveFilename;
    EventFileFormat saveFormat = EventFileFormat::BINARY_AEDAT;
    float dimmingRate = 1.0f;
    bool useDimming = true;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--save" && i + 1 < argc) {
            saveFilename = argv[++i];
        }
        else if (arg == "--format" && i + 1 < argc) {
            std::string format = argv[++i];
            if (format == "aedat") {
                saveFormat = EventFileFormat::BINARY_AEDAT;
            } else if (format == "csv") {
                saveFormat = EventFileFormat::TEXT_CSV;
            } else if (format == "space") {
                saveFormat = EventFileFormat::TEXT_SPACE;
            } else {
                std::cerr << "Unknown format: " << format << std::endl;
                std::cerr << "Supported formats: aedat, csv, space" << std::endl;
                return 1;
            }
        }
        else if (arg == "--dimming" && i + 1 < argc) {
            dimmingRate = std::stof(argv[++i]);
            if (dimmingRate < 0.1f || dimmingRate > 3.0f) {
                std::cerr << "Dimming rate must be between 0.1 and 3.0" << std::endl;
                return 1;
            }
        }
        else if (arg == "--no-dimming") {
            useDimming = false;
        }
        else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "=== Neuromorphic Screen Capture - Direct Overlay Mode ===" << std::endl;
    std::cout << "Initializing direct overlay viewer..." << std::endl;
    
    // Create streaming app
    StreamingApp streamingApp;
    if (saveFilename.empty()) {
        std::cout << "No save file specified - events will not be saved" << std::endl;
    } else {
        streamingApp.setSaveOptions(saveFilename, saveFormat);
        std::cout << "Events will be saved to: " << saveFilename << std::endl;
    }
    
    // Initialize streaming
    if (!streamingApp.initialize()) {
        std::cerr << "Failed to initialize streaming app" << std::endl;
        return 1;
    }
    
    // Create and initialize overlay viewer
    DirectOverlayViewer overlayViewer(streamingApp);
    if (!overlayViewer.Initialize()) {
        std::cerr << "Failed to initialize overlay viewer" << std::endl;
        return 1;
    }
    
    // Configure overlay settings
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
    
    std::cout << std::endl;
    std::cout << "=== Direct Overlay Active ===" << std::endl;
    std::cout << "The overlay is now displaying events directly on your screen." << std::endl;
    std::cout << "Green dots = positive events (brightness increase)" << std::endl;
    std::cout << "Red dots = negative events (brightness decrease)" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop and exit..." << std::endl;
    
    // Main loop - just wait for exit signal
    while (g_running.load()) {
        Sleep(100); // Sleep for 100ms
        
        // Check if streaming is still active
        if (!streamingApp.isRunning()) {
            std::cout << "Streaming stopped unexpectedly" << std::endl;
            break;
        }
        
        // Handle Windows messages for overlay window
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
    
    std::cout << "\nStopping overlay and streaming..." << std::endl;
    
    // Stop overlay and streaming
    overlayViewer.StopOverlay();
    streamingApp.stopStreaming();
    
    std::cout << "Direct overlay session completed." << std::endl;
    return 0;
}