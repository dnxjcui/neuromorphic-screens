#include "event_file_formats.h"
#include "event_file.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <ctime>

namespace neuromorphic {

EventFileFormat EventFileFormats::DetectFormat(const std::string& filename) {
    // Check extension first
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "csv") {
        return EventFileFormat::TEXT_CSV;
    } else if (ext == "txt") {
        // Check content to distinguish between CSV and space-separated
        if (IsCSVFormat(filename)) {
            return EventFileFormat::TEXT_CSV;
        } else {
            return EventFileFormat::TEXT_SPACE;
        }
    } else if (ext == "aedat") {
        return EventFileFormat::BINARY_AEDAT;
    }
    
    // Default to AEDAT binary for unknown extensions
    return EventFileFormat::BINARY_AEDAT;
}

std::string EventFileFormats::GetExtension(EventFileFormat format) {
    switch (format) {
        case EventFileFormat::BINARY_AEDAT: return "aedat";
        case EventFileFormat::TEXT_CSV: return "csv";
        case EventFileFormat::TEXT_SPACE: return "txt";
        default: return "aedat";
    }
}

bool EventFileFormats::WriteEvents(const EventStream& events, const std::string& filename, EventFileFormat format) {
    switch (format) {
        case EventFileFormat::BINARY_AEDAT:
            return WriteAEDAT(events, filename);
        case EventFileFormat::TEXT_CSV:
            return WriteCSV(events, filename);
        case EventFileFormat::TEXT_SPACE:
            return WriteSpaceSeparated(events, filename);
        default:
            std::cerr << "Unsupported format for writing" << std::endl;
            return false;
    }
}

bool EventFileFormats::ReadEvents(EventStream& events, const std::string& filename) {
    EventFileFormat format = DetectFormat(filename);
    
    switch (format) {
        case EventFileFormat::BINARY_AEDAT:
            return ReadAEDAT(events, filename);
        case EventFileFormat::TEXT_CSV:
            return ReadCSV(events, filename);
        case EventFileFormat::TEXT_SPACE:
            return ReadSpaceSeparated(events, filename);
        default:
            std::cerr << "Unsupported format for reading" << std::endl;
            return false;
    }
}

bool EventFileFormats::WriteCSV(const EventStream& events, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    // Write CSV header with metadata
    file << "# Neuromorphic Screen Events - CSV Format" << std::endl;
    file << "# Generated: " << GetCurrentDateTime() << std::endl;
    file << "# Screen resolution: " << events.width << "x" << events.height << std::endl;
    file << "# Start time: " << events.start_time << " (microseconds)" << std::endl;
    file << "# Event count: " << events.size() << std::endl;
    file << "timestamp,x,y,polarity" << std::endl;
    
    // Write events
    for (const auto& event : events) {
        file << event.timestamp << "," 
             << event.x << "," 
             << event.y << "," 
             << static_cast<int>(event.polarity) << std::endl;
    }
    
    file.close();
    return true;
}

bool EventFileFormats::WriteSpaceSeparated(const EventStream& events, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    // Write header comment with metadata (rpg_dvs_ros compatible)
    file << "# Neuromorphic Screen Events - Space-separated format (rpg_dvs_ros compatible)" << std::endl;
    file << "# Format: x y polarity timestamp_microseconds" << std::endl;
    file << "# Screen resolution: " << events.width << "x" << events.height << std::endl;
    file << "# Start time: " << events.start_time << " microseconds" << std::endl;
    file << "# Event count: " << events.size() << std::endl;
    
    // Write events in rpg_dvs_ros format: x y polarity timestamp
    for (const auto& event : events) {
        file << event.x << " " 
             << event.y << " " 
             << static_cast<int>(event.polarity) << " " 
             << event.timestamp << std::endl;
    }
    
    file.close();
    return true;
}

bool EventFileFormats::ReadCSV(EventStream& events, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }
    
    events.clear();
    std::string line;
    bool headerParsed = false;
    
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            // Try to parse metadata from comments
            if (line.find("# Screen resolution:") != std::string::npos) {
                std::size_t xPos = line.find(" ");
                if (xPos != std::string::npos) {
                    sscanf(line.c_str(), "# Screen resolution: %ux%u", &events.width, &events.height);
                }
            } else if (line.find("# Start time:") != std::string::npos) {
                // Parse but don't use - timestamps in CSV are already relative
                uint64_t originalStartTime;
                sscanf(line.c_str(), "# Start time: %llu", &originalStartTime);
                events.start_time = 0; // Set to 0 since CSV timestamps are already relative
            }
            continue;
        }
        
        // Skip header row
        if (!headerParsed && line.find("timestamp") != std::string::npos) {
            headerParsed = true;
            continue;
        }
        
        // Parse event data
        std::stringstream ss(line);
        std::string cell;
        Event event;
        int field = 0;
        
        while (std::getline(ss, cell, ',')) {
            switch (field) {
                case 0: event.timestamp = std::stoull(cell); break;
                case 1: event.x = static_cast<uint16_t>(std::stoi(cell)); break;
                case 2: event.y = static_cast<uint16_t>(std::stoi(cell)); break;
                case 3: event.polarity = static_cast<int8_t>(std::stoi(cell)); break;
            }
            field++;
        }
        
        if (field == 4) {
            events.push_back(event);
        }
    }
    
    file.close();
    return true;
}

bool EventFileFormats::ReadSpaceSeparated(EventStream& events, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }
    
    events.clear();
    std::string line;
    
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            // Try to parse metadata from comments
            if (line.find("# Screen resolution:") != std::string::npos) {
                sscanf(line.c_str(), "# Screen resolution: %ux%u", &events.width, &events.height);
            } else if (line.find("# Start time:") != std::string::npos) {
                // Parse but don't use - timestamps in CSV are already relative
                uint64_t originalStartTime;
                sscanf(line.c_str(), "# Start time: %llu", &originalStartTime);
                events.start_time = 0; // Set to 0 since CSV timestamps are already relative
            }
            continue;
        }
        
        // Parse event data: x y polarity timestamp
        std::stringstream ss(line);
        Event event;
        int polarity_temp;
        
        if (ss >> event.x >> event.y >> polarity_temp >> event.timestamp) {
            event.polarity = static_cast<int8_t>(polarity_temp);
            events.push_back(event);
        }
    }
    
    file.close();
    return true;
}

bool EventFileFormats::IsCSVFormat(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        // Check if first data line contains commas
        if (line.find(',') != std::string::npos) {
            file.close();
            return true;
        }
        break;
    }
    
    file.close();
    return false;
}

bool EventFileFormats::IsSpaceFormat(const std::string& filename) {
    return !IsCSVFormat(filename);
}

bool EventFileFormats::IsAEDATFormat(const std::string& filename) {
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "aedat";
}

bool EventFileFormats::WriteAEDAT(const EventStream& events, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filename << std::endl;
        return false;
    }
    
    // Write AEDAT header
    AEDATHeader header;
    header.magic[0] = 'A'; header.magic[1] = 'E'; header.magic[2] = 'D'; header.magic[3] = 'T';
    header.version = 1;
    header.width = events.width;
    header.height = events.height;
    header.start_time = events.start_time;
    header.event_count = static_cast<uint32_t>(events.size());
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write events in AEDAT format
    for (const auto& event : events) {
        AEDATEvent ae;
        ae.timestamp = static_cast<uint32_t>(event.timestamp);
        ae.x = event.x;
        ae.y = event.y;
        ae.polarity = (event.polarity > 0) ? 1 : 0; // Convert -1/+1 to 0/1
        
        file.write(reinterpret_cast<const char*>(&ae), sizeof(ae));
    }
    
    file.close();
    std::cout << "AEDAT file written successfully: " << filename << std::endl;
    std::cout << "Events: " << events.size() << ", Size: " << 
                 (sizeof(AEDATHeader) + events.size() * sizeof(AEDATEvent)) << " bytes" << std::endl;
    return true;
}

bool EventFileFormats::ReadAEDAT(EventStream& events, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open AEDAT file for reading: " << filename << std::endl;
        return false;
    }
    
    // Read and validate header
    AEDATHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (file.gcount() != sizeof(header)) {
        std::cerr << "Error: Could not read AEDAT header" << std::endl;
        return false;
    }
    
    // Validate magic number
    if (header.magic[0] != 'A' || header.magic[1] != 'E' || 
        header.magic[2] != 'D' || header.magic[3] != 'T') {
        std::cerr << "Error: Invalid AEDAT magic number" << std::endl;
        return false;
    }
    
    // Validate version
    if (header.version != 1) {
        std::cerr << "Error: Unsupported AEDAT version: " << header.version << std::endl;
        return false;
    }
    
    // Set event stream metadata
    events.width = header.width;
    events.height = header.height;
    events.start_time = header.start_time;
    events.clear();
    events.reserve(header.event_count);
    
    // Read events
    for (uint32_t i = 0; i < header.event_count; i++) {
        AEDATEvent ae;
        file.read(reinterpret_cast<char*>(&ae), sizeof(ae));
        
        if (file.gcount() != sizeof(ae)) {
            std::cerr << "Warning: Could not read event " << i << ", stopping" << std::endl;
            break;
        }
        
        Event event;
        event.timestamp = ae.timestamp;
        event.x = ae.x;
        event.y = ae.y;
        event.polarity = (ae.polarity == 1) ? 1 : -1; // Convert 0/1 to -1/+1
        
        events.events.push_back(event);
    }
    
    file.close();
    std::cout << "AEDAT file read successfully: " << filename << std::endl;
    std::cout << "Events loaded: " << events.size() << std::endl;
    return true;
}

std::string EventFileFormats::GetCurrentDateTime() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct);
    return buf;
}

} // namespace neuromorphic