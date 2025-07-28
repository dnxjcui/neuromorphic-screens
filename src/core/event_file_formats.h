#pragma once

#include "event_types.h"
#include <string>
#include <fstream>

namespace neuromorphic {

/**
 * Supported event file formats
 */
enum class EventFileFormat {
    BINARY_NEVS,    // Original binary .evt format
    TEXT_CSV,       // CSV format with header
    TEXT_SPACE      // Space-separated format (rpg_dvs_ros compatible)
};

/**
 * Event file format utilities
 */
class EventFileFormats {
public:
    /**
     * Detect file format from extension or content
     */
    static EventFileFormat DetectFormat(const std::string& filename);
    
    /**
     * Get recommended file extension for format
     */
    static std::string GetExtension(EventFileFormat format);
    
    /**
     * Write events in specified format
     */
    static bool WriteEvents(const EventStream& events, const std::string& filename, EventFileFormat format);
    
    /**
     * Read events from file (auto-detect format)
     */
    static bool ReadEvents(EventStream& events, const std::string& filename);
    
private:
    // Format-specific implementations
    static bool WriteCSV(const EventStream& events, const std::string& filename);
    static bool WriteSpaceSeparated(const EventStream& events, const std::string& filename);
    static bool ReadCSV(EventStream& events, const std::string& filename);
    static bool ReadSpaceSeparated(EventStream& events, const std::string& filename);
    
    // Utility functions
    static bool IsCSVFormat(const std::string& filename);
    static bool IsSpaceFormat(const std::string& filename);
    static std::string GetCurrentDateTime();
};

} // namespace neuromorphic