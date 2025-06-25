import math
import time
import socket
class StereoPolarStackFlat:
    def __init__(self, grid_w=32, grid_h=32, slices=32, rpm=60):
        self.grid_w = grid_w
        self.grid_h = grid_h
        self.slices = slices
        self.rpm = rpm
        self.rps = rpm / 60.0
        self.period_us = (1000.0 / self.rps) * 1000
        self.cx = (grid_w - 1) / 2
        self.cy = (grid_h - 1) / 2

    def get_slice_pixels(self, slice_index):
        pixels = []
        for x in range(self.grid_w):
            for orig_y in range(self.grid_h):
                dx = x - self.cx
                dy = orig_y - self.cy
                theta = math.atan2(dy, dx)
                if theta < 0:
                    theta += 2 * math.pi
                # front pixel
                pixels.append({'x': x, 'y': slice_index})
                # back pixel
                pixels.append({'x': x + self.grid_w, 'y': slice_index})
        return pixels

    def get_frame_buffer(self, slice_index):
        width = 64
        height = 32
        frame = bytearray(width * height // 8)

        pixels = self.get_slice_pixels(slice_index)
        for p in pixels:
            x, y = p['x'], p['y']
            if 0 <= x < width and 0 <= y < height:
                byte_index = y * (width // 8) + (x // 8)
                bit_pos = 7 - (x % 8)
                frame[byte_index] |= (1 << bit_pos)
        return bytes(frame)

def send_frame(ip: str, port: int, frame_data: bytes):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(frame_data, (ip, port))
if __name__ == "__main__":
    TARGET_IP = "192.168.1.212"
    TARGET_PORT = 5005

    stack = StereoPolarStackFlat(grid_w=32, grid_h=32, slices=32, rpm=60)

    slice_index = 0
    while True:
        frame = stack.get_frame_buffer(slice_index)
        send_frame(TARGET_IP, TARGET_PORT, frame)
        print(f"Sent slice {slice_index} frame")
        slice_index = (slice_index + 1) % stack.slices
