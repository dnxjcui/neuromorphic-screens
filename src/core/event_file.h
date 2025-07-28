#pragma once

#include "event_types.h"
#include <string>
#include <vector>

namespace neuromorphic {

/**
 * Static utility class for event file I/O operations
 */
class EventFile {
public:
    /**
     * Write event stream to binary file
     */
    static bool WriteEvents(const EventStream& events, const std::string& filename);
    
    /**
     * Read event stream from binary file
     */
    static bool ReadEvents(EventStream& events, const std::string& filename);
    
    /**
     * Validate event file format
     */
    static bool ValidateFile(const std::string& filename);
    
    /**
     * Get file statistics without loading all events
     */
    static bool GetFileStats(const std::string& filename, EventFileHeader& header);
    
    /**
     * Sort events by timestamp
     */
    static void SortEventsByTime(EventStream& events);
    
    /**
     * Remove duplicate events (same position and time)
     */
    static void RemoveDuplicates(EventStream& events);
    
    /**
     * Filter events by time range
     */
    static void FilterByTimeRange(EventStream& events, uint64_t startTime, uint64_t endTime);
    
    /**
     * Filter events by spatial region
     */
    static void FilterByRegion(EventStream& events, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    
    /**
     * Compress event stream (remove redundant events)
     */
    static void CompressEvents(EventStream& events, float threshold = 0.1f);

private:
    /**
     * Write header to file
     */
    static bool WriteHeader(const EventFileHeader& header, FILE* file);
    
    /**
     * Read header from file
     */
    static bool ReadHeader(EventFileHeader& header, FILE* file);
    
    /**
     * Validate header magic and version
     */
    static bool ValidateHeader(const EventFileHeader& header);
};

} // namespace neuromorphic 