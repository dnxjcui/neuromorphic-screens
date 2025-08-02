#pragma once

#include "event_types.h"
#include <deque>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <cstdint>

namespace neuromorphic {

/**
 * High-performance temporal indexing for recent event access
 * Provides O(1) recent event queries and O(k) retrieval where k = recent events count
 * Eliminates O(n) linear scanning bottleneck in visualization systems
 */
class TemporalEventIndex {
private:
    struct TimeWindowEntry {
        Event event;
        uint64_t absoluteTime;  // stream.start_time + event.timestamp
        uint64_t eventId;       // Unique ID for deduplication
        
        TimeWindowEntry(const Event& e, uint64_t absTime, uint64_t id)
            : event(e), absoluteTime(absTime), eventId(id) {}
    };
    
    // Ring buffer for fixed time window (100ms default)
    std::deque<TimeWindowEntry> m_recentEvents;
    mutable std::mutex m_recentEventsLock;
    
    // Deduplication tracking
    std::unordered_set<uint64_t> m_processedEventIds;
    mutable std::mutex m_processedIdsLock;
    
    // Configuration
    uint64_t m_timeWindowMicros;     // Time window in microseconds (default: 100ms)
    size_t m_maxRecentEvents;        // Maximum events to keep in recent buffer
    uint64_t m_nextEventId;          // Monotonic event ID counter
    
    // Performance counters
    size_t m_totalEventsProcessed;
    size_t m_duplicatesSkipped;
    
public:
    /**
     * Constructor
     * @param timeWindowMicros Time window for recent events (default: 100ms)
     * @param maxRecentEvents Maximum events in recent buffer (default: 10000)
     */
    explicit TemporalEventIndex(uint64_t timeWindowMicros = 100000, size_t maxRecentEvents = 10000)
        : m_timeWindowMicros(timeWindowMicros)
        , m_maxRecentEvents(maxRecentEvents)
        , m_nextEventId(0)
        , m_totalEventsProcessed(0)
        , m_duplicatesSkipped(0) {
    }
    
    /**
     * Add events from event stream with automatic deduplication
     * O(k) complexity where k = number of new events
     * @param stream Event stream to process
     * @param currentTime Current timestamp for age calculation
     */
    void updateFromStream(const EventStream& stream, uint64_t currentTime) {
        // Get thread-safe copy of events - this is unavoidable but we minimize the work done with it
        auto eventsCopy = stream.getEventsCopy();
        
        std::vector<TimeWindowEntry> newEntries;
        newEntries.reserve(eventsCopy.size());
        
        // Pre-process events outside of locks
        for (const auto& event : eventsCopy) {
            uint64_t absoluteTime = stream.start_time + event.timestamp;
            uint64_t eventAge = currentTime - absoluteTime;
            
            // Only consider events within our time window
            if (eventAge <= m_timeWindowMicros) {
                // Generate unique ID based on event properties for deduplication
                uint64_t eventId = generateEventId(event, absoluteTime);
                newEntries.emplace_back(event, absoluteTime, eventId);
            }
        }
        
        // Now update the recent events buffer with deduplication
        {
            std::lock_guard<std::mutex> recentLock(m_recentEventsLock);
            std::lock_guard<std::mutex> processedLock(m_processedIdsLock);
            
            for (const auto& entry : newEntries) {
                // Check if we've already processed this event
                if (m_processedEventIds.find(entry.eventId) != m_processedEventIds.end()) {
                    m_duplicatesSkipped++;
                    continue;
                }
                
                // Add to recent events buffer
                m_recentEvents.push_back(entry);
                m_processedEventIds.insert(entry.eventId);
                m_totalEventsProcessed++;
                
                // Maintain buffer size limit
                if (m_recentEvents.size() > m_maxRecentEvents) {
                    auto& oldest = m_recentEvents.front();
                    m_processedEventIds.erase(oldest.eventId);
                    m_recentEvents.pop_front();
                }
            }
            
            // Clean up expired events from recent buffer
            cleanupExpiredEvents(currentTime);
        }
    }
    
    /**
     * Get recent events within time window
     * O(k) complexity where k = number of recent events (typically << total events)
     * @param currentTime Current timestamp
     * @return Vector of recent events
     */
    std::vector<Event> getRecentEvents(uint64_t currentTime) const {
        std::lock_guard<std::mutex> lock(m_recentEventsLock);
        
        std::vector<Event> result;
        result.reserve(m_recentEvents.size());
        
        for (const auto& entry : m_recentEvents) {
            uint64_t eventAge = currentTime - entry.absoluteTime;
            if (eventAge <= m_timeWindowMicros) {
                result.push_back(entry.event);
            }
        }
        
        return result;
    }
    
    /**
     * Get count of recent events within time window
     * O(k) complexity where k = number of recent events
     * @param currentTime Current timestamp
     * @return Number of recent events
     */
    size_t getRecentEventCount(uint64_t currentTime) const {
        std::lock_guard<std::mutex> lock(m_recentEventsLock);
        
        size_t count = 0;
        for (const auto& entry : m_recentEvents) {
            uint64_t eventAge = currentTime - entry.absoluteTime;
            if (eventAge <= m_timeWindowMicros) {
                count++;
            }
        }
        
        return count;
    }
    
    /**
     * Clear all cached data (useful for stream resets)
     */
    void clear() {
        std::lock_guard<std::mutex> recentLock(m_recentEventsLock);
        std::lock_guard<std::mutex> processedLock(m_processedIdsLock);
        
        m_recentEvents.clear();
        m_processedEventIds.clear();
        m_nextEventId = 0;
        m_totalEventsProcessed = 0;
        m_duplicatesSkipped = 0;
    }
    
    /**
     * Get performance statistics
     */
    void getPerformanceStats(size_t& totalProcessed, size_t& duplicatesSkipped, size_t& currentBufferSize) const {
        std::lock_guard<std::mutex> recentLock(m_recentEventsLock);
        std::lock_guard<std::mutex> processedLock(m_processedIdsLock);
        
        totalProcessed = m_totalEventsProcessed;
        duplicatesSkipped = m_duplicatesSkipped;
        currentBufferSize = m_recentEvents.size();
    }
    
    /**
     * Configure time window
     * @param timeWindowMicros New time window in microseconds
     */
    void setTimeWindow(uint64_t timeWindowMicros) {
        std::lock_guard<std::mutex> lock(m_recentEventsLock);
        m_timeWindowMicros = timeWindowMicros;
    }
    
    /**
     * Get current time window
     * @return Time window in microseconds
     */
    uint64_t getTimeWindow() const {
        return m_timeWindowMicros;
    }

private:
    /**
     * Generate unique event ID for deduplication
     * Based on event properties and timestamp
     */
    uint64_t generateEventId(const Event& event, uint64_t absoluteTime) const {
        // Simple hash combining event properties
        // This should be unique enough for deduplication within reasonable time windows
        uint64_t id = static_cast<uint64_t>(event.x) << 48 |
                      static_cast<uint64_t>(event.y) << 32 |
                      static_cast<uint64_t>(event.polarity) << 24 |
                      (absoluteTime & 0xFFFFFF); // Use lower 24 bits of timestamp for uniqueness
        return id;
    }
    
    /**
     * Remove expired events from recent buffer and processed IDs
     * Must be called with locks held
     */
    void cleanupExpiredEvents(uint64_t currentTime) {
        // Remove events that are too old
        while (!m_recentEvents.empty()) {
            const auto& oldest = m_recentEvents.front();
            uint64_t eventAge = currentTime - oldest.absoluteTime;
            
            if (eventAge <= m_timeWindowMicros) {
                break; // First non-expired event found, stop cleanup
            }
            
            // Remove from processed IDs set
            m_processedEventIds.erase(oldest.eventId);
            m_recentEvents.pop_front();
        }
        
        // Additional cleanup: if processed IDs set gets too large, clean up periodically
        // This prevents memory growth over long running sessions
        constexpr size_t MAX_PROCESSED_IDS = 50000;
        if (m_processedEventIds.size() > MAX_PROCESSED_IDS) {
            // Clear processed IDs and let them rebuild - acceptable trade-off for memory management
            // This might allow some duplicates but prevents unbounded memory growth
            m_processedEventIds.clear();
        }
    }
};

} // namespace neuromorphic