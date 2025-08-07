#!/usr/bin/env python3
"""
Simple UDP Event Receiver Test
Quick test to verify UDP packet reception and event parsing
"""

import socket
import struct
import time
import sys

def test_udp_reception():
    print("Simple UDP Event Reception Test")
    print("===============================")
    
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 9999))
    sock.settimeout(5.0)  # 5 second timeout
    
    print("Listening on 127.0.0.1:9999...")
    print("Start C++ UDP streamer now...")
    
    packets_received = 0
    events_received = 0
    
    try:
        while packets_received < 10:  # Test first 10 packets
            try:
                data, addr = sock.recvfrom(65536)
                packets_received += 1
                
                print(f"\nPacket {packets_received}: {len(data)} bytes from {addr}")
                
                if len(data) >= 8:
                    # Extract packet timestamp
                    packet_timestamp = struct.unpack('<Q', data[:8])[0]
                    print(f"  Packet timestamp: {packet_timestamp}")
                    
                    # Extract events
                    event_data = data[8:]
                    event_size = 13  # DVSEvent: timestamp(8) + x(2) + y(2) + polarity(1)
                    num_events = len(event_data) // event_size
                    
                    print(f"  Event data: {len(event_data)} bytes, {num_events} events")
                    
                    # Parse first few events
                    events_parsed = 0
                    for i in range(min(3, num_events)):
                        offset = i * event_size
                        if offset + event_size <= len(event_data):
                            event_bytes = event_data[offset:offset + event_size]
                            if len(event_bytes) >= 13:
                                timestamp = struct.unpack('<Q', event_bytes[0:8])[0]
                                x = struct.unpack('<H', event_bytes[8:10])[0]
                                y = struct.unpack('<H', event_bytes[10:12])[0]
                                polarity = struct.unpack('<b', event_bytes[12:13])[0]
                                
                                print(f"    Event {i}: t={timestamp}, x={x}, y={y}, pol={polarity}")
                                events_parsed += 1
                    
                    events_received += num_events
                    
                    if num_events > 3:
                        print(f"    ... and {num_events - 3} more events")
                        
            except socket.timeout:
                print("Timeout waiting for packets - is C++ streamer running?")
                break
                
    except KeyboardInterrupt:
        print("\nTest interrupted")
    finally:
        sock.close()
    
    print(f"\nTest Results:")
    print(f"Packets received: {packets_received}")
    print(f"Events received: {events_received}")
    
    if packets_received > 0:
        print("UDP reception is working!")
        return True
    else:
        print("No packets received - check C++ streamer")
        return False

if __name__ == "__main__":
    success = test_udp_reception()
    sys.exit(0 if success else 1) 