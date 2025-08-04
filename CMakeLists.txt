cmake_minimum_required(VERSION 3.16)
project(neuromorphic_screens VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenMP for parallelization
find_package(OpenMP REQUIRED)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found, enabling parallelization")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Base source files (common to all applications)
set(BASE_SOURCES
    src/core/timing.cpp
    src/core/event_file.cpp
    src/core/event_file_formats.cpp
    src/capture/screen_capture.cpp
)

# Source files for CLI application
set(CLI_SOURCES
    src/main.cpp
    ${BASE_SOURCES}
)

# Source files for ImGui GUI application
set(IMGUI_GUI_SOURCES
    src/main_imgui.cpp
    src/visualization/imgui_event_viewer.cpp
    ${BASE_SOURCES}
)

# Source files for Streaming application
set(STREAMING_SOURCES
    src/main_streaming.cpp
    src/streaming_app.cpp
    src/visualization/imgui_streaming_viewer.cpp
    ${BASE_SOURCES}
)

# Source files for Direct Overlay application
set(OVERLAY_SOURCES
    src/main_overlay.cpp
    src/streaming_app.cpp
    src/visualization/direct_overlay_viewer.cpp
    ${BASE_SOURCES}
)

# Source files for UDP Event Streamer application
set(UDP_STREAMER_SOURCES
    src/main_udp_streamer.cpp
    src/streaming_app.cpp
    src/streaming/udp_event_streamer.cpp
    ${BASE_SOURCES}
)

# Create CLI executable
add_executable(neuromorphic_screens ${CLI_SOURCES})

# Link libraries for CLI
target_link_libraries(neuromorphic_screens
    d3d11
    dxgi
    d3dcompiler
    OpenMP::OpenMP_CXX
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(neuromorphic_screens
        user32 gdi32 shell32 ole32 uuid comctl32 advapi32
    )
endif()

# ImGui configuration for stable GUI
if(WIN32)
    # Set ImGui paths - using the Program Files location
    set(IMGUI_DIR "C:/Program Files/imgui")
    
    # Check if ImGui exists at the expected location
    if(EXISTS "${IMGUI_DIR}/imgui.cpp")
        message(STATUS "Found ImGui in C:/Program Files/imgui")
        
        # ImGui source files
        set(IMGUI_SOURCES
            "${IMGUI_DIR}/imgui.cpp"
            "${IMGUI_DIR}/imgui_demo.cpp"
            "${IMGUI_DIR}/imgui_draw.cpp"
            "${IMGUI_DIR}/imgui_tables.cpp"
            "${IMGUI_DIR}/imgui_widgets.cpp"
            "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
            "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
        )
        
        # Create ImGui GUI executable
        add_executable(neuromorphic_screens_imgui ${IMGUI_GUI_SOURCES} ${IMGUI_SOURCES})
        
        # Create Streaming application executable
        add_executable(neuromorphic_screens_streaming ${STREAMING_SOURCES} ${IMGUI_SOURCES})
        
        # Include ImGui directories
        target_include_directories(neuromorphic_screens_imgui PRIVATE 
            "${IMGUI_DIR}"
            "${IMGUI_DIR}/backends"
        )
        
        target_include_directories(neuromorphic_screens_streaming PRIVATE 
            "${IMGUI_DIR}"
            "${IMGUI_DIR}/backends"
        )
        
        # Link ImGui libraries with DirectX 11 backend
        target_link_libraries(neuromorphic_screens_imgui
            d3d11
            dxgi
            d3dcompiler
            OpenMP::OpenMP_CXX
            user32 gdi32 shell32 ole32 uuid comctl32 advapi32
        )
        
        target_link_libraries(neuromorphic_screens_streaming
            d3d11
            dxgi
            d3dcompiler
            OpenMP::OpenMP_CXX
            user32 gdi32 shell32 ole32 uuid comctl32 advapi32
        )
        
        message(STATUS "ImGui GUI applications configured successfully")
        message(STATUS "ImGui path: ${IMGUI_DIR}")
    else()
        message(WARNING "ImGui not found at C:/Program Files/imgui/")
        message(STATUS "Please ensure ImGui is installed at the expected location")
    endif()
endif()

# Create Direct Overlay application executable (now needs ImGui for controls)
if(WIN32 AND EXISTS "${IMGUI_DIR}/imgui.cpp")
    add_executable(neuromorphic_screens_overlay ${OVERLAY_SOURCES} ${IMGUI_SOURCES})
    
    # Include ImGui directories for overlay
    target_include_directories(neuromorphic_screens_overlay PRIVATE 
        "${IMGUI_DIR}"
        "${IMGUI_DIR}/backends"
    )
    
    # Link libraries for overlay application
    target_link_libraries(neuromorphic_screens_overlay
        d3d11
        dxgi
        d3dcompiler
        d2d1
        dwrite
        dwmapi
        OpenMP::OpenMP_CXX
        user32 gdi32 shell32 ole32 uuid comctl32 advapi32
    )
else()
    message(WARNING "Cannot build overlay application without ImGui")
endif()

# Create UDP Event Streamer executable
add_executable(neuromorphic_screens_udp_streamer ${UDP_STREAMER_SOURCES})

# Link libraries for UDP streamer
target_link_libraries(neuromorphic_screens_udp_streamer
    d3d11
    dxgi
    d3dcompiler
    OpenMP::OpenMP_CXX
)

# Windows-specific libraries and socket support
if(WIN32)
    target_link_libraries(neuromorphic_screens_udp_streamer
        user32 gdi32 shell32 ole32 uuid comctl32 advapi32
        ws2_32  # Winsock2 for UDP sockets
    )
else()
    # Linux/Unix socket support is built into libc
endif()

# Add benchmark executable
add_executable(benchmark_parallelization benchmark_parallelization.cpp src/core/timing.cpp)

# Link libraries for benchmark
target_link_libraries(benchmark_parallelization
    OpenMP::OpenMP_CXX
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(benchmark_parallelization
        user32 gdi32 shell32 ole32 uuid comctl32 advapi32
    )
endif()

# Summary of what will be built
message(STATUS "========== Build Summary ==========")
message(STATUS "CLI Application: neuromorphic_screens")
message(STATUS "UDP Event Streamer: neuromorphic_screens_udp_streamer")
message(STATUS "Benchmark Application: benchmark_parallelization")
message(STATUS "Overlay Application: neuromorphic_screens_overlay")
if(WIN32 AND EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(STATUS "ImGui GUI Application: neuromorphic_screens_imgui")
    message(STATUS "Streaming Application: neuromorphic_screens_streaming")
endif()
message(STATUS "======================================")