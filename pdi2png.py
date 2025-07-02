import struct
import zlib
import sys
from PIL import Image

# see ~/src/pdi.h for struct details

PDI_FLAG_COMPRESSED = 0x80000000
PDI_CELL_FLAG_TRANSPARENCY = 3

def read_pdi_to_png(pdi_path, png_path):
    with open(pdi_path, 'rb') as f:
        # Read PDIHeader
        header = f.read(16)  # 12 (magic) + 4 (flags)
        magic, flags = struct.unpack('<12sI', header)
        magic = magic.decode('ascii').strip('\x00')
        
        print("PDI Header:")
        print(f"  Magic: {magic}")
        print(f"  Flags: 0x{flags:08x}")
        if flags & PDI_FLAG_COMPRESSED:
            print("  Image is compressed")
        
        # Read PDIMetadata if compressed
        if flags & PDI_FLAG_COMPRESSED:
            metadata = f.read(16)
            size, width, height, reserved = struct.unpack('<IIII', metadata)
            print("\nPDI Metadata:")
            print(f"  Compressed size: {size}")
            print(f"  Width: {width}")
            print(f"  Height: {height}")
            print(f"  Reserved: {reserved}")
            
            # Read compressed data
            compressed_data = f.read(size)
            decompressed_data = zlib.decompress(compressed_data)
            
            # Use a BytesIO to read from decompressed data
            from io import BytesIO
            f = BytesIO(decompressed_data)
        
        # Read PDICell
        cell_data = f.read(16)
        (clip_width, clip_height, stride, clip_left, clip_right, 
         clip_top, clip_bottom, cell_flags) = struct.unpack('<HHHHHHHH', cell_data)
        
        print("\nPDI Cell:")
        print(f"  Clip Width: {clip_width}")
        print(f"  Clip Height: {clip_height}")
        print(f"  Stride: {stride}")
        print(f"  Clip Left: {clip_left}")
        print(f"  Clip Right: {clip_right}")
        print(f"  Clip Top: {clip_top}")
        print(f"  Clip Bottom: {clip_bottom}")
        print(f"  Cell Flags: 0x{cell_flags:04x}")
        has_transparency = (cell_flags & PDI_CELL_FLAG_TRANSPARENCY) == PDI_CELL_FLAG_TRANSPARENCY
        print(f"  Has Transparency: {has_transparency}")
        
        # Calculate full image dimensions
        full_width = clip_left + clip_width + clip_right
        full_height = clip_top + clip_height + clip_bottom
        print(f"\nFull Image Dimensions: {full_width}x{full_height}")
        
        # Read white (color) data
        white_data_size = stride * clip_height
        white_data = f.read(white_data_size)
        
        # Read opaque (alpha) data if present
        opaque_data = None
        if has_transparency:
            opaque_data = f.read(white_data_size)
        
        # Create image
        img = Image.new('RGBA', (full_width, full_height), (0, 0, 0, 0))
        pixels = img.load()
        
        # Process each pixel
        for y in range(clip_height):
            for x in range(clip_width):
                # Calculate byte and bit positions
                byte_pos = y * stride + x // 8
                bit_pos = 7 - (x % 8)
                
                # Get white (color) value
                if byte_pos < len(white_data):
                    white_byte = white_data[byte_pos]
                    white = (white_byte >> bit_pos) & 1
                else:
                    white = 0
                
                # Get alpha value
                alpha = 255
                if has_transparency:
                    if byte_pos < len(opaque_data):
                        alpha_byte = opaque_data[byte_pos]
                        alpha = 255 if ((alpha_byte >> bit_pos) & 1) else 0
                
                # Calculate final position with clipping
                img_x = clip_left + x
                img_y = clip_top + y
                
                if 0 <= img_x < full_width and 0 <= img_y < full_height:
                    color = 255 if white else 0
                    pixels[img_x, img_y] = (color, color, color, alpha)
        
        # Save PNG
        img.save(png_path)
        print(f"\nSuccessfully saved PNG to {png_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python pdi_to_png.py <input.pdi> <output.png>")
        sys.exit(1)
    
    input_pdi = sys.argv[1]
    output_png = sys.argv[2]
    
    try:
        read_pdi_to_png(input_pdi, output_png)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)