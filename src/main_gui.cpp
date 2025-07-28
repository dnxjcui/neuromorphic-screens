#include "visualization/event_viewer.h"
#include "core/event_file.h"
#include "core/event_file_formats.h"
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <iostream>
#include <string>

using namespace neuromorphic;

void show_usage() {
    std::cout << "Neuromorphic Screens GUI - Event-Based Screen Capture Visualization\n\n";
    std::cout << "Usage:\n";
    std::cout << "  neuromorphic_screens_gui [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --input <file>     Load events from file at startup\n";
    std::cout << "  --help             Show this help message\n\n";
    std::cout << "Controls:\n";
    std::cout << "  Play/Pause/Stop    Control event replay\n";
    std::cout << "  Speed Slider       Adjust playback speed (0.1x to 5.0x)\n";
    std::cout << "  Progress Slider    Seek to specific time in recording\n";
    std::cout << "  Statistics Panel   View event metrics and statistics\n\n";
    std::cout << "Event Visualization:\n";
    std::cout << "  Green dots = Positive events (brightness increase)\n";
    std::cout << "  Red dots = Negative events (brightness decrease)\n";
    std::cout << "  Dots fade over time for transient visualization\n\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Neuromorphic Screens GUI v1.0\n";
    std::cout << "Event-Based Screen Capture Visualization\n\n";
    
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
    
    // Initialize FLTK
    Fl::scheme("gtk+");
    
    try {
        // Create event viewer window - let it size itself
        EventViewer* viewer = new EventViewer(100, 100, 840, 450, "Neuromorphic Event Viewer");
        
        // Load events if specified
        if (!inputFile.empty()) {
            std::cout << "Loading events from: " << inputFile << std::endl;
            if (!viewer->LoadEvents(inputFile)) {
                fl_alert("Failed to load events from file: %s", inputFile.c_str());
            } else {
                std::cout << "Successfully loaded events!" << std::endl;
            }
        }
        
        // Show the window
        viewer->show();
        
        std::cout << "GUI launched successfully!" << std::endl;
        std::cout << "Use the controls to replay events or load a new file." << std::endl;
        
        // Run FLTK event loop
        int result = Fl::run();
        
        std::cout << "GUI application closed." << std::endl;
        
        // Clean up
        delete viewer;
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        fl_alert("Fatal error: %s", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        fl_alert("Unknown fatal error occurred");
        return 1;
    }
}