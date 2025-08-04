# UDP Event Streaming Test Scripts

This directory contains Python scripts for testing the UDP event streaming functionality.

## Files

- `test_event_receiver.py` - Event receiver using the event_stream library (preferred)
- `run_streaming_test.py` - Comprehensive pipeline test that runs both streamer and receiver
- `check_python_deps.py` - Dependency checker for required Python packages

## Usage

### Quick Test
```bash
python run_streaming_test.py
```

### Manual Testing
```bash
# Terminal 1: Start receiver
python test_event_receiver.py --duration 30

# Terminal 2: Start streamer
neuromorphic_screens_streaming.exe --UDP --duration 10
```

### Dependencies
Install required packages with:
```bash
pip install numpy event_stream
```

Check dependencies with:
```bash
python check_python_deps.py
```

## Integration

These scripts are designed to work with the UDP streaming functionality in:
- `neuromorphic_screens_streaming.exe --UDP`

The scripts receive real event data streamed over UDP from the screen capture system.