#include <iostream>
#include <string>
#include <vector>

#include "streaming_app.h"
#include "visualization/imgui_streaming_viewer.h"

using namespace neuromorphic;

/**
 * Command-line argument parser for streaming application
 */
class StreamingCommandLineParser {
private:
    std::vector<std::string> m_args;
    
public:
    StreamingCommandLineParser(int argc, char* argv[]) {
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
        std::cout << "Neuromorphic Screens - Real-Time Event Streaming\\n";
        std::cout << "Usage:\\n";
        std::cout << "  --stream [--save <filename>] [--format <aedat|csv>]\\n";
        std::cout << "  --help\\n";
        std::cout << "\\nOptions:\\n";
        std::cout << "  --save     Optional: Save captured events to file\\n";
        std::cout << "  --format   File format for saving (default: aedat)\\n";
        std::cout << "\\nSupported formats:\\n";
        std::cout << "  aedat      AEDAT binary format (recommended)\\n";
        std::cout << "  csv        CSV text format with headers\\n";
    }
};


int main(int argc, char* argv[]) {
    std::cout << "Neuromorphic Screens - Real-Time Streaming v1.0\\n";
    std::cout << "Event-Based Screen Capture with Live Visualization\\n\\n";
    
    StreamingCommandLineParser parser(argc, argv);
    
    if (parser.hasFlag("--help") || argc == 1) {
        parser.printUsage();
        return 0;
    }
    
    try {
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
        }
        
        // Initialize streaming
        if (!streamingApp.initialize()) {
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
        
        // Run GUI main loop
        viewer.Run();
        
        // Cleanup
        streamingApp.stopStreaming();
        viewer.Cleanup();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
    
    return 0;
}