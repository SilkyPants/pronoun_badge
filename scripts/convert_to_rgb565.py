import sys
import os
import argparse
from PIL import Image

def output_image_bin(img: Image.Image, output_path: str):
    # Binary Output Logic
    width, height = img.size
    with open(output_path, 'wb') as f:
        for y in range(height):
            for x in range(width):
                r, g, b = img.getpixel((x, y))
                # RGB888 to RGB565
                rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                # Byte swap for Big Endian (common for many TFT controllers)
                # be_rgb565 = ((rgb565 & 0xFF) << 8) | ((rgb565 >> 8) & 0xFF)
                # Pack into two bytes
                f.write(rgb565.to_bytes(2, byteorder='big'))

def output_image_code_header(img: Image.Image, output_path: str, input_path: str, data_type: str):
    # Clean variable name for the C array (removes extensions/dashes)
    var_name = os.path.splitext(os.path.basename(input_path))[0].replace("-", "_").replace(" ", "_")

    width, height = img.size
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

def convert_image(input_path:str, output_path:str, data_type:str, format:str):
    if not os.path.exists(input_path):
        print(f"Error: File '{input_path}' not found.")
        return

    # Open image and ensure it's RGB
    img = Image.open(input_path).convert('RGB')

    if format == "bin":
        output_image_bin(img, output_path)
    else: # Default to code headers
        output_image_code_header(img, output_path, input_path, data_type)
    
    print(f"Done! Created {data_type} array at: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert PNG to RGB565 C Array or Binary")
    parser.add_argument("input", help="Input PNG file")
    parser.add_argument("-o", "--output", help="Output file path", default=None)
    parser.add_argument("-t", "--type", choices=['uint8_t', 'uint16_t'], default='uint16_t', 
                        help="Data type for C array (ignored for binary format)")
    parser.add_argument("-f", "--format", choices=['h', 'bin'], default='h',
                        help="Output format: 'h' for header, 'bin' for raw binary (default: h)")

    args = parser.parse_args()
    
    # Handle default output extensions
    if not args.output:
        ext = ".bin" if args.format == "bin" else ".h"
        args.output = os.path.splitext(args.input)[0] + ext

    convert_image(args.input, args.output, args.type, args.format)