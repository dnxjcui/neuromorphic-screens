#pragma once

#include "../core/event_types.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <memory>
#include <vector>

namespace neuromorphic {

/**
 * Screen capture class using Desktop Duplication API
 */
class ScreenCapture {
private:
    // DirectX resources
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    IDXGIOutputDuplication* m_deskDupl;
    ID3D11Texture2D* m_previousFrame;
    ID3D11Texture2D* m_currentFrame;
    
    // Frame buffers for pixel-by-pixel comparison
    uint8_t* m_currentFrameBuffer;
    uint8_t* m_previousFrameBuffer;
    uint32_t m_frameBufferSize;
    uint32_t m_width, m_height;
    
    // State
    bool m_initialized;
    bool m_captureActive;
    bool m_firstFrame;
    
    // Event generation
    float m_changeThreshold;
    uint32_t m_pixelThreshold; // Threshold for pixel change detection
    

public:
    ScreenCapture();
    ~ScreenCapture();
    
    /**
     * Initialize capture system
     */
    bool Initialize();
    
    /**
     * Start capture session
     */
    bool StartCapture();
    
    /**
     * Stop capture session
     */
    void StopCapture();
    
    /**
     * Capture a frame and generate events
     */
    bool CaptureFrame(EventStream& events, uint64_t timestamp, float threshold = 15.0f, uint32_t stride = 1);
    
    /**
     * Capture a frame and generate bit-packed representation (much more efficient)
     */
    bool CaptureFrameBitPacked(BitPackedEventFrame& frame, uint64_t timestamp, float threshold = 15.0f, uint32_t stride = 1);
    
    /**
     * Get screen dimensions
     */
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    
    /**
     * Check if capture is active
     */
    bool IsCapturing() const { return m_captureActive; }
    
    /**
     * Set change threshold for event generation (0.0 to 1.0)
     */
    void SetChangeThreshold(float threshold) { 
        m_changeThreshold = threshold; 
        m_pixelThreshold = static_cast<uint32_t>(threshold * 255.0f);
    }
    
    /**
     * Get change threshold
     */
    float GetChangeThreshold() const { return m_changeThreshold; }
    

private:
    /**
     * Initialize DirectX device and context
     */
    bool InitializeDirectX();
    
    /**
     * Initialize Desktop Duplication
     */
    bool InitializeDesktopDuplication();
    
    /**
     * Capture frame using Desktop Duplication
     */
    bool CaptureFrameDesktopDuplication();
    
    /**
     * Generate events from pixel differences
     */
    void GenerateEventsFromFrame(EventStream& events, uint64_t timestamp, float threshold, uint32_t stride);
    
    /**
     * Compare pixels between current and previous frames
     */
    void ComparePixels(EventStream& events, uint64_t timestamp, float threshold, uint32_t stride);
    
    /**
     * Calculate pixel difference and determine polarity
     */
    int8_t CalculatePixelDifference(uint32_t x, uint32_t y, const float & sensitiveThreshold);
    
    /**
     * Clean up DirectX resources
     */
    void CleanupDirectX();
    
    /**
     * Clean up Desktop Duplication resources
     */
    void CleanupDesktopDuplication();
};

} // namespace neuromorphic 