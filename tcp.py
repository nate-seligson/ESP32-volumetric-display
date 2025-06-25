import math
import socket
import struct
import time
pixel_count = 24
pixel_distance = 2
PANEL_W = 64
PANEL_H = 32
ROW_BYTES = PANEL_W // 8
FRAME_SIZE = PANEL_H * ROW_BYTES

class Renderer:
    def __init__(self, pixel_count=pixel_count, pixel_distance=pixel_distance, rpm=3000):
        self.rpm = rpm / 2
        self.rps = self.rpm / 60
        print("Building render dictionary...")
        self.renderDict = {}

        for a in range(-pixel_count, pixel_count, pixel_distance):
            for b in range(-pixel_count, pixel_count, pixel_distance):
                bottom = b < 0 if b != 0 else a < 0
                a_squared = (a ** 2)
                b_squared = (b ** 2)
                pixel_x = min(max(round(math.sqrt(a_squared + b_squared)), 0), PANEL_W)
                angle = math.atan2(b, a)
                if angle <= 0:
                    angle += (2 * math.pi)

                if bottom:
                    angle -= math.pi
                    pixel_x *= -1

                time_for_one_rotation = (1 / self.rps * 10000)
                ratio = angle / (2 * math.pi)
                timing_final_ms = int(time_for_one_rotation * ratio)

                if angle > math.pi:
                    pixel_x *= -1
                    timing_final_ms -= (time_for_one_rotation / 2)

                position_x = (a + pixel_count)
                position_y = (b + pixel_count)
                data = {"activePixel": pixel_x, "x": position_x, "y": position_y}
                self.renderDict.setdefault(timing_final_ms, []).append(data)

    def createPipeline(self, graphicsSlice, y):
        pipeline = {}
        for delay, values in self.renderDict.items():
            for data in values:
                x, z = data['x'], data['y']
                if (x, z) in graphicsSlice.keys():
                    color = graphicsSlice[(x, z)]
                    pipeline.setdefault(delay, []).append(((data["activePixel"], y), color))
        return pipeline

    def convertToTiming(self, scene):
        master_pipeline = {}
        for y in range(len(scene[0])):
            graphicsSlice = {}
            for x in range(len(scene)):
                for z in range(len(scene[x][y])):
                    if scene[x][y][z] is not None:
                        graphicsSlice[(x, z)] = scene[x][y][z]
            pipe = self.createPipeline(graphicsSlice, y)
            for key, value in pipe.items():
                master_pipeline.setdefault(key, []).extend(value)

        ordered_keys = sorted(master_pipeline.keys())
        subtractor = 0
        timings = []
        positions = []
        for key in ordered_keys:
            timings.append(key - subtractor)
            positions.append(master_pipeline[key])
            subtractor = key
        return positions, timings

    def frame_to_monochrome_buffer(self, frame, mirror_left=False):
        # Create empty framebuffer for one monochrome frame
        buffer = bytearray(FRAME_SIZE)  # 256 bytes

        for (x, y), color in frame:
            brightness = sum(color) / 3
            if brightness > 0:
                # Clip coordinates to panel limits
                if 0 <= x < PANEL_W and 0 <= y < PANEL_H:
                    # If mirror_left is True and pixel in left half, mirror x to right half
                    if mirror_left and x < PANEL_W // 2:
                        mirror_x = PANEL_W - 1 - x  # mirror horizontally
                        byte_index = y * ROW_BYTES + (mirror_x // 8)
                        bit_mask = 0x80 >> (mirror_x % 8)
                    else:
                        byte_index = y * ROW_BYTES + (x // 8)
                        bit_mask = 0x80 >> (x % 8)
                    buffer[byte_index] |= bit_mask
        return buffer

    def sendToDevice(self, scene, host='192.168.1.212', port=5005):
        positions, timings = self.convertToTiming(scene)
        half_cycle_time = max(timings) // 2  # half rotation in timing units
        mirror = False
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                sock.connect((host, port))
                print(f"Connected to {host}:{port}")

                elapsed = 0
                start_time = time.time()

                while True:
                    for frame, delay_ms in zip(positions, timings):
                        buffer = self.frame_to_monochrome_buffer(frame, mirror_left=mirror)
                        sock.sendall(buffer)

                        # Sleep according to frame delay
                        time.sleep(delay_ms / 10000.0)
                        elapsed += delay_ms

                        # Flip mirror flag every half cycle time
                        if elapsed >= half_cycle_time:
                            mirror = not mirror
                            elapsed = 0

        except Exception as e:
            print(f"Error: {e}")


if __name__ == "__main__":
    r = Renderer(rpm=10)
    # Dummy scene with some pixels lit
    scene = [[[(0,0,0) if z < 32 else (255,255,255) for z in range(pixel_count*2)] for _ in range(32)] for _ in range(pixel_count*2)]
    r.sendToDevice(scene)
