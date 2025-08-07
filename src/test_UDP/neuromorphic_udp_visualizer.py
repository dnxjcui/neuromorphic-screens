#!/usr/bin/env python3
"""
Neuromorphic UDP Event Visualizer
Professional real-time visualization of neuromorphic events from UDP stream

Features:
- Large neuromorphic screen visualization (mimics ImGui C++ implementation)
- Dynamic event capture plot with 5-second sliding window
- Python ImGui interface for professional appearance
- Real-time UDP event reception and processing
- Performance monitoring and statistics
"""

import sys
import numpy as np
import socket
import struct
import threading
import queue
import time
import argparse
from collections import deque
import imgui
import imgui.integrations.glfw
import glfw
from OpenGL.GL import *
import matplotlib.pyplot as plt
from matplotlib.backends.backend_agg import FigureCanvasAgg
from PIL import Image
from joblib import Parallel, delayed

try:
    import cupy as cp
    GPU_AVAILABLE = True
    print("GPU support available with CuPy")
except ImportError:
    GPU_AVAILABLE = False
    print("GPU support not available (CuPy not installed)")


# Constants matching C++ implementation
DOT_SIZE = 2.0
DOT_FADE_DURATION = 0.1  # 100ms fade duration


class EventData:
    """Thread-safe container for neuromorphic event data"""
    def __init__(self):
        self.events = deque(maxlen=100000)  # Large buffer for event history
        self.lock = threading.Lock()
        self.stats = {'packets': 0, 'events': 0, 'bytes': 0}
    
    def add_event(self, timestamp, x, y, polarity):
        with self.lock:
            current_time = time.time()
            self.events.append({
                'timestamp': timestamp,
                'x': x,
                'y': y,
                'polarity': polarity,
                'received_time': current_time
            })
    
    def get_recent_events(self, time_window=5.0):
        """Get events from the last time_window seconds"""
        with self.lock:
            current_time = time.time()
            cutoff_time = current_time - time_window
            recent_events = [e for e in self.events if e['received_time'] >= cutoff_time]
            return recent_events
    
    def clear(self):
        with self.lock:
            self.events.clear()


class UDPEventReceiver:
    """High-performance UDP receiver for neuromorphic events"""
    def __init__(self, port=9999, buffer_size=1024 * 1024 * 20):  # 20MB buffer
        self.port = port
        self.buffer_size = buffer_size
        self.socket = None
        self.running = False
        self.event_data = EventData()
        self.thread = None
        
    def start(self):
        """Start UDP receiver thread"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.bind(('127.0.0.1', self.port))
            self.socket.settimeout(1.0)
            self.running = True
            
            self.thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.thread.start()
            print(f"UDP receiver started on port {self.port}")
            return True
        except Exception as e:
            print(f"Failed to start UDP receiver: {e}")
            return False
    
    def stop(self):
        """Stop UDP receiver"""
        self.running = False
        if self.socket:
            self.socket.close()
        if self.thread:
            self.thread.join()
        print("UDP receiver stopped")
    
    def _receive_loop(self):
        """Main UDP receiving loop"""
        while self.running:
            try:
                data, addr = self.socket.recvfrom(self.buffer_size)
                self._process_packet(data)
                self.event_data.stats['packets'] += 1
                self.event_data.stats['bytes'] += len(data)
                
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"UDP receive error: {e}")
                break
    
    def _process_packet(self, data):
        """Process received UDP packet containing DVS events"""
        if len(data) < 8:  # Need at least timestamp
            return
        
        try:
            # Extract packet timestamp (first 8 bytes)
            packet_timestamp = struct.unpack('<Q', data[:8])[0]
            
            # Extract events (remaining data)
            event_data = data[8:]
            # DVSEvent size: timestamp(8) + x(2) + y(2) + polarity(1) = 13 bytes (matches C++ Event struct)
            event_size = 16
            
            num_events = len(event_data) // event_size
            
            # Debug output for packet analysis (much less frequent)
            if num_events > 0 and self.event_data.stats['packets'] % 500 == 0:
                print(f"Received packet: {len(data)} bytes, {num_events} events")
            
            # Process events in batch for better performance
            events_to_add = []
            for i in range(num_events):
                offset = i * event_size
                if offset + event_size <= len(event_data):
                    event_bytes = event_data[offset:offset + event_size]
                    if len(event_bytes) >= 13:  # Minimum required bytes for DVSEvent
                        timestamp = struct.unpack('<Q', event_bytes[0:8])[0]
                        x = struct.unpack('<H', event_bytes[8:10])[0]  # uint16_t
                        y = struct.unpack('<H', event_bytes[10:12])[0]  # uint16_t
                        polarity = struct.unpack('<b', event_bytes[12:13])[0]  # int8_t polarity
                        
                        # Validate coordinates (screen bounds check)
                        if 0 <= x <= 1920 and 0 <= y <= 1080:
                            events_to_add.append((timestamp, x, y, polarity))
            
            # Add events in batch
            if events_to_add:
                with self.event_data.lock:
                    current_time = time.time()
                    for timestamp, x, y, polarity in events_to_add:
                        self.event_data.events.append({
                            'timestamp': timestamp,
                            'x': x,
                            'y': y,
                            'polarity': polarity,
                            'received_time': current_time
                        })
                    self.event_data.stats['events'] += len(events_to_add)
                            
        except struct.error as e:
            print(f"Packet parsing error: {e}")
        except Exception as e:
            print(f"Unexpected error processing packet: {e}")


class NeuromorphicVisualizer:
    """ImGui-based neuromorphic event visualizer matching C++ implementation"""
    def __init__(self, width=1920, height=1080, canvas_width=800, canvas_height=600, use_gpu=False):
        self.screen_width = width
        self.screen_height = height
        self.canvas_width = canvas_width
        self.canvas_height = canvas_height
        
        # GPU acceleration option
        self.use_gpu = use_gpu and GPU_AVAILABLE
        if self.use_gpu:
            print("GPU acceleration enabled")
        
        # Event visualization - matching C++ implementation
        self.active_dots = []  # List of (event, fade_time) tuples
        self.max_active_dots = 100000
        
        # Plot data for event capture rate
        self.plot_times = deque(maxlen=300)  # 5 seconds at 60fps
        self.plot_event_counts = deque(maxlen=300)
        self.last_plot_update = time.time()
        self.plot_update_interval = 1.0 / 60.0  # 60 FPS plot updates
        
        # Statistics
        self.performance_stats = {
            'fps': 0.0,
            'events_per_sec': 0.0,
            'active_dots': 0,
            'total_events': 0
        }
        
        # Timing for FPS calculation
        self.last_frame_time = time.time()
        self.frame_times = deque(maxlen=60)
        
        # Plot texture for matplotlib integration
        self.plot_texture_id = None
        self.plot_width = 400
        self.plot_height = 200
        
    def screen_to_canvas(self, screen_x, screen_y):
        """Convert screen coordinates to canvas coordinates - matching C++ implementation"""
        if self.screen_width > 0 and self.screen_height > 0:
            scale_x = float(self.canvas_width) / float(self.screen_width)
            scale_y = float(self.canvas_height) / float(self.screen_height)
            return (screen_x * scale_x, screen_y * scale_y)
        else:
            return (screen_x, screen_y)
    
    def screen_to_canvas_batch_gpu(self, events):
        """GPU-accelerated batch coordinate transformation"""
        if not self.use_gpu or not events:
            return events
        
        try:
            # Extract coordinates
            coords = [(e['x'], e['y']) for e in events]
            coords_gpu = cp.array(coords, dtype=cp.float32)
            
            # Apply scaling
            scale_x = float(self.canvas_width) / float(self.screen_width)
            scale_y = float(self.canvas_height) / float(self.screen_height)
            coords_gpu[:, 0] *= scale_x
            coords_gpu[:, 1] *= scale_y
            
            # Convert back to CPU
            coords_scaled = cp.asnumpy(coords_gpu)
            
            # Update events with scaled coordinates
            for i, event in enumerate(events):
                event['canvas_x'] = coords_scaled[i, 0]
                event['canvas_y'] = coords_scaled[i, 1]
            
            return events
        except Exception as e:
            print(f"GPU coordinate transformation failed: {e}")
            return events
        
    def update_events(self, event_data):
        """Update visualization with new events - matching C++ implementation"""
        current_time = time.time()
        
        # Get recent events (last 100ms for active dots - matching C++ DOT_FADE_DURATION)
        recent_events = event_data.get_recent_events(time_window=0.1)
        
        # Update active dots with proper fade timing - optimized for performance
        self.active_dots = []
        if recent_events:  # Only process if we have events
            # Limit to most recent events for performance
            max_events_to_process = min(len(recent_events), self.max_active_dots)
            recent_events = recent_events[-max_events_to_process:]
            
            def process_event(event):
                age = current_time - event['received_time']
                if age <= DOT_FADE_DURATION:
                    # Calculate alpha exactly like C++: alpha = dot.second / DOT_FADE_DURATION
                    alpha = (DOT_FADE_DURATION - age) / DOT_FADE_DURATION
                    alpha = max(0.0, min(1.0, alpha))  # Clamp to [0,1]
                    self.active_dots.append({
                        'x': event['x'],
                        'y': event['y'], 
                        'polarity': event['polarity'],
                        'alpha': alpha
                    })

            # Parallel(n_jobs=-1)(delayed(process_event)(event) for event in recent_events)
            
            for event in recent_events:
                age = current_time - event['received_time']
                if age <= DOT_FADE_DURATION:
                    # Calculate alpha exactly like C++: alpha = dot.second / DOT_FADE_DURATION
                    alpha = (DOT_FADE_DURATION - age) / DOT_FADE_DURATION
                    alpha = max(0.0, min(1.0, alpha))  # Clamp to [0,1]
                    
                    self.active_dots.append({
                        'x': event['x'],
                        'y': event['y'], 
                        'polarity': event['polarity'],
                        'alpha': alpha
                    })
        
        # Update performance stats
        self.performance_stats['active_dots'] = len(self.active_dots)
        self.performance_stats['total_events'] = event_data.stats['events']
        
        # Calculate FPS (less frequent updates)
        self.frame_times.append(current_time)
        if len(self.frame_times) > 1:
            fps = len(self.frame_times) / (self.frame_times[-1] - self.frame_times[0])  
            self.performance_stats['fps'] = fps
        
        # Update plot data (less frequent updates)
        if current_time - self.last_plot_update >= self.plot_update_interval:
            self.plot_times.append(current_time)
            
            # Count events in last second
            one_sec_ago = current_time - 1.0
            recent_events_1sec = [e for e in recent_events if e['received_time'] >= one_sec_ago]
            self.plot_event_counts.append(len(recent_events_1sec))
            self.performance_stats['events_per_sec'] = len(recent_events_1sec)
            
            self.last_plot_update = current_time
    
    def render_neuromorphic_canvas(self):
        """Render the main neuromorphic event canvas - matching C++ implementation"""
        # Dynamic canvas sizing based on available window space
        window_size = imgui.get_window_size()
        canvas_pos = imgui.get_cursor_screen_pos()
        
        # Use most of the window space for canvas, leaving room for controls
        controls_height = 200  # Space for stats/controls
        canvas_size = (
            window_size.x - 100,  # Leave 50px margin on each side
            window_size.y - canvas_pos.y - controls_height  # Leave space for controls below
        )
        
        # Ensure minimum canvas size
        if canvas_size[0] < 400.0:
            canvas_size = (400.0, canvas_size[1])
        if canvas_size[1] < 300.0:
            canvas_size = (canvas_size[0], 300.0)
        
        # Update canvas dimensions for coordinate mapping
        self.canvas_width = int(canvas_size[0])
        self.canvas_height = int(canvas_size[1])
        
        draw_list = imgui.get_window_draw_list()
        
        # Draw canvas background
        draw_list.add_rect_filled(
            canvas_pos[0], canvas_pos[1],
            canvas_pos[0] + canvas_size[0], canvas_pos[1] + canvas_size[1],
            imgui.get_color_u32_rgba(0.08, 0.08, 0.08, 1.0)
        )
        
        # Draw canvas border
        draw_list.add_rect(
            canvas_pos[0], canvas_pos[1],
            canvas_pos[0] + canvas_size[0], canvas_pos[1] + canvas_size[1],
            imgui.get_color_u32_rgba(0.4, 0.4, 0.4, 1.0)
        )

        # def draw_dot(dot):
        #     canvas_x, canvas_y = self.screen_to_canvas(dot['x'], dot['y'])
        #     screen_x = canvas_pos[0] + canvas_x
        #     screen_y = canvas_pos[1] + canvas_y
        #     draw_list.add_circle_filled(screen_x, screen_y, DOT_SIZE, color)

        #     if dot['polarity'] > 0:
        #         color = imgui.get_color_u32_rgba(0, int(255 * dot['alpha']), 0, 255)  # Green for positive
        #     else:
        #         color = imgui.get_color_u32_rgba(int(255 * dot['alpha']), 0, 0, 255)  # Red for negative

        #     draw_list.add_circle_filled(screen_x, screen_y, DOT_SIZE, color)

        # Parallel(n_jobs=-1)(delayed(draw_dot)(dot) for dot in self.active_dots)
        
        # Draw active dots - matching C++ implementation exactly
        for dot in self.active_dots:
            # Convert screen coordinates to canvas coordinates
            canvas_x, canvas_y = self.screen_to_canvas(dot['x'], dot['y'])
            
            screen_x = canvas_pos[0] + canvas_x
            screen_y = canvas_pos[1] + canvas_y
            
            # Color based on polarity with alpha - matching C++ exactly
            if dot['polarity'] > 0:
                color = imgui.get_color_u32_rgba(0, int(255 * dot['alpha']), 0, 255)  # Green for positive
            else:
                color = imgui.get_color_u32_rgba(int(255 * dot['alpha']), 0, 0, 255)  # Red for negative
            
            # Draw dot with constant size - matching C++ DOT_SIZE
            draw_list.add_circle_filled(screen_x, screen_y, DOT_SIZE, color)
        
        # Reserve space for canvas
        imgui.dummy(canvas_size[0], canvas_size[1])
    
    def render_statistics_panel(self):
        """Render statistics panel"""
        if imgui.collapsing_header("Statistics"):
            imgui.text(f"FPS: {self.performance_stats['fps']:.1f}")
            imgui.text(f"Events/sec: {self.performance_stats['events_per_sec']:.0f}")
            imgui.text(f"Active dots: {self.performance_stats['active_dots']}")
            imgui.text(f"Total events: {self.performance_stats['total_events']}")
            
            # Add debug information
            if len(self.active_dots) > 0:
                imgui.text(f"Sample event: ({self.active_dots[0]['x']}, {self.active_dots[0]['y']})")
                imgui.text(f"Sample polarity: {self.active_dots[0]['polarity']}")
                imgui.text(f"Sample alpha: {self.active_dots[0]['alpha']:.2f}")
            
            if imgui.button("Clear Events"):
                # Signal to clear events (would need event_data reference)
                pass
    
    def render_simple_plot_panel(self):
        """Render simple line plot of events per second vs time"""
        if imgui.collapsing_header("Events Per Second (5s window)"):
            if len(self.plot_times) > 1 and len(self.plot_event_counts) > 1:
                # Get ImGui drawing area
                draw_list = imgui.get_window_draw_list()
                canvas_pos = imgui.get_cursor_screen_pos()
                plot_width = 400
                plot_height = 150
                
                # Draw background
                draw_list.add_rect_filled(
                    canvas_pos[0], canvas_pos[1],
                    canvas_pos[0] + plot_width, canvas_pos[1] + plot_height,
                    imgui.get_color_u32_rgba(0.08, 0.08, 0.08, 1.0)
                )
                
                # Draw border
                draw_list.add_rect(
                    canvas_pos[0], canvas_pos[1],
                    canvas_pos[0] + plot_width, canvas_pos[1] + plot_height,
                    imgui.get_color_u32_rgba(0.4, 0.4, 0.4, 1.0)
                )
                
                # Calculate data ranges with proper bounds checking
                if self.plot_times and self.plot_event_counts:
                    start_time = self.plot_times[0]
                    rel_times = [(t - start_time) for t in self.plot_times]
                    
                    time_range = max(rel_times) - min(rel_times) if len(rel_times) > 1 else 5.0
                    count_range = max(self.plot_event_counts) - min(self.plot_event_counts) if len(self.plot_event_counts) > 1 else 1000
                    
                    if time_range == 0:
                        time_range = 5.0
                    if count_range == 0:
                        count_range = 1000
                    
                    # Draw line plot with proper bounds checking
                    points = []
                    for i in range(min(len(rel_times), len(self.plot_event_counts))):
                        # Scale to plot area with bounds checking
                        x = canvas_pos[0] + (rel_times[i] / time_range) * plot_width
                        y = canvas_pos[1] + plot_height - (self.plot_event_counts[i] / count_range) * plot_height
                        
                        # Clamp to plot bounds
                        x = max(canvas_pos[0], min(canvas_pos[0] + plot_width, x))
                        y = max(canvas_pos[1], min(canvas_pos[1] + plot_height, y))
                        
                        points.append((x, y))
                    
                    # Draw connected lines
                    for i in range(len(points) - 1):
                        draw_list.add_line(
                            points[i][0], points[i][1],
                            points[i + 1][0], points[i + 1][1],
                            imgui.get_color_u32_rgba(0, 1, 0, 1.0), 2.0
                        )
                    
                    # Draw current value indicator
                    if points:
                        last_point = points[-1]
                        draw_list.add_circle_filled(
                            last_point[0], last_point[1], 3.0,
                            imgui.get_color_u32_rgba(1, 1, 0, 1.0)
                        )
                
                # Reserve space for plot
                imgui.dummy(plot_width, plot_height)
                
                # Show current statistics
                imgui.text(f"Current: {self.performance_stats['events_per_sec']:.0f} events/sec")
                if self.plot_event_counts:
                    imgui.text(f"Max: {max(self.plot_event_counts):.0f} events/sec")
                    imgui.text(f"Avg: {sum(self.plot_event_counts) / len(self.plot_event_counts):.0f} events/sec")
            else:
                imgui.text("Collecting data...")


def init_glfw_imgui():
    """Initialize GLFW and ImGui"""
    if not glfw.init():
        print("Failed to initialize GLFW")
        return None, None
        
    # Create window
    glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
    glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
    glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
    
    window = glfw.create_window(1400, 900, "Neuromorphic UDP Event Visualizer", None, None)
    if not window:
        print("Failed to create GLFW window")
        glfw.terminate()
        return None, None
        
    glfw.make_context_current(window)
    glfw.swap_interval(1)  # Enable vsync
    
    # Initialize ImGui
    imgui.create_context()
    impl = imgui.integrations.glfw.GlfwRenderer(window)
    
    # Style
    imgui.style_colors_dark()
    
    return window, impl


def main_loop(window, impl, receiver, visualizer):
    """Main application loop"""
    while not glfw.window_should_close(window):
        glfw.poll_events()
        impl.process_inputs()
        
        # Update visualization
        visualizer.update_events(receiver.event_data)
        
        # Start ImGui frame
        imgui.new_frame()
        
        # Main window
        imgui.set_next_window_position(10, 10)
        imgui.set_next_window_size(1380, 880)
        
        expanded, opened = imgui.begin("Neuromorphic Event Visualizer", True, 
                                     imgui.WINDOW_NO_RESIZE | imgui.WINDOW_NO_MOVE)
        
        if expanded:
            # Left panel - main canvas
            imgui.columns(2, "main_columns")
            imgui.set_column_width(0, 850)
            
            imgui.text("Live Neuromorphic Screen Capture")
            imgui.separator()
            
            # Main neuromorphic canvas
            visualizer.render_neuromorphic_canvas()
            
            # Right panel - controls and statistics
            imgui.next_column()
            
            imgui.text("Controls & Statistics")
            imgui.separator()
            
            # Statistics panel
            visualizer.render_statistics_panel()
            
            # Simple plot panel  
            visualizer.render_simple_plot_panel()
            
            # Connection info
            if imgui.collapsing_header("Connection Info"):
                imgui.text(f"UDP Port: {receiver.port}")
                imgui.text(f"Status: {'Connected' if receiver.running else 'Disconnected'}")
                imgui.text(f"Packets: {receiver.event_data.stats['packets']}")
                imgui.text(f"Bytes: {receiver.event_data.stats['bytes']}")
        
        imgui.end()
        
        # Render
        glClearColor(0.1, 0.1, 0.1, 1.0)
        glClear(GL_COLOR_BUFFER_BIT)
        
        imgui.render()
        impl.render(imgui.get_draw_data())
        
        glfw.swap_buffers(window)
        
        if not opened:
            break


def main():
    parser = argparse.ArgumentParser(description='Neuromorphic UDP Event Visualizer')
    parser.add_argument('--port', type=int, default=9999,
                       help='UDP port for receiving events (default: 9999)')
    parser.add_argument('--width', type=int, default=1920,
                       help='Screen width for coordinate scaling (default: 1920)')
    parser.add_argument('--height', type=int, default=1080,
                       help='Screen height for coordinate scaling (default: 1080)')
    parser.add_argument('--gpu', action='store_true',
                       help='Enable GPU acceleration (requires CuPy)')
    
    args = parser.parse_args()
    
    print("Neuromorphic UDP Event Visualizer")
    print("================================")
    print(f"Port: {args.port}")
    print(f"Screen: {args.width}x{args.height}")
    print("Make sure to run the C++ streamer with: --UDP --port", args.port)
    print()
    
    # Initialize GLFW and ImGui
    window, impl = init_glfw_imgui()
    if not window or not impl:
        return 1
    
    # Create UDP receiver
    receiver = UDPEventReceiver(args.port)
    if not receiver.start():
        return 1
    
    # Create visualizer
    visualizer = NeuromorphicVisualizer(args.width, args.height, use_gpu=args.gpu)
    
    try:
        # Run main loop
        main_loop(window, impl, receiver, visualizer)
        
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        # Cleanup
        receiver.stop()
        
        # Print final statistics
        stats = receiver.event_data.stats
        print(f"\nFinal Statistics:")
        print(f"Packets received: {stats['packets']}")
        print(f"Events processed: {stats['events']}")
        print(f"Data received: {stats['bytes'] / 1024:.1f} KB")
        
        impl.shutdown()
        glfw.terminate()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())