import socket
import struct
import imgui
from imgui.integrations.pygame import PygameRenderer
import pygame
import time

# Constants
WIDTH, HEIGHT = 1920, 1080
DOT_SIZE = 3
MAX_EVENTS = 10000
FADE_DURATION = 0.1  # seconds

class UDPVisualizer:
    def __init__(self, ip="127.0.0.1", port=9999):
        self.ip = ip
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.ip, self.port))
        self.sock.setblocking(False)

        self.active_dots = []  # list of dicts: {x, y, polarity, timestamp, alpha}
        self.event_buffer = []
        self.duplicates = 0
        self.processed = 0

        self.clear_requested = False
        self.debug_output = True

    def update(self):
        try:
            while True:
                data, _ = self.sock.recvfrom(1500 * 13)
                events = self.parse_events(data)
                self.event_buffer.extend(events)
        except BlockingIOError:
            pass

        current_time = time.time()
        new_active = []
        seen = set()

        for ev in self.event_buffer:
            key = (ev['x'], ev['y'])
            if key in seen:
                self.duplicates += 1
                continue
            seen.add(key)

            ev['timestamp'] = current_time
            ev['alpha'] = 1.0
            new_active.append(ev)

        self.processed += len(new_active)
        self.active_dots.extend(new_active)
        self.event_buffer.clear()

        # Fade out dots
        self.active_dots = [dot for dot in self.active_dots if current_time - dot['timestamp'] <= FADE_DURATION]
        for dot in self.active_dots:
            dot['alpha'] = max(0.0, 1.0 - (current_time - dot['timestamp']) / FADE_DURATION)

        if self.clear_requested:
            self.active_dots.clear()
            self.clear_requested = False

    def parse_events(self, data):
        events = []
        event_size = 13
        for i in range(0, len(data), event_size):
            if i + event_size > len(data):
                break
            timestamp, x, y, polarity = struct.unpack_from('<QHHb', data, i)
            x = min(max(x, 0), WIDTH - 1)
            y = min(max(y, 0), HEIGHT - 1)
            events.append({'timestamp': timestamp, 'x': x, 'y': y, 'polarity': polarity})
        return events

    def render(self, canvas_pos, draw_list, scale_x, scale_y):
        for dot in self.active_dots:
            x, y = dot['x'], dot['y']
            canvas_x = x * scale_x
            canvas_y = y * scale_y
            screen_x = canvas_pos[0] + canvas_x
            screen_y = canvas_pos[1] + canvas_y
            if dot['polarity'] > 0:
                color = imgui.get_color_u32_rgba(0, int(255 * dot['alpha']), 0, 255)
            else:
                color = imgui.get_color_u32_rgba(int(255 * dot['alpha']), 0, 0, 255)
            draw_list.add_circle_filled(screen_x, screen_y, DOT_SIZE, color)

    def print_stats(self):
        if self.debug_output:
            print(f"Streaming GUI: {len(self.active_dots)} active dots, buffer: {len(self.event_buffer)}, processed: {self.processed}, duplicates: {self.duplicates}")


def main():
    pygame.init()
    screen = pygame.display.set_mode((1280, 800), pygame.DOUBLEBUF | pygame.OPENGL)
    imgui.create_context()
    impl = PygameRenderer()
    visualizer = UDPVisualizer()

    running = True
    last_print = time.time()

    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            impl.process_event(event)

        imgui.new_frame()
        imgui.begin("Neuromorphic UDP Viewer")

        canvas_pos = imgui.get_cursor_screen_pos()
        canvas_size = imgui.get_content_region_avail()
        scale_x = canvas_size.x / WIDTH
        scale_y = canvas_size.y / HEIGHT

        draw_list = imgui.get_window_draw_list()
        draw_list.add_rect_filled(canvas_pos[0], canvas_pos[1], canvas_pos[0]+canvas_size.x, canvas_pos[1]+canvas_size.y, imgui.get_color_u32_rgba(0.1, 0.1, 0.1, 1.0))

        visualizer.update()
        visualizer.render(canvas_pos, draw_list, scale_x, scale_y)

        if imgui.button("Clear Events"):
            visualizer.clear_requested = True

        if imgui.button("Toggle Debug Output"):
            visualizer.debug_output = not visualizer.debug_output

        if time.time() - last_print > 2.0:
            visualizer.print_stats()
            last_print = time.time()

        imgui.end()
        imgui.render()
        impl.render(imgui.get_draw_data())
        pygame.display.flip()

    impl.shutdown()
    pygame.quit()


if __name__ == "__main__":
    main()
