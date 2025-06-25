import math
import socket
import time
import struct
import select
from sprite import Sprite
pixel_count = 24
pixel_distance = 2

class Renderer:
    def __init__(self, pixel_count=pixel_count, pixel_distance=pixel_distance, rpm=3000, LEDs_x=64, LEDs_y=32):
        # rotation per minute and per second
        self.rpm = rpm / 2
        self.rps = self.rpm / 60
        self.sock = None
        self.pixel_fmt = struct.Struct('BBBBB')  # Precompile struct
        # convert grid of pixels to a render queue -- after some seconds to wait, display this screen 
        print("Building render dictionary...")
        self.renderDict = {}

        for a in range(-pixel_count, pixel_count, pixel_distance):
            for b in range(-pixel_count, pixel_count, pixel_distance):
                bottom = b < 0 if b != 0 else a < 0
                a_squared = (a ** 2)
                b_squared = (b ** 2)
                # constrain pixel_x to grid limits
                pixel_x = min(max(round(math.sqrt(a_squared + b_squared)), 0), LEDs_x)
                
                # calculate angle at which pixel should be displayed
                angle = math.atan2(b, a)
                if angle <= 0:
                    angle += (2 * math.pi)

                if bottom:
                    angle -= math.pi
                    pixel_x *= -1
                # calculate timing (ms) for this pixel
                time_for_one_rotation = (1 / self.rps * 10000)  # for half the screen
                ratio = angle / (2 * math.pi)
                timing_final_ms = int(time_for_one_rotation * ratio)

                if angle > math.pi:
                    pixel_x *= -1
                    timing_final_ms -= (time_for_one_rotation / 2)
                # add to render dict
                position_x = (a + pixel_count)
                position_y = (b + pixel_count)
                data = {"activePixel": pixel_x, "x": position_x, "y": position_y}
                if timing_final_ms in self.renderDict:
                    self.renderDict[timing_final_ms].append(data)
                else:
                    self.renderDict[timing_final_ms] = [data]

    def createPipeline(self, graphicsSlice, y):
        pipeline = {}
        for delay, values in self.renderDict.items():
            for data in values:
                x, z = (data['x'], data['y'])
                if (x, z) in graphicsSlice.keys():
                    color = graphicsSlice[(x, z)]
                    if delay not in pipeline.keys():
                        pipeline[delay] = [((data["activePixel"], y), color)]
                    else:
                        pipeline[delay] += [((data["activePixel"], y), color)]
        return pipeline

    def convertToTiming(self, scene):
        # create "master pipeline" from all XZ planes and combine them
        master_pipeline = {}
        for y in range(len(scene[0])):
            graphicsSlice = {}
            for x in range(len(scene)):
                for z in range(len(scene[x][y])):
                    if scene[x][y][z] is not None:
                        graphicsSlice[(x, z)] = scene[x][y][z]
            if y == 0:
                master_pipeline = self.createPipeline(graphicsSlice, y)
            else:
                for key, value in self.createPipeline(graphicsSlice, y).items():
                    if key not in master_pipeline.keys():
                        master_pipeline[key] = value
                    else:
                        master_pipeline[key] += value
        # sort master pipeline & simplify delays so one comes after the other
        ordered_keys = sorted(master_pipeline.keys())
        subtractor = 0
        timings = []
        positions = []
        for key in ordered_keys:
            timings.append(key - subtractor)
            positions.append(master_pipeline[key])
            subtractor = key
        return positions, timings

    def sendToDevice(self, scene, host='192.168.1.212', port=8888):
        positions, timings = self.convertToTiming(scene)
        print(sum(timings))
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                self.sock = sock
                # Enable TCP_NODELAY to bypass Nagle’s algorithm (minimizes delays)
                self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                self.sock.settimeout(10)  # Default timeout for general operations
                self.sock.connect((host, port))
                print(f"Connected to {host}:{port}")

                # Send initialization ping immediately (no sleep)
                self.sock.sendall(b"loop\n")
                while True:
                    self.send_positions_over_wifi(positions,timings)
        except (ConnectionRefusedError, socket.timeout) as e:
            print(f"Network error: {str(e)}")
        except Exception as e:  
            print(f"Unexpected error: {str(e)}")

    def process_frames(self, positions, timings):
        sock = self.sock
        check_reset = lambda: select.select([sock], [], [], 0)[0]
        for frame, timing in zip(positions, timings):
            if check_reset():  # Reset detected
                if sock.recv(1) == b'\x00':
                    return True
            
            frame_delay = max(0, min(int(timing), 1500))

            # Build buffer efficiently
            buffer = bytearray()
            buffer_extend = buffer.extend  # Local reference for speed
            pack = self.pixel_fmt.pack  # Local reference for speed

            for (x, y), (r, g, b) in frame:
                panel_x = max(0, min((x) + 31, 63))
                panel_y = max(0, min(y, 31))
                buffer_extend(pack(panel_x, panel_y, r, g, b))

            buffer.extend(frame_delay.to_bytes(2, 'big'))
            buffer.extend(b'\xFF\xFF')

            sock.sendall(buffer)  # Send everything at once

        return False

    def send_positions_over_wifi(self, positions, timings):
        sock = self.sock
        while not self.process_frames(positions, timings):
            ready = select.select([sock], [], [], 1)[0]  # Wait for 1 second for any reset signal
            if ready:
                reset_signal = sock.recv(1)  # Read 1 byte to check if there's a reset signal
                if reset_signal == b'\x00':
                    break  # Reset detected, exit the loop

if __name__ == "__main__":
    # Example usage with WiFi parameters
    r = Renderer(rpm=5000)
    scene = [[[(0, 0, 0) if z < 32 else (0,255,0) for z in range(pixel_count * 2)] for _ in range(32)] for _ in range(pixel_count * 2)]
    # Send to device (replace IP with your ESP32's IP)
    r.sendToDevice(scene)   