#!/usr/bin/env python3
"""
Simple UDP Event Receiver for testing neuromorphic event streams
Receives and displays neuromorphic events from the C++ UDP event streamer
"""

import socket
import struct
import time
import sys
import signal
from collections import defaultdict

class EventReceiver:
    def __init__(self, port=9999, buffer_size=131072):  # Increased buffer size
        self.port = port
        self.buffer_size = buffer_size
        self.socket = None
        self.running = False
        self.stats = {
            'packets_received': 0,
            'events_received': 0,
            'bytes_received': 0,
            'start_time': 0,
            'polarity_counts': defaultdict(int)
        }
        
    def start(self):
        """Start the UDP receiver"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.bind(('127.0.0.1', self.port))
            self.socket.settimeout(1.0)  # 1 second timeout for graceful shutdown
            self.running = True
            self.stats['start_time'] = time.time()
            
            print(f"UDP Event Receiver started on port {self.port}")
            print("Waiting for events from C++ streamer...")
            print("Use Ctrl+C to stop\n")
            
            return True
        except Exception as e:
            print(f"Failed to start UDP receiver: {e}")
            return False
    
    def stop(self):
        """Stop the UDP receiver"""
        self.running = False
        if self.socket:
            self.socket.close()
    
    def process_packet(self, data):
        """Process a received UDP packet"""
        if len(data) < 8:
            print(f"Warning: Packet too small ({len(data)} bytes)")
            return
        
        try:
            # Read packet timestamp (first 8 bytes)
            packet_timestamp = struct.unpack('<Q', data[:8])[0]
            
            # Process events in the packet
            event_data = data[8:]
            events_processed = 0
            
            # Each DVSEvent is 32 bytes: timestamp(8) + x(4) + y(4) + polarity(1) + on(1) + padding(14)
            event_size = 32
            offset = 0
            
            while offset + event_size <= len(event_data):
                event_bytes = event_data[offset:offset + event_size]
                
                if len(event_bytes) >= 17:  # Minimum needed: timestamp + x + y + polarity
                    timestamp = struct.unpack('<Q', event_bytes[0:8])[0]
                    x = struct.unpack('<I', event_bytes[8:12])[0]
                    y = struct.unpack('<I', event_bytes[12:16])[0]
                    polarity = struct.unpack('<B', event_bytes[16:17])[0]
                    
                    # Validate coordinates
                    if 0 <= x <= 1920 and 0 <= y <= 1080:
                        # Update statistics
                        self.stats['polarity_counts'][polarity] += 1
                        events_processed += 1
                        
                        # Print first few events for debugging
                        if self.stats['events_received'] < 5:
                            print(f"Event {self.stats['events_received']}: t={timestamp}, x={x}, y={y}, pol={polarity}")
                
                offset += event_size
            
            self.stats['events_received'] += events_processed
            
            # Print periodic updates
            if self.stats['packets_received'] % 50 == 0:
                elapsed = time.time() - self.stats['start_time']
                packets_per_sec = self.stats['packets_received'] / elapsed if elapsed > 0 else 0
                events_per_sec = self.stats['events_received'] / elapsed if elapsed > 0 else 0
                
                print(f"Packets: {self.stats['packets_received']}, "
                      f"Events: {self.stats['events_received']}, "
                      f"Rate: {packets_per_sec:.1f} pkt/s, {events_per_sec:.0f} evt/s")
        
        except struct.error as e:
            print(f"Struct unpacking error: {e}")
        except Exception as e:
            print(f"Error processing packet: {e}")
    
    def run(self):
        """Main receiver loop"""
        if not self.start():
            return False
        
        try:
            while self.running:
                try:
                    data, addr = self.socket.recvfrom(self.buffer_size)
                    self.stats['packets_received'] += 1
                    self.stats['bytes_received'] += len(data)
                    
                    # Process the packet
                    self.process_packet(data)
                    
                except socket.timeout:
                    # Timeout is normal, continue
                    continue
                except Exception as e:
                    if self.running:
                        print(f"Error receiving data: {e}")
                    break
        
        except KeyboardInterrupt:
            print("\nShutdown requested...")
        
        finally:
            self.stop()
            self.print_final_stats()
        
        return True
    
    def print_final_stats(self):
        """Print final statistics"""
        elapsed = time.time() - self.stats['start_time']
        
        print(f"\n=== Final Statistics ===")
        print(f"Runtime: {elapsed:.1f} seconds")
        print(f"Packets received: {self.stats['packets_received']}")
        print(f"Events received: {self.stats['events_received']}")
        print(f"Bytes received: {self.stats['bytes_received']}")
        
        if elapsed > 0:
            print(f"Average packet rate: {self.stats['packets_received'] / elapsed:.1f} packets/sec")
            print(f"Average event rate: {self.stats['events_received'] / elapsed:.0f} events/sec")
            print(f"Average throughput: {self.stats['bytes_received'] / elapsed / 1024:.1f} KB/sec")
        
        print(f"\nPolarity distribution:")
        for polarity, count in sorted(self.stats['polarity_counts'].items()):
            percentage = (count / self.stats['events_received'] * 100) if self.stats['events_received'] > 0 else 0
            print(f"  Polarity {polarity}: {count} events ({percentage:.1f}%)")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='UDP Event Receiver for Neuromorphic Streams')
    parser.add_argument('--port', type=int, default=9999, 
                       help='UDP port to listen on (default: 9999)')
    parser.add_argument('--buffer', type=int, default=131072,
                       help='UDP receive buffer size (default: 131072)')
    
    args = parser.parse_args()
    
    print("Neuromorphic UDP Event Receiver")
    print("==============================")
    print(f"Listening on port: {args.port}")
    print(f"Buffer size: {args.buffer} bytes")
    print()
    
    receiver = EventReceiver(args.port, args.buffer)
    
    # Set up signal handler for graceful shutdown
    def signal_handler(sig, frame):
        print("\nReceived interrupt signal")
        receiver.stop()
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Run the receiver
    success = receiver.run()
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())