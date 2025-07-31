#include "src/capture/screen_capture.h"
#include "src/core/timing.h"
#include "src/core/event_types.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <omp.h>

using namespace neuromorphic;

// Test function to simulate pixel processing without actual screen capture
class PixelProcessingBenchmark {
private:
    uint32_t m_width, m_height;
    std::vector<uint8_t> m_currentFrame;
    std::vector<uint8_t> m_previousFrame;
    
public:
    PixelProcessingBenchmark(uint32_t width, uint32_t height) 
        : m_width(width), m_height(height) {
        
        // Initialize test frame buffers with some variation
        size_t bufferSize = width * height * 4; // RGBA
        m_currentFrame.resize(bufferSize);
        m_previousFrame.resize(bufferSize);
        
        // Fill with test data
        for (size_t i = 0; i < bufferSize; i += 4) {
            // Create some variation between current and previous frames
            m_currentFrame[i] = static_cast<uint8_t>(i % 255);     // B
            m_currentFrame[i+1] = static_cast<uint8_t>((i+1) % 255); // G
            m_currentFrame[i+2] = static_cast<uint8_t>((i+2) % 255); // R
            m_currentFrame[i+3] = 255; // A
            
            m_previousFrame[i] = static_cast<uint8_t>((i + 10) % 255);     // B
            m_previousFrame[i+1] = static_cast<uint8_t>((i+11) % 255); // G
            m_previousFrame[i+2] = static_cast<uint8_t>((i+12) % 255); // R
            m_previousFrame[i+3] = 255; // A
        }
    }
    
    // Serial version (original approach)
    std::vector<Event> processPixelsSerial(float threshold, uint32_t stride, size_t maxEvents) {
        std::vector<Event> events;
        events.reserve(maxEvents);
        
        uint64_t baseTime = HighResTimer::GetMicroseconds();
        
        for (uint32_t y = 0; y < m_height; y += stride) {
            for (uint32_t x = 0; x < m_width; x += stride) {
                int8_t pixelChange = calculatePixelDifference(x, y, threshold);
                
                if (pixelChange >= 0 && events.size() < maxEvents) {
                    uint64_t uniqueTimestamp = HighResTimer::GetMicroseconds();
                    uint64_t relativeTimestamp = uniqueTimestamp - baseTime;
                    
                    Event event(relativeTimestamp, static_cast<uint16_t>(x), static_cast<uint16_t>(y), pixelChange);
                    events.push_back(event);
                }
            }
        }
        
        return events;
    }
    
    // Parallel version (OpenMP approach)
    std::vector<Event> processPixelsParallel(float threshold, uint32_t stride, size_t maxEvents) {
        std::vector<Event> events;
        events.reserve(maxEvents);
        
        uint64_t baseTime = HighResTimer::GetMicroseconds();
        
        // Pre-calculate total number of rows to process
        uint32_t totalRows = (m_height + stride - 1) / stride;
        
        #pragma omp parallel
        {
            // Each thread gets its own local event vector
            std::vector<Event> localEvents;
            localEvents.reserve(maxEvents / omp_get_num_threads());
            
            // Parallel loop over rows
            #pragma omp for schedule(dynamic, 16)
            for (int row = 0; row < static_cast<int>(totalRows); ++row) {
                uint32_t y = row * stride;
                
                for (uint32_t x = 0; x < m_width; x += stride) {
                    int8_t pixelChange = calculatePixelDifference(x, y, threshold);
                    
                    if (pixelChange >= 0 && localEvents.size() < maxEvents / omp_get_num_threads()) {
                        uint64_t uniqueTimestamp = HighResTimer::GetMicroseconds();
                        uint64_t relativeTimestamp = uniqueTimestamp - baseTime;
                        
                        Event event(relativeTimestamp, static_cast<uint16_t>(x), static_cast<uint16_t>(y), pixelChange);
                        localEvents.push_back(event);
                    }
                }
            }
            
            // Critical section to merge local events into global vector
            #pragma omp critical
            {
                if (events.size() + localEvents.size() <= maxEvents) {
                    events.insert(events.end(), localEvents.begin(), localEvents.end());
                } else {
                    size_t remainingSpace = maxEvents - events.size();
                    events.insert(events.end(), localEvents.begin(), localEvents.begin() + remainingSpace);
                }
            }
        }
        
        return events;
    }
    
private:
    // Simplified pixel difference calculation (same logic as ScreenCapture)
    int8_t calculatePixelDifference(uint32_t x, uint32_t y, float threshold) {
        uint32_t pixelIndex = (y * m_width + x) * 4;
        
        if (pixelIndex + 3 >= m_currentFrame.size()) {
            return 0;
        }
        
        // Get current and previous pixel values
        uint8_t* currentPixel = &m_currentFrame[pixelIndex];
        uint8_t* previousPixel = &m_previousFrame[pixelIndex];
        
        // Calculate luminance (Y = 0.299R + 0.587G + 0.114B)
        float currentLuminance = 
            currentPixel[2] * 0.299f +  // R
            currentPixel[1] * 0.587f +  // G
            currentPixel[0] * 0.114f;   // B
        
        float previousLuminance = 
            previousPixel[2] * 0.299f +  // R
            previousPixel[1] * 0.587f +  // G
            previousPixel[0] * 0.114f;   // B
        
        // Calculate difference
        float difference = currentLuminance - previousLuminance;
        float absDifference = abs(difference);
        
        // Check if difference exceeds threshold
        if (absDifference > threshold) {
            return (difference > 0) ? 1 : 0;
        }
        
        return -1;
    }
};

void printBenchmarkResults(const std::string& testName, 
                          const std::chrono::microseconds& duration, 
                          size_t eventCount, 
                          uint32_t width, 
                          uint32_t height) {
    double durationMs = duration.count() / 1000.0;
    double pixelsPerSecond = (width * height) / (durationMs / 1000.0);
    double eventsPerSecond = eventCount / (durationMs / 1000.0);
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << testName << ":" << std::endl;
    std::cout << "  Duration: " << durationMs << " ms" << std::endl;
    std::cout << "  Events generated: " << eventCount << std::endl;
    std::cout << "  Pixels processed: " << (width * height) << std::endl;
    std::cout << "  Pixels/second: " << pixelsPerSecond << std::endl;
    std::cout << "  Events/second: " << eventsPerSecond << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Pixel Processing Parallelization Benchmark ===" << std::endl;
    std::cout << "OpenMP Threads: " << omp_get_max_threads() << std::endl;
    std::cout << std::endl;
    
    // Test different resolutions
    std::vector<std::pair<uint32_t, uint32_t>> resolutions = {
        {1920, 1080},  // Full HD
        {2560, 1440},  // 2K
        {3840, 2160},  // 4K
        {5120, 2880}   // 5K
    };
    
    for (const auto& resolution : resolutions) {
        uint32_t width = resolution.first;
        uint32_t height = resolution.second;
        
        std::cout << "Testing resolution: " << width << "x" << height << std::endl;
        std::cout << "Total pixels: " << (width * height) << std::endl;
        std::cout << std::endl;
        
        PixelProcessingBenchmark benchmark(width, height);
        
        // Test parameters
        float threshold = 15.0f;
        uint32_t stride = 1;
        size_t maxEvents = 100000;
        int numRuns = 5;
        
        // Warm up
        benchmark.processPixelsSerial(threshold, stride, maxEvents);
        benchmark.processPixelsParallel(threshold, stride, maxEvents);
        
        // Benchmark serial version
        auto serialStart = std::chrono::high_resolution_clock::now();
        std::vector<Event> serialEvents;
        for (int i = 0; i < numRuns; ++i) {
            serialEvents = benchmark.processPixelsSerial(threshold, stride, maxEvents);
        }
        auto serialEnd = std::chrono::high_resolution_clock::now();
        auto serialDuration = std::chrono::duration_cast<std::chrono::microseconds>(serialEnd - serialStart);
        
        // Benchmark parallel version
        auto parallelStart = std::chrono::high_resolution_clock::now();
        std::vector<Event> parallelEvents;
        for (int i = 0; i < numRuns; ++i) {
            parallelEvents = benchmark.processPixelsParallel(threshold, stride, maxEvents);
        }
        auto parallelEnd = std::chrono::high_resolution_clock::now();
        auto parallelDuration = std::chrono::duration_cast<std::chrono::microseconds>(parallelEnd - parallelStart);
        
        // Calculate averages
        auto avgSerialDuration = std::chrono::microseconds(serialDuration.count() / numRuns);
        auto avgParallelDuration = std::chrono::microseconds(parallelDuration.count() / numRuns);
        
        // Print results
        printBenchmarkResults("Serial Processing", avgSerialDuration, serialEvents.size(), width, height);
        printBenchmarkResults("Parallel Processing", avgParallelDuration, parallelEvents.size(), width, height);
        
        // Calculate speedup
        double speedup = static_cast<double>(avgSerialDuration.count()) / avgParallelDuration.count();
        std::cout << "Speedup: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        std::cout << "Efficiency: " << (speedup / omp_get_max_threads() * 100) << "%" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << std::endl;
    }
    
    return 0;
} 