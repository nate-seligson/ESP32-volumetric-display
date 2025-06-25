import socket
import time

def send_frame(ip: str, port: int, frame_data: bytes):
    """Send a 64x32 1bpp frame (256 bytes)"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(frame_data, (ip, port))

def create_test_frame():
    """Create a test pattern (alternating horizontal lines)"""
    return bytes([0xFF if y % 2 else 0x00 for y in range(32) for _ in range(8)])

if __name__ == "__main__":
    TARGET_IP = "192.168.1.212"  # Change to your ESP32's IP
    TARGET_PORT = 5005
    
    while True:
        frame = create_test_frame()
        send_frame(TARGET_IP, TARGET_PORT, frame)
        print(f"Sent frame - {len(frame)} bytes")