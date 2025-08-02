@echo off
echo Building benchmark executable...

REM Create build directory if it doesn't exist
if not exist "build" mkdir build
cd build

REM Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

REM Build the benchmark
cmake --build . --target benchmark_parallelization --config Release

REM Run the benchmark
echo.
echo Running benchmark...
echo.
bin\Release\benchmark_parallelization.exe

echo.
echo Benchmark completed!
pause 