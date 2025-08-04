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
    src/core/streaming_app.cpp
    src/capture/screen_capture.cpp
)

# Source files for CLI application
set(CLI_SOURCES
    src/main.cpp
    ${BASE_SOURCES}
)

# Source files for Streaming application (consolidated)
set(STREAMING_SOURCES
    src/main_streaming.cpp
    src/visualization/imgui_streaming_viewer.cpp
    src/visualization/direct_overlay_viewer.cpp
    src/streaming/udp_event_streamer.cpp
    ${BASE_SOURCES}
)

# Create CLI executable (with optional ImGui support)
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

# ImGui configuration for GUI support
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
        
        # Add ImGui support to CLI executable for --gui flag
        target_sources(neuromorphic_screens PRIVATE 
            ${IMGUI_SOURCES}
            src/visualization/imgui_event_viewer.cpp
        )
        
        # Create consolidated Streaming application executable
        add_executable(neuromorphic_screens_streaming ${STREAMING_SOURCES} ${IMGUI_SOURCES})
        
        # Include ImGui directories for both executables
        target_include_directories(neuromorphic_screens PRIVATE 
            "${IMGUI_DIR}"
            "${IMGUI_DIR}/backends"
        )
        
        target_include_directories(neuromorphic_screens_streaming PRIVATE 
            "${IMGUI_DIR}"
            "${IMGUI_DIR}/backends"
        )
        
        # Link ImGui libraries for streaming application
        target_link_libraries(neuromorphic_screens_streaming
            d3d11
            dxgi
            d3dcompiler
            d2d1
            dwrite
            dwmapi
            ws2_32
            OpenMP::OpenMP_CXX
            user32 gdi32 shell32 ole32 uuid comctl32 advapi32
        )
        
        message(STATUS "ImGui GUI support configured successfully")
        message(STATUS "ImGui path: ${IMGUI_DIR}")
    else()
        message(WARNING "ImGui not found at C:/Program Files/imgui/")
        message(STATUS "GUI features will be disabled")
    endif()
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
message(STATUS "CLI Application: neuromorphic_screens (includes --gui support)")
message(STATUS "Benchmark Application: benchmark_parallelization")
if(WIN32 AND EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(STATUS "Streaming Application: neuromorphic_screens_streaming (consolidated)")
    message(STATUS "  - Default mode: Real-time streaming with GUI")
    message(STATUS "  - --overlay: Direct screen overlay")
    message(STATUS "  - --UDP: UDP event streaming with visualization options")
endif()
message(STATUS "======================================")