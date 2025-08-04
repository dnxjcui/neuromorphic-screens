#!/usr/bin/env python3
"""
Complete test of UDP event streaming pipeline
Starts both receiver and streamer for testing
"""

import subprocess
import time
import threading
import socket
import struct
import sys
import os

def test_receiver(results, duration=10):
    """Receiver function to run in separate thread"""
    ip = "127.0.0.1"
    port = 9999
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    
    try:
        sock.bind((ip, port))
        print(f"[RECEIVER] Listening on {ip}:{port}")
    except OSError as e:
        print(f"[RECEIVER] Error binding: {e}")
        results['receiver_error'] = str(e)
        return
    
    total_packets = 0
    total_events = 0
    start_time = time.time()
    
    try:
        while time.time() - start_time < duration:
            try:
                data, addr = sock.recvfrom(4096)
                total_packets += 1
                
                if len(data) >= 8:
                    packet_timestamp = struct.unpack('<Q', data[:8])[0]
                    event_data_size = len(data) - 8
                    dvs_event_size = 8 + 2 + 2 + 1
                    num_events = event_data_size // dvs_event_size
                    total_events += num_events
                    
                    if total_packets % 50 == 0:
                        elapsed = time.time() - start_time
                        print(f"[RECEIVER] {total_packets} packets, {total_events} events ({total_events/elapsed:.0f} evt/s)")
                    
            except socket.timeout:
                continue
                
    except Exception as e:
        print(f"[RECEIVER] Error: {e}")
        results['receiver_error'] = str(e)
        
    finally:
        sock.close()
        
    results['packets'] = total_packets
    results['events'] = total_events
    results['duration'] = time.time() - start_time
    
    print(f"[RECEIVER] Finished: {total_packets} packets, {total_events} events")

def main():
    print("Comprehensive UDP Event Streaming Test")
    print("=" * 50)
    
    # Check if streamer executable exists
    streamer_path = "build\\bin\\Release\\neuromorphic_screens_udp_streamer.exe"
    if not os.path.exists(streamer_path):
        print(f"[ERROR] Streamer not found: {streamer_path}")
        print("Build it with: cmake --build build --config Release --target neuromorphic_screens_udp_streamer")
        return 1
    
    # Results dictionary for thread communication
    results = {}
    
    # Start receiver in background thread
    print("[TEST] Starting receiver thread...")
    receiver_thread = threading.Thread(target=test_receiver, args=(results, 15))
    receiver_thread.daemon = True
    receiver_thread.start()
    
    # Give receiver time to start
    time.sleep(2)
    
    # Start C++ streamer process
    print("[TEST] Starting C++ streamer...")
    try:
        streamer_cmd = [streamer_path, "--duration", "10", "--batch", "50"]
        result = subprocess.run(streamer_cmd, capture_output=True, text=True, timeout=15)
        
        print("[STREAMER] Output:")
        print(result.stdout)
        if result.stderr:
            print("[STREAMER] Errors:")
            print(result.stderr)
            
    except subprocess.TimeoutExpired:
        print("[STREAMER] Timeout - process took too long")
    except FileNotFoundError:
        print(f"[STREAMER] Executable not found: {streamer_path}")
        return 1
    except Exception as e:
        print(f"[STREAMER] Error: {e}")
        return 1
    
    # Wait for receiver thread to finish
    print("[TEST] Waiting for receiver to finish...")
    receiver_thread.join(timeout=5)
    
    # Analyze results
    print("\\n" + "=" * 50)
    print("TEST RESULTS:")
    
    if 'receiver_error' in results:
        print(f"[FAIL] Receiver error: {results['receiver_error']}")
        return 1
    
    packets = results.get('packets', 0)
    events = results.get('events', 0)
    duration = results.get('duration', 0)
    
    print(f"Packets received: {packets}")
    print(f"Events received: {events}")
    print(f"Duration: {duration:.2f} seconds")
    
    if packets > 0 and events > 0:
        print(f"Average events/sec: {events/duration:.0f}")
        print("\\n[SUCCESS] Event streaming pipeline works correctly!")
        print("\\nNext steps:")
        print("1. Install event_stream: pip install event_stream")
        print("2. Test with: python test_event_receiver.py")
        print("3. Ready for BindsNET integration!")
        return 0
    else:
        print("\\n[FAIL] No events received")
        print("Check that both processes can communicate via UDP")
        return 1

if __name__ == "__main__":
    sys.exit(main())