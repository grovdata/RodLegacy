# /// script
# dependencies = [
#   "subprocess",
# ]
# ///

#!/usr/bin/env python3
"""
Convert PNG icon to ICO format using ImageMagick
"""

import sys
import os
import subprocess

def convert_png_to_ico(png_path, ico_path):
    """Convert PNG to ICO format using ImageMagick"""
    try:
        # Use ImageMagick convert command
        cmd = [
            'convert', 
            png_path,
            '-resize', '16x16', '16x16.png',
            '-resize', '32x32', '32x32.png', 
            '-resize', '48x48', '48x48.png',
            '-resize', '64x64', '64x64.png',
            '-resize', '128x128', '128x128.png',
            '-resize', '256x256', '256x256.png',
            ico_path
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"ImageMagick error: {result.stderr}")
            sys.exit(1)
            
        print(f"Successfully converted {png_path} to {ico_path}")
        
    except FileNotFoundError:
        print("Error: ImageMagick 'convert' command not found. Please install ImageMagick.")
        sys.exit(1)
    except Exception as e:
        print(f"Error converting icon: {e}")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_icon.py <input.png> <output.ico>")
        sys.exit(1)
    
    png_path = sys.argv[1]
    ico_path = sys.argv[2]
    
    if not os.path.exists(png_path):
        print(f"Error: Input file {png_path} does not exist")
        sys.exit(1)
    
    convert_png_to_ico(png_path, ico_path) 