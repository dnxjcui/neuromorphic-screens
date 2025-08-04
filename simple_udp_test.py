#!/usr/bin/env python3
"""
Simple UDP test to verify packets are being received
"""
import socket
import struct
import time
import threading

def udp_receiver():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', 9999))
    sock.settimeout(1.0)
    
    packet_count = 0
    total_events = 0
    start_time = time.time()
    
    print("UDP receiver started, listening on port 9999...")
    
    try:
        while time.time() - start_time < 10:  # Run for 10 seconds
            try:
                data, addr = sock.recvfrom(4096)
                packet_count += 1
                
                if len(data) >= 8:
                    # Parse packet timestamp
                    packet_timestamp = struct.unpack('<Q', data[:8])[0]
                    
                    # Calculate events
                    event_data_size = len(data) - 8
                    dvs_event_size = 13  # 8+2+2+1
                    num_events = event_data_size // dvs_event_size
                    total_events += num_events
                    
                    print(f"Packet {packet_count}: {len(data)} bytes, {num_events} events, timestamp={packet_timestamp}")
                    
                    if packet_count >= 5:
                        break
                        
            except socket.timeout:
                continue
                
    except KeyboardInterrupt:
        print("Interrupted")
        
    finally:
        sock.close()
        
    print(f"Received {packet_count} packets, {total_events} total events")

if __name__ == "__main__":
    udp_receiver()