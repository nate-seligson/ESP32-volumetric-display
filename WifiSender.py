import socket
import struct
from PolarRenderer import *

class WiFiSender:
    def __init__(self, stereo_stack, target_ip, target_port):
        self.stack = stereo_stack
        self.ip = target_ip
        self.port = target_port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def pack_activation(self, x, y, timing_us, side_flag):
        # x = pixel x, y = slice index (stack index)
        return struct.pack('<BBIB', x, y, int(timing_us), side_flag)

    def send_stack_earliest_sorted(self, batch_size=200):
        all_activations = []

        # Collect activations from entire stack
        for slice_index, slice_obj in enumerate(self.stack.slices):
            for p in slice_obj.pixels:
                x = p['x']
                y = slice_index  # Use slice index as Y
                if x >= 64 or y >= 32:
                    continue  # Filter out of bounds
                t_front = p['t_front_us']
                t_back = p['t_back_us']

                if t_front <= t_back:
                    all_activations.append((t_front, x, y, 0))
                else:
                    all_activations.append((t_back, x, y, 1))

        # Sort all activations by earliest timing
        all_activations.sort(key=lambda e: e[0])

        batch = bytearray()
        count = 0

        for timing_us, x, y, side in all_activations:
            batch += self.pack_activation(x, y, timing_us, side)
            count += 1
            if count >= batch_size:
                self.sock.sendto(batch, (self.ip, self.port))
                batch = bytearray()
                count = 0

        if batch:
            self.sock.sendto(batch, (self.ip, self.port))

stack = StereoPolarStack(grid_w=64, grid_h=64, slices=32, rpm=60)
sender = WiFiSender(stack, '192.168.1.212', 5005)
sender.send_stack_earliest_sorted()
