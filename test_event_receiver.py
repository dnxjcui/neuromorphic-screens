#!/usr/bin/env python3
"""
Neuromorphic Event Stream Receiver Test
Compatible with UDP event streamer from neuromorphic_screens

This script receives DVS events streamed over UDP and displays statistics.
Designed to work with the C++ UDP event streamer and prepare data for BindsNET integration.

Dependencies:
    pip install event_stream numpy

Usage:
    python test_event_receiver.py [--ip IP] [--port PORT] [--duration SECONDS]
"""

import argparse
import time
import queue
import threading
import numpy as np
import sys
from typing import Optional, List, Dict, Any

try:
    import event_stream
    HAS_EVENT_STREAM = True
except ImportError:
    print("Warning: event_stream library not found.")
    print("Install with: pip install event_stream")
    HAS_EVENT_STREAM = False

class EventReceiver:
    """
    Receives and processes neuromorphic events from UDP stream
    """
    
    def __init__(self, ip: str = "127.0.0.1", port: int = 9999):
        self.ip = ip
        self.port = port
        self.event_queue = queue.Queue()
        self.running = False
        self.consumer_thread: Optional[threading.Thread] = None
        
        # Statistics
        self.total_chunks = 0
        self.total_events = 0
        self.start_time = 0
        self.last_stats_time = 0
        self.events_since_last_stats = 0
        
    def start(self):
        """Start the event receiver"""
        if self.running:
            print("Event receiver already running")
            return
            
        if not HAS_EVENT_STREAM:
            print("Cannot start receiver: event_stream library not available")
            return
            
        self.running = True
        self.start_time = time.time()
        self.last_stats_time = self.start_time
        
        # Start consumer thread
        self.consumer_thread = threading.Thread(
            target=self._consume_events_to_queue,
            args=(self.ip, self.port, self.event_queue)
        )
        self.consumer_thread.daemon = True
        self.consumer_thread.start()
        
        print(f"Event receiver started, listening on {self.ip}:{self.port}")
        
    def stop(self):
        """Stop the event receiver"""
        if not self.running:
            return
            
        self.running = False
        if self.consumer_thread and self.consumer_thread.is_alive():
            # Signal stop by putting None in queue
            self.event_queue.put(None)
            self.consumer_thread.join(timeout=1.0)
            
        print("Event receiver stopped")
        
    def _consume_events_to_queue(self, ip: str, port: int, output_queue: queue.Queue):
        """
        Consumer thread function that receives UDP event streams
        Compatible with event_stream.UdpDecoder
        """
        print(f"Starting UDP decoder for {ip}:{port}")
        
        try:
            # event_stream.UdpDecoder expects each UDP packet to start with uint64 timestamp
            # followed by DVS event data
            with event_stream.UdpDecoder(port) as decoder:
                print(f"UDP Decoder initialized:")
                print(f"  Type: {getattr(decoder, 'type', 'unknown')}")
                print(f"  Width: {getattr(decoder, 'width', 'unknown')}")  
                print(f"  Height: {getattr(decoder, 'height', 'unknown')}")
                
                for chunk in decoder:
                    if not self.running:
                        break
                        
                    # chunk is a numpy array with dtype=event_stream.dvs_dtype
                    # dtype: [('t', '<u8'), ('x', '<u2'), ('y', '<u2'), ('on', '?')]
                    if len(chunk) > 0:
                        output_queue.put(chunk)
                    else:
                        # Empty chunk, continue waiting
                        time.sleep(0.001)
                        
        except Exception as e:
            print(f"Error in UDP decoder: {e}")
            import traceback
            traceback.print_exc()
        finally:
            output_queue.put(None)  # Signal end of stream
            
    def process_events(self, max_duration: float = 0) -> Dict[str, Any]:
        """
        Process received events and return statistics
        
        Args:
            max_duration: Maximum time to run (0 = unlimited)
            
        Returns:
            Dictionary with processing statistics
        """
        if not self.running:
            print("Receiver not running")
            return {}
            
        print("Processing events... (Ctrl+C to stop)")
        
        try:
            end_time = time.time() + max_duration if max_duration > 0 else float('inf')
            
            while self.running and time.time() < end_time:
                try:
                    # Get chunk from queue with timeout
                    chunk = self.event_queue.get(timeout=1.0)
                    
                    if chunk is None:
                        print("End of event stream detected")
                        break
                        
                    # Process chunk
                    self._process_chunk(chunk)
                    
                except queue.Empty:
                    # No events received, continue waiting
                    continue
                    
        except KeyboardInterrupt:
            print("\nProcessing interrupted by user")
            
        # Calculate final statistics
        total_time = time.time() - self.start_time
        avg_events_per_sec = self.total_events / total_time if total_time > 0 else 0
        
        stats = {
            'total_chunks': self.total_chunks,
            'total_events': self.total_events,
            'duration_seconds': total_time,
            'avg_events_per_second': avg_events_per_sec
        }
        
        return stats
        
    def _process_chunk(self, chunk: np.ndarray):
        """Process a single chunk of events"""
        self.total_chunks += 1
        chunk_size = len(chunk)
        self.total_events += chunk_size
        self.events_since_last_stats += chunk_size
        
        current_time = time.time()
        
        # Print statistics every 5 seconds
        if current_time - self.last_stats_time >= 5.0:
            time_elapsed = current_time - self.last_stats_time
            events_per_sec = self.events_since_last_stats / time_elapsed
            
            # Analyze chunk content
            if chunk_size > 0:
                timestamps = chunk['t']
                x_coords = chunk['x'] 
                y_coords = chunk['y']
                polarities = chunk['on']
                
                positive_events = np.sum(polarities)
                negative_events = chunk_size - positive_events
                
                print(f"\n--- Event Statistics ---")
                print(f"Chunks received: {self.total_chunks}")
                print(f"Total events: {self.total_events}")
                print(f"Recent events/sec: {events_per_sec:.1f}")
                print(f"Last chunk: {chunk_size} events")
                print(f"  Positive events: {positive_events}")
                print(f"  Negative events: {negative_events}")
                print(f"  Timestamp range: {timestamps[0]} - {timestamps[-1]} μs")
                print(f"  X range: {np.min(x_coords)} - {np.max(x_coords)}")
                print(f"  Y range: {np.min(y_coords)} - {np.max(y_coords)}")
                
            self.last_stats_time = current_time
            self.events_since_last_stats = 0


def simulate_receiver_without_event_stream(ip: str, port: int, duration: float):
    """
    Fallback receiver using raw sockets when event_stream is not available
    """
    import socket
    import struct
    
    print(f"Using fallback socket receiver (event_stream not available)")
    print(f"Listening on {ip}:{port} for {duration} seconds...")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((ip, port))
    sock.settimeout(1.0)
    
    total_packets = 0
    total_events = 0
    start_time = time.time()
    
    try:
        while time.time() - start_time < duration:
            try:
                data, addr = sock.recvfrom(4096)
                total_packets += 1
                
                if len(data) >= 8:  # At least timestamp
                    # Parse packet timestamp
                    packet_timestamp = struct.unpack('<Q', data[:8])[0]
                    
                    # Calculate number of events (remaining bytes / DVSEvent size)
                    event_data_size = len(data) - 8
                    dvs_event_size = 8 + 2 + 2 + 1  # timestamp + x + y + polarity
                    num_events = event_data_size // dvs_event_size
                    total_events += num_events
                    
                    if total_packets % 50 == 0:
                        elapsed = time.time() - start_time
                        print(f"Received {total_packets} packets, {total_events} events "
                              f"({total_events/elapsed:.1f} events/sec)")
                        
            except socket.timeout:
                continue
                
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        
    finally:
        sock.close()
        
    elapsed = time.time() - start_time
    print(f"\nFallback receiver results:")
    print(f"  Packets: {total_packets}")
    print(f"  Events: {total_events}")
    print(f"  Duration: {elapsed:.1f}s")
    print(f"  Avg events/sec: {total_events/elapsed:.1f}")


def main():
    parser = argparse.ArgumentParser(description='Neuromorphic Event Stream Receiver Test')
    parser.add_argument('--ip', default='127.0.0.1', help='IP address to listen on (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=9999, help='UDP port to listen on (default: 9999)')
    parser.add_argument('--duration', type=float, default=0, help='Duration to run in seconds (0 = unlimited)')
    parser.add_argument('--fallback', action='store_true', help='Use fallback socket receiver')
    
    args = parser.parse_args()
    
    print("Neuromorphic Event Stream Receiver Test")
    print("=" * 40)
    
    if args.fallback or not HAS_EVENT_STREAM:
        if args.duration <= 0:
            args.duration = 30  # Default to 30 seconds for fallback
        simulate_receiver_without_event_stream(args.ip, args.port, args.duration)
        return
    
    # Use event_stream library
    receiver = EventReceiver(args.ip, args.port)
    
    try:
        receiver.start()
        stats = receiver.process_events(args.duration)
        
        print("\n" + "=" * 40)
        print("Final Statistics:")
        print(f"  Total chunks: {stats.get('total_chunks', 0)}")
        print(f"  Total events: {stats.get('total_events', 0)}")  
        print(f"  Duration: {stats.get('duration_seconds', 0):.2f} seconds")
        print(f"  Average events/sec: {stats.get('avg_events_per_second', 0):.1f}")
        
    finally:
        receiver.stop()


if __name__ == "__main__":
    main()