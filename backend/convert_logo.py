import sys
from PIL import Image

def generate_c_array(image_path, output_path):
    img = Image.open(image_path).convert("RGBA")
    
    # Create a BLACK background to handle transparent eyes and background
    bg = Image.new("RGBA", img.size, (0, 0, 0, 255))
    bg.paste(img, mask=img)
    img = bg.convert("L") # Grayscale
    
    # Resize keeping aspect ratio, height = 64
    aspect = img.width / img.height
    new_w = int(64 * aspect)
    img = img.resize((new_w, 64), Image.Resampling.LANCZOS)
    
    canvas = [0] * (128 * 64)
    x_offset = (128 - new_w) // 2
    
    for y in range(64):
        for x in range(new_w):
            px = img.getpixel((x, y))
            if px > 128: # Face is white (LIT)
                canvas_x = x + x_offset
                if 0 <= canvas_x < 128:
                    canvas[y * 128 + canvas_x] = 1
                    
    # Now convert to SH1106 memory format (8 pages of 128 columns)
    buffer = bytearray(1024)
    for p in range(8):
        for x in range(128):
            byte = 0
            for bit in range(8):
                y = p * 8 + bit
                if canvas[y * 128 + x]:
                    byte |= (1 << bit)
            buffer[p * 128 + x] = byte
            
    # Write to C array
    with open(output_path, "w") as f:
        f.write("const uint8_t rabbit_logo_bmp[1024] = {\n")
        for i in range(0, 1024, 16):
            chunk = buffer[i:i+16]
            f.write("    " + ", ".join([f"0x{b:02X}" for b in chunk]) + ",\n")
        f.write("};\n")
        
if __name__ == "__main__":
    img_path = sys.argv[1]
    generate_c_array(img_path, "rabbit_logo.h")
    print("Generated rabbit_logo.h")
