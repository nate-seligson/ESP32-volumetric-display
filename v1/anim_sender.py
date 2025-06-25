import json
import socket
import time

ESP32_IP = "192.168.1.28"  # Update to your ESP32's IP
ESP32_PORT = 8888
PANEL_WIDTH = 64
PANEL_HEIGHT = 32

def send_frame(sock, frame, full_frame=True):
    buffer = []
    
    if full_frame:
        for i, color in enumerate(frame):
            x = i % PANEL_WIDTH
            y = i // PANEL_WIDTH
            if color == 0:
                flag1, flag2 = 1, 0
            elif color == 1:
                flag1, flag2 = 0, 1
            else:
                flag1, flag2 = 0, 0
            buffer.append(f"{x},{y},{flag1},0,{flag2}\n")
    else:
        for change in frame:
            idx = change.get("index")
            color = change.get("color")
            if idx is None:
                continue
            x = idx % PANEL_WIDTH
            y = idx // PANEL_WIDTH
            if color == 0:
                flag1, flag2 = 1, 0
            elif color == 1:
                flag1, flag2 = 0, 1
            else:
                flag1, flag2 = 0, 0
            buffer.append(f"{x},{y},{flag1},0,{flag2}\n")
    
    # Send all data in one batch
    sock.sendall(''.join(buffer).encode())

def main():
    with open("hologram-animation.json", "r") as f:
        data = json.load(f)
    
    # Create a persistent socket connection
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((ESP32_IP, ESP32_PORT))
        print("Connected to ESP32!")
        
        # Send full first frame
        print("Sending full frame...")
        send_frame(sock, data[0], full_frame=True)
        
        # Send subsequent partial frames
        for frame in data[1:]:
            print("Sending partial frame...")
            send_frame(sock, frame, full_frame=False)
            time.sleep(1)  # Optional: Add a small delay between frames
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        # Close the socket connection
        sock.close()
        print("Socket connection closed.")

if __name__ == "__main__":
    while True:
        main()
        time.sleep(3)