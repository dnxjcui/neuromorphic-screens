#!/usr/bin/env python3
"""
Debug UDP packet format between C++ sender and Python receiver
"""
import socket
import struct
import time

def debug_udp_packets(port=9999, duration=20):
    """Debug raw UDP packets to understand the format"""
    print(f"Debugging UDP packets on port {port} for {duration} seconds")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', port))
    sock.settimeout(1.0)
    
    start_time = time.time()
    packet_count = 0
    
    try:
        while time.time() - start_time < duration:
            try:
                data, addr = sock.recvfrom(4096)
                packet_count += 1
                
                print(f"\n--- Packet {packet_count} from {addr} ---")
                print(f"Raw packet size: {len(data)} bytes")
                print(f"First 64 bytes (hex): {data[:64].hex()}")
                
                if len(data) >= 8:
                    # Parse packet timestamp (first 8 bytes)
                    packet_timestamp = struct.unpack('<Q', data[:8])[0]
                    print(f"Packet timestamp: {packet_timestamp}")
                    
                    # Parse DVS events
                    event_data = data[8:]
                    dvs_event_size = 8 + 2 + 2 + 1  # timestamp + x + y + polarity = 13 bytes
                    num_events = len(event_data) // dvs_event_size
                    
                    print(f"Event data size: {len(event_data)} bytes")
                    print(f"Expected DVS event size: {dvs_event_size} bytes")
                    print(f"Calculated number of events: {num_events}")
                    
                    if num_events > 0:
                        # Parse first few events
                        for i in range(min(3, num_events)):
                            offset = i * dvs_event_size
                            event_bytes = event_data[offset:offset + dvs_event_size]
                            
                            if len(event_bytes) >= dvs_event_size:
                                # Parse DVSEvent: uint64_t timestamp, uint16_t x, uint16_t y, bool polarity
                                timestamp, x, y, polarity = struct.unpack('<QHH?', event_bytes)
                                print(f"  Event {i+1}: t={timestamp}, x={x}, y={y}, polarity={polarity}")
                                
                    if packet_count >= 5:
                        print(f"\nReceived {packet_count} packets, stopping debug...")
                        break
                        
            except socket.timeout:
                continue
                
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        
    finally:
        sock.close()
        
    elapsed = time.time() - start_time
    print(f"\nDebug completed:")
    print(f"  Duration: {elapsed:.1f}s")
    print(f"  Packets received: {packet_count}")

if __name__ == "__main__":
    debug_udp_packets()