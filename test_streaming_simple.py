#!/usr/bin/env python3
"""
Simple test for UDP event streaming pipeline
"""

import socket
import struct
import time
import argparse

def test_udp_streaming(ip="127.0.0.1", port=9999, duration=10):
    """Test UDP event streaming with fallback socket receiver"""
    
    print(f"Starting UDP receiver on {ip}:{port}")
    print(f"Listening for {duration} seconds...")
    print("Start the C++ streamer with: build\\bin\\Release\\neuromorphic_screens_udp_streamer.exe")
    print("-" * 60)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    
    try:
        sock.bind((ip, port))
    except OSError as e:
        print(f"Error binding to {ip}:{port}: {e}")
        return
    
    total_packets = 0
    total_events = 0
    start_time = time.time()
    last_stats_time = start_time
    
    try:
        while time.time() - start_time < duration:
            try:
                data, addr = sock.recvfrom(4096)
                total_packets += 1
                
                # Parse packet structure: uint64_t timestamp + DVSEvent array
                if len(data) >= 8:
                    # Extract packet timestamp (first 8 bytes)
                    packet_timestamp = struct.unpack('<Q', data[:8])[0]
                    
                    # Calculate number of events
                    event_data_size = len(data) - 8
                    dvs_event_size = 8 + 2 + 2 + 1  # timestamp(8) + x(2) + y(2) + polarity(1)
                    num_events = event_data_size // dvs_event_size
                    total_events += num_events
                    
                    # Show statistics every 5 seconds
                    current_time = time.time()
                    if current_time - last_stats_time >= 5.0:
                        elapsed = current_time - start_time
                        packets_per_sec = total_packets / elapsed
                        events_per_sec = total_events / elapsed
                        
                        print(f"Statistics (after {elapsed:.1f}s):")
                        print(f"  Packets: {total_packets} ({packets_per_sec:.1f}/sec)")
                        print(f"  Events: {total_events} ({events_per_sec:.1f}/sec)")
                        print(f"  Last packet: {num_events} events")
                        
                        last_stats_time = current_time
                    
            except socket.timeout:
                current_time = time.time()
                if current_time - start_time > 5 and total_packets == 0:
                    print("No packets received yet. Is the C++ streamer running?")
                continue
                
    except KeyboardInterrupt:
        print("\\nReceiver interrupted by user")
        
    finally:
        sock.close()
        
    # Final statistics
    total_time = time.time() - start_time
    print(f"\\n{'='*40}")
    print("Final Results:")
    print(f"  Duration: {total_time:.2f} seconds")
    print(f"  Total packets: {total_packets}")
    print(f"  Total events: {total_events}")
    if total_time > 0:
        print(f"  Avg packets/sec: {total_packets/total_time:.1f}")
        print(f"  Avg events/sec: {total_events/total_time:.1f}")
    
    if total_packets > 0:
        print("\\n[SUCCESS] Event streaming pipeline is working!")
        return True
    else:
        print("\\n[WARNING] No packets received.")
        return False

def main():
    parser = argparse.ArgumentParser(description='Test UDP Event Streaming Pipeline')
    parser.add_argument('--ip', default='127.0.0.1', help='IP address to listen on')
    parser.add_argument('--port', type=int, default=9999, help='UDP port to listen on')
    parser.add_argument('--duration', type=int, default=10, help='Test duration in seconds')
    
    args = parser.parse_args()
    
    print("UDP Event Streaming Pipeline Test")
    print("=" * 40)
    
    success = test_udp_streaming(args.ip, args.port, args.duration)
    return 0 if success else 1

if __name__ == "__main__":
    import sys
    sys.exit(main())