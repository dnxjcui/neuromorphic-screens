#include "visualization/imgui_event_viewer.h"
#include <iostream>
#include <string>

using namespace neuromorphic;

void show_usage() {
    std::cout << "Neuromorphic Screens ImGui GUI - Stable Event-Based Screen Capture Visualization\n\n";
    std::cout << "Usage:\n";
    std::cout << "  neuromorphic_screens_imgui [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --input <file>     Load events from file at startup\n";
    std::cout << "  --help             Show this help message\n\n";
    std::cout << "Controls:\n";
    std::cout << "  Play/Pause/Stop    Control event replay\n";
    std::cout << "  Speed Slider       Adjust playback speed (0.01x to 5.0x)\n";
    std::cout << "  Progress Slider    Seek to specific time in recording\n";
    std::cout << "  Downsample Slider  Reduce visualization density (1x to 8x)\n";
    std::cout << "  Statistics Panel   View event metrics and real-time status\n\n";
    std::cout << "Event Visualization:\n";
    std::cout << "  Green dots = Positive events (brightness increase)\n";
    std::cout << "  Red dots = Negative events (brightness decrease)\n";
    std::cout << "  Dots fade over time for transient visualization\n\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Neuromorphic Screens ImGui GUI v1.0\n";
    std::cout << "Stable Event-Based Screen Capture Visualization\n\n";
    
    std::string inputFile;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            show_usage();
            return 0;
        } else if (arg == "--input" && i + 1 < argc) {
            inputFile = argv[i + 1];
            i++; // Skip next argument
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            show_usage();
            return 1;
        }
    }
    
    try {
        // Create ImGui event viewer
        ImGuiEventViewer viewer;
        
        // Initialize the viewer
        if (!viewer.Initialize("Neuromorphic Event Viewer - ImGui", 1280, 720)) {
            std::cerr << "Failed to initialize ImGui event viewer" << std::endl;
            return 1;
        }
        
        std::cout << "ImGui viewer initialized successfully" << std::endl;
        
        // Load events if specified
        if (!inputFile.empty()) {
            std::cout << "Loading events from: " << inputFile << std::endl;
            if (!viewer.LoadEvents(inputFile)) {
                std::cerr << "Failed to load events from file: " << inputFile << std::endl;
                return 1;
            } else {
                std::cout << "Events loaded successfully!" << std::endl;
                std::cout << "Click the Play button for automatic video-like playback" << std::endl;
            }
        }
        
        std::cout << "GUI launched successfully!" << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "  - Stable operation (no segfaults)" << std::endl;
        std::cout << "  - Automatic playback when Play is pressed" << std::endl;
        std::cout << "  - Real-time 60 FPS rendering" << std::endl;
        std::cout << "  - Thread-safe event processing" << std::endl;
        
        // Main render loop
        while (viewer.Render()) {
            // The render loop handles all events and drawing
            // This will continue until the user closes the window
        }
        
        std::cout << "ImGui application closed successfully." << std::endl;
        
        // Cleanup is handled by the destructor
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}