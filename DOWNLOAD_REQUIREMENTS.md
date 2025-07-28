# Download Requirements for Neuromorphic Screens

## Overview

This document lists all the software, libraries, and tools required to compile and run the neuromorphic screens project on Windows 11.

## Essential Development Tools

### 1. Visual Studio 2019/2022 (Required)

**Download Link**: https://visualstudio.microsoft.com/downloads/

**Required Components**:
- **Visual Studio Community** (Free) or Professional/Enterprise
- **C++ Development Tools** workload
- **Windows 10/11 SDK** (latest version)
- **CMake Tools for Visual Studio** (optional but recommended)

**Installation Steps**:
1. Download Visual Studio Installer
2. Run installer and select "C++ Development Tools"
3. Ensure "Windows 10/11 SDK" is included
4. Install (may take 30-60 minutes)

**Verification**:
```bash
# Open Developer Command Prompt and run:
cl
# Should show Microsoft C/C++ compiler version
```

### 2. CMake (Required)

**Download Link**: https://cmake.org/download/

**Version**: 3.16 or later

**Installation Options**:
- **Option A**: Download installer from cmake.org
- **Option B**: Install via Visual Studio (recommended)
- **Option C**: Install via Chocolatey: `choco install cmake`

**Verification**:
```bash
cmake --version
# Should show version 3.16 or later
```

### 3. Git (Required)

**Download Link**: https://git-scm.com/download/win

**Installation**:
- Download and run installer
- Use default settings
- Ensure "Git Bash" is included

**Verification**:
```bash
git --version
# Should show Git version
```

## Required Libraries

### 4. Windows SDK (Included with Visual Studio)

**Status**: Automatically installed with Visual Studio

**Components**:
- DirectX 11 headers and libraries
- Windows API headers
- High-resolution timing functions

**Verification**:
```bash
# Check if DirectX headers are available
dir "C:\Program Files (x86)\Windows Kits\10\Include"
```

### 5. NVIDIA Capture SDK (Optional - Requires NDA)

**Download Link**: https://developer.nvidia.com/capture-sdk

**Requirements**:
- NVIDIA Developer account
- NDA agreement signed
- NVIDIA GPU (RTX 3050 or better)

**Installation**:
1. Sign up for NVIDIA Developer account
2. Request access to Capture SDK
3. Download after approval
4. Extract to project's `include/nvfbc/` directory

**Alternative**: If NVFBC is not available, the system will automatically fall back to Desktop Duplication API.

### 6. FLTK (For GUI Version - Future)

**Download Link**: https://www.fltk.org/software.php

**Version**: 1.3.x or 1.4.x

**Installation Options**:

**Option A: Pre-built Binaries**
- Download Windows pre-built package
- Extract to `C:\fltk\`
- Add to PATH: `C:\fltk\bin`

**Option B: Build from Source**
```bash
# Download source
git clone https://github.com/fltk/fltk.git
cd fltk

# Build with Visual Studio
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

**Option C: vcpkg (Recommended)**
```bash
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install FLTK
.\vcpkg install fltk:x64-windows
```

## Optional Tools

### 7. Chocolatey (Package Manager - Recommended)

**Download Link**: https://chocolatey.org/install

**Installation**:
```powershell
# Run PowerShell as Administrator
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
```

**Useful Packages**:
```bash
choco install cmake
choco install git
choco install visualstudio2019buildtools
choco install ninja
```

### 8. Ninja (Fast Build System - Optional)

**Download Link**: https://github.com/ninja-build/ninja/releases

**Installation**:
- Download ninja-win.zip
- Extract ninja.exe to a directory in PATH
- Or install via Chocolatey: `choco install ninja`

### 9. Doxygen (Documentation - Optional)

**Download Link**: https://www.doxygen.nl/download.html

**Purpose**: Generate code documentation

**Installation**:
- Download Windows installer
- Run installer
- Add to PATH if needed

## System Requirements

### Hardware Requirements

**Minimum**:
- **CPU**: Intel Core i5 or AMD equivalent
- **RAM**: 8GB
- **GPU**: Any DirectX 11 compatible GPU
- **Storage**: 10GB free space

**Recommended**:
- **CPU**: Intel Core i7 or AMD equivalent
- **RAM**: 16GB
- **GPU**: NVIDIA RTX 3050 or better (for NVFBC)
- **Storage**: 20GB free space

### Software Requirements

**Operating System**:
- Windows 11 (64-bit)
- Windows 10 (64-bit) - version 1903 or later

**Graphics Drivers**:
- Latest NVIDIA drivers (for NVFBC support)
- Latest Intel/AMD drivers (for Desktop Duplication fallback)

## Installation Checklist

### Pre-Installation
- [ ] Ensure Windows 11 is up to date
- [ ] Install latest graphics drivers
- [ ] Disable antivirus temporarily (if needed for development)

### Core Tools
- [ ] Install Visual Studio 2019/2022 with C++ tools
- [ ] Install CMake (3.16+)
- [ ] Install Git
- [ ] Verify all tools are in PATH

### Libraries
- [ ] Verify Windows SDK is installed
- [ ] Download NVIDIA Capture SDK (if available)
- [ ] Install FLTK (for GUI version)

### Verification
- [ ] Test Visual Studio compilation
- [ ] Test CMake configuration
- [ ] Test Git operations
- [ ] Verify DirectX headers are available

## Build Environment Setup

### 1. Environment Variables

**Set in System Properties**:
```
PATH += C:\Program Files\CMake\bin
PATH += C:\Program Files\Git\bin
PATH += C:\fltk\bin (if using FLTK)
```

### 2. Visual Studio Configuration

**Required Workloads**:
- C++ Development Tools
- Windows 10/11 SDK
- CMake Tools for Visual Studio

**Required Individual Components**:
- MSVC v142 - VS 2019 C++ x64/x86 build tools
- Windows 10/11 SDK
- CMake Tools for Visual Studio

### 3. Project-Specific Setup

**Directory Structure**:
```
neuromorphic_screens/
├── include/
│   └── nvfbc/          # NVIDIA SDK headers (if available)
├── lib/
│   └── nvfbc.lib       # NVIDIA SDK library (if available)
├── src/
├── CMakeLists.txt
└── README.md
```

## Troubleshooting

### Common Issues

**1. CMake Not Found**
```bash
# Solution: Add CMake to PATH
set PATH=%PATH%;C:\Program Files\CMake\bin
```

**2. Visual Studio Not Found**
```bash
# Solution: Use Developer Command Prompt
# Or set environment variables:
set VS160COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\
```

**3. DirectX Headers Not Found**
```bash
# Solution: Install Windows SDK
# Or specify SDK version in CMakeLists.txt:
set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
```

**4. FLTK Not Found**
```bash
# Solution: Set FLTK_DIR environment variable
set FLTK_DIR=C:\fltk
# Or use vcpkg:
set CMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

**5. NVIDIA SDK Not Available**
- **Impact**: System will use Desktop Duplication API fallback
- **Solution**: No action needed, fallback is automatic
- **Verification**: Check console output for "Using Desktop Duplication capture"

### Performance Issues

**1. Slow Compilation**
- Use Ninja build system
- Enable parallel compilation: `cmake --build . --parallel`
- Use SSD storage

**2. High Memory Usage**
- Close other applications during build
- Use Release configuration for testing
- Monitor Task Manager during compilation

## Verification Commands

### Test Installation
```bash
# Test Visual Studio
cl /?

# Test CMake
cmake --version

# Test Git
git --version

# Test DirectX headers
dir "C:\Program Files (x86)\Windows Kits\10\Include\10.0.*\um\d3d11.h"

# Test FLTK (if installed)
fltk-config --version
```

### Test Project Build
```bash
# Clone project
git clone <repository-url>
cd neuromorphic_screens

# Configure with CMake
mkdir build
cd build
cmake ..

# Build project
cmake --build . --config Release

# Test executable
.\bin\Release\neuromorphic_screens.exe --help
```

## Alternative Installation Methods

### Using vcpkg (Recommended for Libraries)

**Install vcpkg**:
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

**Install Required Packages**:
```bash
.\vcpkg install cmake:x64-windows
.\vcpkg install fltk:x64-windows
.\vcpkg integrate install
```

**Use with CMake**:
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### Using Conan (Alternative Package Manager)

**Install Conan**:
```bash
pip install conan
```

**Create conanfile.txt**:
```
[requires]
fltk/1.4.0

[generators]
cmake
```

**Install Dependencies**:
```bash
conan install . --build=missing
```

## Summary

### Required Downloads (Essential)
1. **Visual Studio 2019/2022** - C++ development environment
2. **CMake 3.16+** - Build system
3. **Git** - Version control
4. **Windows SDK** - DirectX and Windows APIs

### Optional Downloads (Enhanced Features)
1. **NVIDIA Capture SDK** - High-performance screen capture
2. **FLTK** - GUI framework
3. **Chocolatey** - Package manager
4. **Ninja** - Fast build system
5. **Doxygen** - Documentation generator

### Estimated Download Sizes
- Visual Studio: ~3-8GB (depending on components)
- CMake: ~50MB
- Git: ~300MB
- FLTK: ~10MB
- NVIDIA SDK: ~100MB (if available)
- **Total**: ~4-9GB

### Estimated Installation Time
- **Fast Internet (100+ Mbps)**: 30-60 minutes
- **Slow Internet (10-50 Mbps)**: 2-4 hours
- **Installation and Configuration**: 1-2 hours
- **Total Setup Time**: 1.5-6 hours

## Next Steps After Installation

1. **Clone the repository**
2. **Run the test build**: `cmake --build . --config Release`
3. **Test basic functionality**: `.\bin\Release\neuromorphic_screens.exe --test`
4. **Generate test data**: `.\bin\Release\neuromorphic_screens.exe --generate-test-data --output test.evt`
5. **Replay test data**: `.\bin\Release\neuromorphic_screens.exe --replay --input test.evt`

## Support

If you encounter issues during installation:

1. **Check the troubleshooting section above**
2. **Verify system requirements**
3. **Ensure all tools are properly installed**
4. **Check PATH environment variables**
5. **Try alternative installation methods**
6. **Consult project documentation**

The project is designed to work with minimal dependencies and includes fallback mechanisms for optional components. 