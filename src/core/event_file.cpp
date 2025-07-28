#include "event_file.h"
#include "timing.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace neuromorphic {

void EventStats::calculate(const EventStream& stream) {
    total_events = static_cast<uint32_t>(stream.events.size());
    positive_events = 0;
    negative_events = 0;
    
    for (const auto& event : stream.events) {
        if (event.polarity > 0) {
            positive_events++;
        } else if (event.polarity < 0) {
            negative_events++;
        }
    }
    
    if (total_events > 0) {
        duration_us = stream.events.back().timestamp - stream.events.front().timestamp;
        events_per_second = static_cast<float>(total_events) * 1000000.0f / static_cast<float>(duration_us);
    } else {
        duration_us = 0;
        events_per_second = 0.0f;
    }
}

bool EventFile::WriteEvents(const EventStream& events, const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    // Create header
    EventFileHeader header;
    header.width = events.width;
    header.height = events.height;
    header.start_time = events.start_time;
    header.event_count = static_cast<uint32_t>(events.events.size());
    
    // Write header
    if (!WriteHeader(header, file)) {
        fclose(file);
        return false;
    }
    
    // Write events
    size_t written = fwrite(events.events.data(), sizeof(Event), events.events.size(), file);
    fclose(file);
    
    if (written != events.events.size()) {
        std::cerr << "Failed to write all events to file" << std::endl;
        return false;
    }
    
    return true;
}

bool EventFile::ReadEvents(EventStream& events, const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }
    
    // Read header
    EventFileHeader header;
    if (!ReadHeader(header, file)) {
        fclose(file);
        return false;
    }
    
    // Validate header
    if (!ValidateHeader(header)) {
        fclose(file);
        return false;
    }
    
    // Set stream metadata
    events.width = header.width;
    events.height = header.height;
    events.start_time = header.start_time;
    
    // Read events
    events.events.resize(header.event_count);
    size_t read = fread(events.events.data(), sizeof(Event), header.event_count, file);
    fclose(file);
    
    if (read != header.event_count) {
        std::cerr << "Failed to read all events from file" << std::endl;
        return false;
    }
    
    return true;
}

bool EventFile::ValidateFile(const std::string& filename) {
    EventFileHeader header;
    return GetFileStats(filename, header);
}

bool EventFile::GetFileStats(const std::string& filename, EventFileHeader& header) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        return false;
    }
    
    bool success = ReadHeader(header, file);
    fclose(file);
    
    if (success) {
        success = ValidateHeader(header);
    }
    
    return success;
}

void EventFile::SortEventsByTime(EventStream& events) {
    std::sort(events.events.begin(), events.events.end(), 
              [](const Event& a, const Event& b) {
                  return a.timestamp < b.timestamp;
              });
}

void EventFile::RemoveDuplicates(EventStream& events) {
    auto it = std::unique(events.events.begin(), events.events.end(),
                         [](const Event& a, const Event& b) {
                             return a.timestamp == b.timestamp && 
                                    a.x == b.x && a.y == b.y;
                         });
    events.events.erase(it, events.events.end());
}

void EventFile::FilterByTimeRange(EventStream& events, uint64_t startTime, uint64_t endTime) {
    events.events.erase(
        std::remove_if(events.events.begin(), events.events.end(),
                      [startTime, endTime](const Event& event) {
                          return event.timestamp < startTime || event.timestamp > endTime;
                      }),
        events.events.end()
    );
}

void EventFile::FilterByRegion(EventStream& events, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    events.events.erase(
        std::remove_if(events.events.begin(), events.events.end(),
                      [x1, y1, x2, y2](const Event& event) {
                          return event.x < x1 || event.x > x2 || 
                                 event.y < y1 || event.y > y2;
                      }),
        events.events.end()
    );
}

void EventFile::CompressEvents(EventStream& events, float threshold) {
    if (events.events.size() < 2) return;
    
    std::vector<Event> compressed;
    compressed.reserve(events.events.size());
    
    compressed.push_back(events.events[0]);
    
    for (size_t i = 1; i < events.events.size(); i++) {
        const Event& prev = compressed.back();
        const Event& curr = events.events[i];
        
        uint64_t timeDiff = curr.timestamp - prev.timestamp;
        uint16_t xDiff = (curr.x > prev.x) ? (curr.x - prev.x) : (prev.x - curr.x);
        uint16_t yDiff = (curr.y > prev.y) ? (curr.y - prev.y) : (prev.y - curr.y);
        
        // Keep event if it's significantly different in time or position
        if (timeDiff > static_cast<uint64_t>(threshold * 1000000.0f) || 
            xDiff > static_cast<uint16_t>(threshold * 100.0f) || 
            yDiff > static_cast<uint16_t>(threshold * 100.0f)) {
            compressed.push_back(curr);
        }
    }
    
    events.events = std::move(compressed);
}

bool EventFile::WriteHeader(const EventFileHeader& header, FILE* file) {
    return fwrite(&header, sizeof(EventFileHeader), 1, file) == 1;
}

bool EventFile::ReadHeader(EventFileHeader& header, FILE* file) {
    return fread(&header, sizeof(EventFileHeader), 1, file) == 1;
}

bool EventFile::ValidateHeader(const EventFileHeader& header) {
    // Check magic number
    if (header.magic[0] != 'N' || header.magic[1] != 'E' || 
        header.magic[2] != 'V' || header.magic[3] != 'S') {
        std::cerr << "Invalid file format: wrong magic number" << std::endl;
        return false;
    }
    
    // Check version
    if (header.version != 1) {
        std::cerr << "Unsupported file version: " << header.version << std::endl;
        return false;
    }
    
    // Check dimensions
    if (header.width == 0 || header.height == 0) {
        std::cerr << "Invalid dimensions: " << header.width << "x" << header.height << std::endl;
        return false;
    }
    
    return true;
}

} // namespace neuromorphic 