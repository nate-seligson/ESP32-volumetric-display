import socket

# Replace with your ESP32's IP address.
HOST = "192.168.1.186"
PORT = 8888

def main():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((HOST, PORT))
            print("Connected to ESP32. Sending 'setup' command...")
            # Send "setup" followed by a newline character as expected by the ESP32 code.
            sock.sendall(b"setup\n")
            
            print("Waiting for readings...")
            while True:
                data = sock.recv(1024)
                if not data:
                    break
                print("RPM Reading:", data.decode().strip())
    except Exception as e:
        print("Oops! Something went wrong:", e)

if __name__ == "__main__":
    main()
