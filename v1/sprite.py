import json

def int_to_rgb(color):
    color_int = color
    red = (color_int >> 16) & 0xFF
    green = (color_int >> 8) & 0xFF
    blue = color_int & 0xFF
    if red+green+blue == 255 * 3:
        return (100, 100, 0)
    return (red, green, blue)

class Sprite:
    def __init__(self, name):
        # Open a JSON file named "<name>.json" and load its content.
        with open(f"{name}.json", "r") as file:
            data = json.load(file)
            self.w = data["dimensions"]["w"]
            self.h = data["dimensions"]["h"]
            self.l = data["dimensions"]["l"]

            # Map each pixel from its integer representation to an RGB tuple.
            self.pixels =[
                    [
                        [int_to_rgb(int(pixel)) if pixel != None else None for pixel in z]
                        for z in y
                    ]
                    for y in data["data"]
                ]
