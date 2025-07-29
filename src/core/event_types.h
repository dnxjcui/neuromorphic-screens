#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace neuromorphic {

/**
 * Constants for the neuromorphic screen capture system
 */
namespace constants {
    constexpr uint32_t BLOCK_SIZE = 16;           // 16x16 pixel blocks for difference detection
    constexpr uint32_t DOT_SIZE = 2;              // 2x2 pixel dots for visualization
    constexpr float DOT_FADE_DURATION = 0.1f;     // 100ms fade duration for transient effects
    constexpr float REPLAY_FPS = 60.0f;           // 60 FPS target for replay
    constexpr uint64_t FRAME_TIMEOUT_MS = 16;     // 16ms frame timeout
    constexpr uint32_t MAX_EVENTS_PER_FRAME = 10000; // Safety limit for events per frame
}

/**
 * Individual neuromorphic event representing a pixel change
 */
struct Event {
    uint64_t timestamp;  // Microseconds since epoch
    uint16_t x, y;       // Pixel coordinates
    int8_t polarity;     // +1 for brightness increase, -1 for decrease
    
    Event() : timestamp(0), x(0), y(0), polarity(0) {}
    Event(uint64_t ts, uint16_t px, uint16_t py, int8_t pol) 
        : timestamp(ts), x(px), y(py), polarity(pol) {}
};

/**
 * Stream of events with metadata
 */
struct EventStream {
    std::vector<Event> events;
    uint32_t width, height;  // Screen dimensions
    uint64_t start_time;     // Recording start timestamp
    
    EventStream() : width(0), height(0), start_time(0) {}
};

/**
 * Binary file header for NEVS (.evt) files
 */
struct EventFileHeader {
    char magic[4];        // "NEVS" magic number
    uint32_t version;     // File format version (1)
    uint32_t width;       // Screen width
    uint32_t height;      // Screen height
    uint64_t start_time;  // Recording start timestamp
    uint32_t event_count; // Number of events
    
    EventFileHeader() {
        magic[0] = 'N'; magic[1] = 'E'; magic[2] = 'V'; magic[3] = 'S';
        version = 1;
        width = height = 0;
        start_time = 0;
        event_count = 0;
    }
};

/**
 * Statistics for event stream analysis
 */
struct EventStats {
    uint32_t total_events;
    uint32_t positive_events;
    uint32_t negative_events;
    uint64_t duration_us;
    float events_per_second;
    
    EventStats() : total_events(0), positive_events(0), negative_events(0), 
                   duration_us(0), events_per_second(0.0f) {}
    
    void calculate(const EventStream& stream);
};

/**
 * AEDAT event structure for binary file format
 */
#pragma pack(push, 1)
struct AEDATEvent {
    uint32_t timestamp;  // in microseconds
    uint16_t x, y;       // pixel coordinates
    uint8_t polarity;    // +1 for brightness increase, -1 for decrease (stored as 0/1)
};

/**
 * AEDAT file header
 */
struct AEDATHeader {
    char magic[4];       // "AEDT"
    uint32_t version;    // 1
    uint32_t width;      // screen width
    uint32_t height;     // screen height
    uint64_t start_time; // recording start timestamp
    uint32_t event_count; // number of events
};
#pragma pack(pop)

} // namespace neuromorphic 