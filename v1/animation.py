from flask import Flask, request, jsonify
from flask_cors import CORS
import json
import os

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

ANIMATION_FILE = "hologram-animation.json"

def compute_delta(previous_frame, current_frame):
    delta = []
    for index, (prev_color, current_color) in enumerate(zip(previous_frame, current_frame)):
        if prev_color != current_color:
            delta.append({"index": index, "color": current_color})
    return delta

def reconstruct_last_frame(animation):
    """
    Reconstruct the last full frame by starting with the initial full frame and
    applying all subsequent delta frames.
    """
    # Make a shallow copy of the initial full frame
    last_frame = animation[0].copy()
    # Apply each delta change
    for delta in animation[1:]:
        for change in delta:
            idx = change["index"]
            last_frame[idx] = change["color"]
    return last_frame

@app.route('/save', methods=['POST']) 
def save_data():
    try:
        data = request.json
        current_pixels = data.get('pixels', [])

        if not current_pixels:
            return jsonify({"error": "No pixel data received"}), 400

        if not os.path.exists(ANIMATION_FILE):
            # Save initial frame
            animation = [current_pixels]
            with open(ANIMATION_FILE, "w") as f:
                json.dump(animation, f, indent=4)
            return jsonify({"message": "Initial frame saved successfully"}), 200
        else:
            # Load existing animation data
            with open(ANIMATION_FILE, "r") as f:
                animation = json.load(f)
            # Reconstruct the last full frame from the animation
            last_frame = reconstruct_last_frame(animation)

            # Compute the delta relative to the last frame
            delta = compute_delta(last_frame, current_pixels)

            # Append the new delta frame only if there are changes
            if delta:
                animation.append(delta)
                with open(ANIMATION_FILE, "w") as f:
                    json.dump(animation, f, indent=4)
                return jsonify({"message": "Delta frame saved successfully"}), 200
            else:
                return jsonify({"message": "No changes detected"}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host="0.0.0.0", port=6969, debug=True)
