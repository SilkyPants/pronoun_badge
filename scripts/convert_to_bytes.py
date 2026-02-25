import sys
import os
import argparse
from PIL import Image

def convert_image(input_path, output_path, data_type):
    if not os.path.exists(input_path):
        print(f"Error: File '{input_path}' not found.")
        return

    # Open image and ensure it's RGB
    img = Image.open(input_path).convert('RGB')
    width, height = img.size
    
    # Clean variable name for the C array (removes extensions/dashes)
    var_name = os.path.splitext(os.path.basename(input_path))[0].replace("-", "_").replace(" ", "_")

    with open(output_path, 'w') as f:
        f.write(f"// Generated from: {input_path}\n")
        f.write(f"// Dimensions: {width}x{height}\n")
        
        if data_type == 'uint8_t':
            # Stores two bytes per pixel: [HighByte, LowByte, HighByte, LowByte...]
            f.write(f"const uint8_t {var_name}_bits[] PROGMEM = {{\n")
            for y in range(height):
                row = []
                for x in range(width):
                    r, g, b = img.getpixel((x, y))
                    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    # Split into two bytes (Big Endian swap included)
                    high_byte = rgb565 & 0xFF
                    low_byte = (rgb565 >> 8) & 0xFF
                    row.append(f"0x{high_byte:02X}, 0x{low_byte:02X}")
                f.write("  " + ", ".join(row) + ",\n")
        
        else: # uint16_t
            f.write(f"const uint16_t {var_name}_bits[] PROGMEM = {{\n")
            for y in range(height):
                row = []
                for x in range(width):
                    r, g, b = img.getpixel((x, y))
                    # RGB888 to RGB565
                    rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                    # Byte swap for TFT_eSPI (Big Endian)
                    rgb565 = ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)
                    row.append(f"0x{rgb565:04X}")
                f.write("  " + ", ".join(row) + ",\n")

        f.write("};\n")
    
    print(f"Done! Created {data_type} array at: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert PNG to RGB565 C Array for TFT_eSPI")
    parser.add_argument("input", help="Input PNG file")
    parser.add_argument("-o", "--output", help="Output .h file path", default=None)
    parser.add_argument("-t", "--type", choices=['uint8_t', 'uint16_t'], default='uint16_t', 
                        help="Data type of the array (default: uint16_t)")

    args = parser.parse_args()
    
    # Default output name if none provided
    if not args.output:
        args.output = os.path.splitext(args.input)[0] + ".h"

    convert_image(args.input, args.output, args.type)