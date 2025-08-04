#include "screen_capture.h"
#include "../core/timing.h"
#include <iostream>
#include <algorithm>
#include <cstring> // For memset
#include <omp.h>   // For OpenMP parallelization

namespace neuromorphic {

ScreenCapture::ScreenCapture() 
    : m_device(nullptr)
    , m_context(nullptr)
    , m_deskDupl(nullptr)
    , m_previousFrame(nullptr)
    , m_currentFrame(nullptr)
    , m_currentFrameBuffer(nullptr)
    , m_previousFrameBuffer(nullptr)
    , m_frameBufferSize(0)
    , m_width(0)
    , m_height(0)
    , m_initialized(false)
    , m_captureActive(false)
    , m_firstFrame(true)
    , m_changeThreshold(0.15f)  // 15% threshold - sensitive enough for mouse movement
    , m_pixelThreshold(38) // 38/255 ≈ 15%
    , verbose(false) {    
}

ScreenCapture::~ScreenCapture() {
    StopCapture();
    CleanupDesktopDuplication();
    CleanupDirectX();
}

bool ScreenCapture::Initialize() {
    // Get screen dimensions
    m_width = GetSystemMetrics(SM_CXSCREEN);
    m_height = GetSystemMetrics(SM_CYSCREEN);
    
    std::cout << "Screen capture initialized: " << m_width << "x" << m_height << std::endl;
    
    // Calculate frame buffer size (RGBA = 4 bytes per pixel)
    m_frameBufferSize = m_width * m_height * 4;
    
    // Allocate frame buffers
    m_currentFrameBuffer = new uint8_t[m_frameBufferSize];
    m_previousFrameBuffer = new uint8_t[m_frameBufferSize];
    
    if (!m_currentFrameBuffer || !m_previousFrameBuffer) {
        std::cerr << "Failed to allocate frame buffers" << std::endl;
        return false;
    }
    
    // Initialize DirectX
    if (!InitializeDirectX()) {
        return false;
    }
    
    // Initialize Desktop Duplication
    if (!InitializeDesktopDuplication()) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

bool ScreenCapture::StartCapture() {
    if (!m_initialized) {
        return false;
    }
    
    m_captureActive = true;
    m_firstFrame = true;
    
    // Initialize previous frame buffer with zeros
    memset(m_previousFrameBuffer, 0, m_frameBufferSize);
    
    std::cout << "Starting screen capture for " << m_width << "x" << m_height << " pixels" << std::endl;
    std::cout << "Move your mouse or open/close windows to generate events" << std::endl;
    std::cout << "Capture sensitivity threshold: 15.0 (lower = more sensitive)" << std::endl;
    
    return true;
}

void ScreenCapture::StopCapture() {
    m_captureActive = false;
    std::cout << "Screen capture stopped" << std::endl;
}

bool ScreenCapture::CaptureFrame(EventStream& events, uint64_t timestamp, float threshold, uint32_t stride, size_t maxEvents) {
    if (!m_captureActive || !m_initialized) {
        return false;
    }
    
    // Capture frame using Desktop Duplication
    if (!CaptureFrameDesktopDuplication()) {
        return false;
    }
    
    // Generate events from pixel differences with dynamic parameters
    GenerateEventsFromFrame(events, timestamp, threshold, stride, maxEvents);
    
    return true;
}

bool ScreenCapture::InitializeDirectX() {
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel;
    
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
        &m_device, &featureLevel, &m_context
    );
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device" << std::endl;
        return false;
    }
    
    return true;
}

bool ScreenCapture::InitializeDesktopDuplication() {
    // Get DXGI adapter and output
    IDXGIDevice* dxgiDevice = nullptr;
    HRESULT hr = m_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI device" << std::endl;
        return false;
    }
    
    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter));
    dxgiDevice->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI adapter" << std::endl;
        return false;
    }
    
    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    dxgiAdapter->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI output" << std::endl;
        return false;
    }
    
    IDXGIOutput1* dxgiOutput1 = nullptr;
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
    dxgiOutput->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI output 1" << std::endl;
        return false;
    }
    
    // Create desktop duplication
    hr = dxgiOutput1->DuplicateOutput(m_device, &m_deskDupl);
    dxgiOutput1->Release();
    if (FAILED(hr)) {
        std::cerr << "Failed to create desktop duplication" << std::endl;
        return false;
    }
    
    // Create textures for frame capture
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = m_width;
    textureDesc.Height = m_height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_previousFrame);
    if (FAILED(hr)) {
        std::cerr << "Failed to create previous frame texture" << std::endl;
        return false;
    }
    
    hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_currentFrame);
    if (FAILED(hr)) {
        std::cerr << "Failed to create current frame texture" << std::endl;
        return false;
    }
    
    return true;
}

bool ScreenCapture::CaptureFrameDesktopDuplication() {
    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    
    HRESULT hr = m_deskDupl->AcquireNextFrame(constants::FRAME_TIMEOUT_MS, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // No new frame available, this is normal
            return false;
        } else if (hr == DXGI_ERROR_ACCESS_LOST) {
            std::cerr << "Desktop duplication access lost - reinitializing..." << std::endl;
            CleanupDesktopDuplication();
            if (!InitializeDesktopDuplication()) {
                std::cerr << "Failed to reinitialize desktop duplication" << std::endl;
            }
            return false;
        } else {
            std::cerr << "Desktop duplication frame acquisition failed with HRESULT: 0x" 
                      << std::hex << hr << std::dec << std::endl;
        }
        return false;
    }
    
    // Check if cursor shape or position changed (enables cursor capture)
    if (frameInfo.PointerPosition.Visible || frameInfo.PointerShapeBufferSize > 0) {
        // Cursor is visible and may have moved - this ensures cursor changes are captured
    }
    
    // Get the desktop texture
    ID3D11Texture2D* desktopTexture = nullptr;
    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&desktopTexture));
    desktopResource->Release();
    if (FAILED(hr)) {
        return false;
    }
    
    // Copy to staging texture for CPU access
    m_context->CopyResource(m_currentFrame, desktopTexture);
    desktopTexture->Release();
    
    // Map the texture to get pixel data
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_context->Map(m_currentFrame, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        return false;
    }
    
    // Copy pixel data to current frame buffer
    uint8_t* srcData = static_cast<uint8_t*>(mappedResource.pData);
    for (uint32_t y = 0; y < m_height; y++) {
        memcpy(m_currentFrameBuffer + y * m_width * 4, 
               srcData + y * mappedResource.RowPitch, 
               m_width * 4);
    }
    
    m_context->Unmap(m_currentFrame, 0);
    m_deskDupl->ReleaseFrame();
    
    return true;
}

bool ScreenCapture::CaptureFrameBitPacked(BitPackedEventFrame& frame, uint64_t timestamp, float threshold, uint32_t stride) {
    if (!m_captureActive || !m_initialized) {
        return false;
    }
    
    // Capture frame using Desktop Duplication
    if (!CaptureFrameDesktopDuplication()) {
        return false;
    }
    
    // Initialize bit-packed frame
    frame = BitPackedEventFrame(timestamp, m_width, m_height);
    
    // Skip first frame (no previous frame to compare against)
    if (m_firstFrame) {
        memcpy(m_previousFrameBuffer, m_currentFrameBuffer, m_frameBufferSize);
        m_firstFrame = false;
        return true; // Return empty frame
    }
    
    // Generate bit-packed representation (optimized)
    // Use the provided threshold and stride parameters
    
    // Use smaller chunk size for better load balancing
    #pragma omp parallel for schedule(dynamic, 32)
    for (int y = 0; y < static_cast<int>(m_height); y += stride) {
        for (uint32_t x = 0; x < m_width; x += stride) {
            int8_t pixelChange = CalculatePixelDifference(x, static_cast<uint32_t>(y), threshold);
            
            if (pixelChange >= 0) { // 0 or 1 (decrease or increase)
                frame.setPixel(x, y, pixelChange == 1);
            }
        }
    }
    
    // Copy current frame to previous frame buffer for next comparison
    memcpy(m_previousFrameBuffer, m_currentFrameBuffer, m_frameBufferSize);
    
    return true;
}

void ScreenCapture::GenerateEventsFromFrame(EventStream& events, uint64_t timestamp, float threshold, uint32_t stride, size_t maxEvents) {
    // Skip first frame (no previous frame to compare against)
    if (m_firstFrame) {
        // Copy current frame to previous frame buffer for next comparison (fast copy)
        memcpy(m_previousFrameBuffer, m_currentFrameBuffer, m_frameBufferSize);
        m_firstFrame = false;
        return;
    }
    
    // Compare pixels between current and previous frames
    ComparePixels(events, timestamp, threshold, stride, maxEvents);
    
    // Copy current frame to previous frame buffer for next comparison (simple memcpy is often faster)
    memcpy(m_previousFrameBuffer, m_currentFrameBuffer, m_frameBufferSize);
}

void ScreenCapture::ComparePixels(EventStream& events, uint64_t timestamp, float threshold, uint32_t stride, size_t maxEvents) {
    // Use pixel-by-pixel comparison for accurate DVS-style event generation
    // Generate events only for pixels that actually changed significantly
    
    const size_t maxEventsPerFrame = maxEvents; // Use the provided max events parameter
    // Use the provided threshold and stride parameters instead of hardcoded values
    
    // Parallelized approach using OpenMP
    std::vector<Event> frameEvents;
    frameEvents.reserve(maxEventsPerFrame);
    
    // Pre-calculate total number of pixels to process
    uint32_t totalRows = (m_height + stride - 1) / stride;
    uint32_t totalCols = (m_width + stride - 1) / stride;
    
    // Use OpenMP for parallel processing
    #pragma omp parallel
    {
        // Each thread gets its own local event vector
        std::vector<Event> localEvents;
        localEvents.reserve(maxEventsPerFrame / omp_get_num_threads());
        
        // Parallel loop over rows
        #pragma omp for schedule(dynamic, 16) // Dynamic scheduling with chunk size 16
        for (int row = 0; row < static_cast<int>(totalRows); ++row) {
            uint32_t y = row * stride;
            
            for (uint32_t x = 0; x < m_width; x += stride) {
                int8_t pixelChange = CalculatePixelDifference(x, y, threshold);
                
                if (pixelChange >= 0 && localEvents.size() < maxEventsPerFrame / omp_get_num_threads()) {
                    // Generate unique timestamp for each event
                    uint64_t uniqueTimestamp = HighResTimer::GetMicroseconds();
                    uint64_t relativeTimestamp = uniqueTimestamp - events.start_time;
                    
                    Event event(relativeTimestamp, static_cast<uint16_t>(x), static_cast<uint16_t>(y), pixelChange);
                    localEvents.push_back(event);
                }
            }
        }
        
        // Critical section to merge local events into global vector
        #pragma omp critical
        {
            if (frameEvents.size() + localEvents.size() <= maxEventsPerFrame) {
                frameEvents.insert(frameEvents.end(), localEvents.begin(), localEvents.end());
            } else {
                // If we would exceed maxEventsPerFrame, only add what fits
                size_t remainingSpace = maxEventsPerFrame - frameEvents.size();
                frameEvents.insert(frameEvents.end(), localEvents.begin(), localEvents.begin() + remainingSpace);
            }
        }
    }
    
    // Add events if we found any
    if (!frameEvents.empty()) {
        events.addEvents(frameEvents);
    }
    
    // Debug output to see if events are generated
    if (frameEvents.size() > 0 && this->verbose) {
        std::cout << "\nGenerated " << frameEvents.size() << " events";
    }
}

int8_t ScreenCapture::CalculatePixelDifference(uint32_t x, uint32_t y, const float & sensitiveThreshold) {
    uint32_t pixelIndex = (y * m_width + x) * 4; // RGBA = 4 bytes per pixel
    
    // Bounds checking
    if (pixelIndex + 3 >= m_frameBufferSize) {
        return 0;
    }
    
    // Get current and previous pixel values (using luminance for comparison)
    uint8_t* currentPixel = m_currentFrameBuffer + pixelIndex;
    uint8_t* previousPixel = m_previousFrameBuffer + pixelIndex;
    
    // Calculate luminance (Y = 0.299R + 0.587G + 0.114B)
    // Note: BGRA format, so indices are [B, G, R, A]
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
    if (absDifference > sensitiveThreshold) {
        // Determine polarity based on luminance change
        return (difference > 0) ? 1 : 0;
    }
    
    return -1; // No significant change (will be filtered out)
}

void ScreenCapture::CleanupDirectX() {
    if (m_context) {
        m_context->Release();
        m_context = nullptr;
    }
    
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
}

void ScreenCapture::CleanupDesktopDuplication() {
    if (m_currentFrame) {
        m_currentFrame->Release();
        m_currentFrame = nullptr;
    }
    
    if (m_previousFrame) {
        m_previousFrame->Release();
        m_previousFrame = nullptr;
    }
    
    if (m_deskDupl) {
        m_deskDupl->Release();
        m_deskDupl = nullptr;
    }
    
    if (m_currentFrameBuffer) {
        delete[] m_currentFrameBuffer;
        m_currentFrameBuffer = nullptr;
    }
    
    if (m_previousFrameBuffer) {
        delete[] m_previousFrameBuffer;
        m_previousFrameBuffer = nullptr;
    }
}

} // namespace neuromorphic 