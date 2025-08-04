@echo off
echo Testing UDP Event Streaming Pipeline
echo ====================================
echo.

REM Check if the UDP streamer executable exists
if not exist "build\bin\Release\neuromorphic_screens_udp_streamer.exe" (
    echo ERROR: UDP streamer not found. Please build first with:
    echo cmake --build build --config Release --target neuromorphic_screens_udp_streamer
    pause
    exit /b 1
)

echo Starting Python receiver in background...
start /B python test_streaming_simple.py --duration 15

echo Waiting 2 seconds for receiver to start...
timeout /t 2 /nobreak > nul

echo.
echo Starting C++ UDP event streamer for 10 seconds...
echo This will stream simulated events to the Python receiver.
echo.

REM Run the C++ streamer for 10 seconds
timeout /t 10 | build\bin\Release\neuromorphic_screens_udp_streamer.exe --duration 10

echo.
echo Test completed. Check the output above for results.
pause