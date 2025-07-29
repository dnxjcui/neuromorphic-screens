#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <string>
#include <mutex>

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
    constexpr size_t MAX_EVENT_CONTEXT_WINDOW = 1000000; // Maximum events in rolling buffer
}

/**
 * Individual neuromorphic event representing a pixel change
 */
struct Event {
    uint64_t timestamp;  // Microseconds since epoch
    uint16_t x, y;       // Pixel coordinates
    int8_t polarity;     // +1 for brightness increase, 0 for decrease
    
    Event() : timestamp(0), x(0), y(0), polarity(0) {}
    Event(uint64_t ts, uint16_t px, uint16_t py, int8_t pol) 
        : timestamp(ts), x(px), y(py), polarity(pol) {}
};

/**
 * Stream of events with metadata using fixed-length rolling buffer
 */
struct EventStream {
    std::deque<Event> events;
    uint32_t width, height;  // Screen dimensions
    uint64_t start_time;     // Recording start timestamp
    size_t max_events;       // Maximum events in rolling buffer
    uint64_t total_events_generated; // Total events generated (for statistics)
    mutable std::mutex events_mutex; // Thread safety for event access
    
    EventStream() : width(0), height(0), start_time(0), 
                   max_events(constants::MAX_EVENT_CONTEXT_WINDOW), 
                   total_events_generated(0) {}
    
    EventStream(size_t max_size) : width(0), height(0), start_time(0), 
                                  max_events(max_size), total_events_generated(0) {}
    
    // Thread-safe method to add events with rolling buffer behavior
    void addEvents(const std::vector<Event>& newEvents) {
        std::lock_guard<std::mutex> lock(events_mutex);
        for (const auto& event : newEvents) {
            if (events.size() >= max_events) {
                events.pop_front();
            }
            events.push_back(event);
            total_events_generated++;
        }
    }
    
    // Thread-safe method to add single event
    void addEvent(const Event& event) {
        std::lock_guard<std::mutex> lock(events_mutex);
        if (events.size() >= max_events) {
            events.pop_front();
        }
        events.push_back(event);
        total_events_generated++;
    }
    
    // Thread-safe access to events for reading
    std::vector<Event> getEventsCopy() const {
        std::lock_guard<std::mutex> lock(events_mutex);
        return std::vector<Event>(events.begin(), events.end());
    }
    
    // Thread-safe size check
    size_t size() const {
        std::lock_guard<std::mutex> lock(events_mutex);
        return events.size();
    }
    
    // File format compatibility methods
    void clear() {
        std::lock_guard<std::mutex> lock(events_mutex);
        events.clear();
        total_events_generated = 0;
    }
    
    void reserve(size_t capacity) {
        (void)capacity; // Suppress unused parameter warning
        // No-op for deque compatibility with vector-style reserve calls
    }
    
    void push_back(const Event& event) {
        addEvent(event);
    }
    
    void resize(size_t new_size) {
        std::lock_guard<std::mutex> lock(events_mutex);
        events.resize(new_size);
    }
    
    Event* data() {
        // Return pointer to first element for file I/O - WARNING: not thread-safe by design for file operations
        return events.empty() ? nullptr : &events[0];
    }
    
    const Event* data() const {
        // Return const pointer to first element for file I/O - WARNING: not thread-safe by design for file operations  
        return events.empty() ? nullptr : &events[0];
    }
    
    // Iterator access for file formats (not thread-safe by design for performance)
    auto begin() { return events.begin(); }
    auto end() { return events.end(); }
    auto begin() const { return events.begin(); }
    auto end() const { return events.end(); }
    
    // Erase methods for compatibility with vector-style operations
    template<typename Iterator>
    auto erase(Iterator first, Iterator last) {
        std::lock_guard<std::mutex> lock(events_mutex);
        return events.erase(first, last);
    }
    
    template<typename Iterator>
    auto erase(Iterator pos) {
        std::lock_guard<std::mutex> lock(events_mutex);
        return events.erase(pos);
    }
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

/**
 * Bit-packed event frame for efficient storage and streaming
 * Each pixel is represented by 1 bit: 1=brightness increase, 0=brightness decrease
 * Massive memory savings: ~259KB per 1920x1080 frame vs millions of 13-byte events
 */
struct BitPackedEventFrame {
    uint64_t timestamp;           // Frame timestamp
    uint32_t width, height;       // Frame dimensions
    std::vector<uint8_t> bitData; // Packed bits (width*height bits packed into bytes)
    
    BitPackedEventFrame() : timestamp(0), width(0), height(0) {}
    BitPackedEventFrame(uint64_t ts, uint32_t w, uint32_t h) 
        : timestamp(ts), width(w), height(h) {
        size_t bitCount = w * h;
        size_t byteCount = (bitCount + 7) / 8; // Round up to nearest byte
        bitData.resize(byteCount, 0);
    }
    
    // Set pixel bit (1=increase, 0=decrease)
    void setPixel(uint32_t x, uint32_t y, bool increase) {
        if (x >= width || y >= height) return;
        
        size_t bitIndex = y * width + x;
        size_t byteIndex = bitIndex / 8;
        size_t bitOffset = bitIndex % 8;
        
        if (increase) {
            bitData[byteIndex] |= (1 << bitOffset);
        } else {
            bitData[byteIndex] &= ~(1 << bitOffset);
        }
    }
    
    // Get pixel bit
    bool getPixel(uint32_t x, uint32_t y) const {
        if (x >= width || y >= height) return false;
        
        size_t bitIndex = y * width + x;
        size_t byteIndex = bitIndex / 8;
        size_t bitOffset = bitIndex % 8;
        
        return (bitData[byteIndex] & (1 << bitOffset)) != 0;
    }
    
    // Get storage size in bytes
    size_t getStorageSize() const {
        return sizeof(timestamp) + sizeof(width) + sizeof(height) + bitData.size();
    }
};

} // namespace neuromorphic 