#!/usr/bin/python3

import sys
import json
import socket
import os

ESP32_IP = "192.168.1.128"
ESP32_PORT = 8888

def send_pixel_data(pixel_data):
    print(pixel_data)
    try:
        messages = [
            f"{p[0]},{p[1]},{p[2]},{p[3]},{p[4]}\n"
            for p in pixel_data
        ]
        payload = "".join(messages).encode()

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ESP32_IP, ESP32_PORT))
            s.sendall(payload)
            print(f"Sent {len(pixel_data)} pixels to ESP32")
    except Exception as e:
        print(f"Error: {e}")
        return False
    return True

# Send CORS headers first
print("Access-Control-Allow-Origin: *")
print("Access-Control-Allow-Methods: POST, OPTIONS")
print("Access-Control-Allow-Headers: Content-Type")
print("Content-Type: application/json\n")

# Handle OPTIONS preflight request
if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
    # Just return headers and no content for preflight
    sys.exit(0)

if os.environ.get('REQUEST_METHOD') == 'POST':
    try:
        length = int(os.environ.get('CONTENT_LENGTH', 0))
        post_data = sys.stdin.read(length)
        data = json.loads(post_data)

        print(f"Received data: {data}")

        result = send_pixel_data(data['grid'])

        if result:
            response = {'status': 'success', 'message': 'Pixel data sent successfully'}
        else:
            response = {'status': 'error', 'message': 'Failed to send pixel data'}

    except Exception as e:
        response = {'status': 'error', 'message': f'Error processing request: {str(e)}'}

else:
    response = {'status': 'error', 'message': 'Invalid HTTP method. Only POST is allowed.'}

print(json.dumps(response))
