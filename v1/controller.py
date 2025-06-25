import socket

# Replace with your ESP32's IP address and port
ESP32_IP = "192.168.1.186"
ESP32_PORT = 8888

def send_command(command):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        try:
            sock.connect((ESP32_IP, ESP32_PORT))
            # Add newline to match the server's readStringUntil('\n')
            message = f"{command}\n"
            sock.sendall(message.encode())
            print(f"Sent com    mand: {command}")
        except Exception as e:
            print(f"Error connecting to ESP32: {e}")

def main(): 
    print("Connecting to ESP32...")
    while True:
        command = input("Enter a mode (1, 2, or 3) or 'q' to quit: ").strip()
        if command.lower() == 'q':
            print("Quitting. May the remote mode-switching force be with you!")
            break
        elif command in ["1", "2", "3"]:
            send_command(command)
        else:
            print("Invalid input. Please enter '1', '2', '3', or 'q' to quit.")

if __name__ == '__main__':
    main()