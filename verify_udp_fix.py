#!/usr/bin/env python3
"""
Verify that UDP streaming fix works with both receiver types
"""
import subprocess
import time
import threading
import sys
from pathlib import Path

def run_fallback_test():
    """Test the fallback UDP receiver"""
    print("=== Testing Fallback UDP Receiver ===")
    receiver_cmd = [
        sys.executable, 
        "src/test_UDP/test_event_receiver.py", 
        "--fallback", 
        "--duration", "8"
    ]
    
    streamer_cmd = [
        "build/bin/Release/neuromorphic_screens_streaming.exe",
        "--UDP", "--novis", "--duration", "6"
    ]
    
    # Start receiver
    print("Starting fallback receiver...")
    receiver_proc = subprocess.Popen(receiver_cmd, 
                                   stdout=subprocess.PIPE, 
                                   stderr=subprocess.STDOUT,
                                   text=True)
    
    # Wait 2 seconds, then start streamer
    time.sleep(2)
    print("Starting C++ UDP streamer...")
    streamer_proc = subprocess.run(streamer_cmd, 
                                 capture_output=True, 
                                 text=True)
    
    # Get receiver output
    receiver_output, _ = receiver_proc.communicate()
    
    print("C++ Streamer output:")
    print(streamer_proc.stdout)
    if streamer_proc.stderr:
        print("C++ Streamer errors:")
        print(streamer_proc.stderr)
    
    print("\nFallback Receiver output:")
    print(receiver_output)
    
    # Check if receiver got packets
    if "Packets: 0" in receiver_output:
        print("FAIL: Fallback receiver got 0 packets")
        return False
    else:
        print("PASS: Fallback receiver got packets")
        return True

def run_eventstream_test():
    """Test the event_stream UDP receiver"""
    print("\n=== Testing Event Stream UDP Receiver ===")
    receiver_cmd = [
        sys.executable, 
        "src/test_UDP/test_event_receiver.py", 
        "--duration", "8"
    ]
    
    streamer_cmd = [
        "build/bin/Release/neuromorphic_screens_streaming.exe",
        "--UDP", "--novis", "--duration", "6"
    ]
    
    # Start receiver
    print("Starting event_stream receiver...")
    receiver_proc = subprocess.Popen(receiver_cmd, 
                                   stdout=subprocess.PIPE, 
                                   stderr=subprocess.STDOUT,
                                   text=True)
    
    # Wait 2 seconds, then start streamer
    time.sleep(2)
    print("Starting C++ UDP streamer...")
    streamer_proc = subprocess.run(streamer_cmd, 
                                 capture_output=True, 
                                 text=True)
    
    # Get receiver output
    receiver_output, _ = receiver_proc.communicate()
    
    print("C++ Streamer output:")
    print(streamer_proc.stdout)
    if streamer_proc.stderr:
        print("C++ Streamer errors:")
        print(streamer_proc.stderr)
    
    print("\nEvent Stream Receiver output:")
    print(receiver_output)
    
    # Check if receiver got events (not just packets)
    if "Total events: 0" in receiver_output:
        print("FAIL: Event stream receiver got 0 events")
        return False
    else:
        print("PASS: Event stream receiver got events")
        return True

def main():
    print("UDP Streaming Fix Verification")
    print("=" * 40)
    
    # Change to project directory
    project_root = Path(__file__).parent
    import os
    os.chdir(project_root)
    
    fallback_pass = run_fallback_test()
    eventstream_pass = run_eventstream_test()
    
    print("\n" + "=" * 40)
    print("FINAL RESULTS:")
    print(f"Fallback receiver: {'PASS' if fallback_pass else 'FAIL'}")
    print(f"Event stream receiver: {'PASS' if eventstream_pass else 'FAIL'}")
    
    if fallback_pass and eventstream_pass:
        print("SUCCESS: UDP streaming fix verified!")
        return 0
    else:
        print("ISSUES: Some receivers failed")
        return 1

if __name__ == "__main__":
    sys.exit(main())