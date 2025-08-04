#!/usr/bin/env python3
"""
Test the UDP event streaming pipeline
Simple test that uses the fallback socket receiver to test the C++ UDP streamer
"""

import socket
import struct
import time
import threading
import argparse
import sys

class SimpleUDPReceiver:
    """Simple UDP receiver for testing the C++ streamer without event_stream dependency"""
    
    def __init__(self, ip="127.0.0.1", port=9999):
        self.ip = ip
        self.port = port
        self.running = False
        self.total_packets = 0
        self.total_events = 0
        self.start_time = 0
        
    def start(self, duration=30):
        """Start receiving packets for the specified duration"""
        print(f"Starting UDP receiver on {self.ip}:{self.port}")
        print(f"Listening for {duration} seconds...")
        print("Start the C++ streamer with: neuromorphic_screens_udp_streamer.exe")
        print("-" * 60)
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(1.0)
        
        try:
            sock.bind((self.ip, self.port))
        except OSError as e:
            print(f"Error binding to {self.ip}:{self.port}: {e}")
            print("Make sure no other process is using this port.")
            return
        
        self.running = True
        self.start_time = time.time()
        last_stats_time = self.start_time
        
        try:
            while time.time() - self.start_time < duration and self.running:
                try:
                    data, addr = sock.recvfrom(4096)
                    self.total_packets += 1
                    
                    # Parse packet structure: uint64_t timestamp + DVSEvent array
                    if len(data) >= 8:
                        # Extract packet timestamp (first 8 bytes)
                        packet_timestamp = struct.unpack('<Q', data[:8])[0]
                        
                        # Calculate number of events
                        event_data_size = len(data) - 8
                        dvs_event_size = 8 + 2 + 2 + 1  # timestamp(8) + x(2) + y(2) + polarity(1)
                        num_events = event_data_size // dvs_event_size
                        self.total_events += num_events
                        
                        # Parse first event for demonstration
                        if num_events > 0 and len(data) >= 8 + dvs_event_size:
                            event_start = 8  # Skip packet timestamp
                            event_timestamp = struct.unpack('<Q', data[event_start:event_start+8])[0]
                            event_x = struct.unpack('<H', data[event_start+8:event_start+10])[0]
                            event_y = struct.unpack('<H', data[event_start+10:event_start+12])[0]
                            event_polarity = struct.unpack('?', data[event_start+12:event_start+13])[0]
                            
                            # Show statistics every 5 seconds
                            current_time = time.time()
                            if current_time - last_stats_time >= 5.0:
                                elapsed = current_time - self.start_time
                                packets_per_sec = self.total_packets / elapsed
                                events_per_sec = self.total_events / elapsed
                                
                                print(f\"\\n--- Statistics (after {elapsed:.1f}s) ---\")\n                                print(f\"Packets received: {self.total_packets} ({packets_per_sec:.1f}/sec)\")\n                                print(f\"Events received: {self.total_events} ({events_per_sec:.1f}/sec)\")\n                                print(f\"Last packet: {num_events} events, timestamp: {packet_timestamp}\")\n                                print(f\"Sample event: x={event_x}, y={event_y}, polarity={event_polarity}\")\n                                \n                                last_stats_time = current_time\n                        \n                except socket.timeout:\n                    # Check if we've received any data yet\n                    current_time = time.time()\n                    if current_time - self.start_time > 10 and self.total_packets == 0:\n                        print(\"No packets received yet. Is the C++ streamer running?\")\n                        print(\"Run: build\\\\bin\\\\Release\\\\neuromorphic_screens_udp_streamer.exe\")\n                    continue\n                    \n        except KeyboardInterrupt:\n            print(\"\\nReceiver interrupted by user\")\n            \n        finally:\n            sock.close()\n            self.running = False\n            \n        # Final statistics\n        total_time = time.time() - self.start_time\n        print(f\"\\n{'='*60}\")\n        print(\"Final Results:\")\n        print(f\"  Duration: {total_time:.2f} seconds\")\n        print(f\"  Total packets: {self.total_packets}\")\n        print(f\"  Total events: {self.total_events}\")\n        if total_time > 0:\n            print(f\"  Average packets/sec: {self.total_packets/total_time:.1f}\")\n            print(f\"  Average events/sec: {self.total_events/total_time:.1f}\")\n        \n        if self.total_packets > 0:\n            print(\"\\n[SUCCESS] Event streaming pipeline is working!\")\n        else:\n            print(\"\\n[WARNING] No packets received. Check that the C++ streamer is running.\")\n\ndef main():\n    parser = argparse.ArgumentParser(description='Test UDP Event Streaming Pipeline')\n    parser.add_argument('--ip', default='127.0.0.1', help='IP address to listen on')\n    parser.add_argument('--port', type=int, default=9999, help='UDP port to listen on')\n    parser.add_argument('--duration', type=int, default=30, help='Test duration in seconds')\n    \n    args = parser.parse_args()\n    \n    print(\"UDP Event Streaming Pipeline Test\")\n    print(\"=\" * 40)\n    print(\"This test receives events from the C++ UDP streamer.\")\n    print(\"\")\n    print(\"Instructions:\")\n    print(\"1. Start this receiver first\")\n    print(\"2. In another terminal, run the C++ streamer:\")\n    print(\"   build\\\\bin\\\\Release\\\\neuromorphic_screens_udp_streamer.exe\")\n    print(\"\")\n    \n    receiver = SimpleUDPReceiver(args.ip, args.port)\n    receiver.start(args.duration)\n\nif __name__ == \"__main__\":\n    main()