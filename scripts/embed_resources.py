#!/usr/bin/env python3
"""
Embed binary resources (like PNG files) into C code.
Usage: embed_resources.py <input_files...> <output_dir>
"""
import sys
import os

def generate_resources(input_files, output_dir):
    """Generate C header and source files with embedded binary data."""
    h_file = os.path.join(output_dir, 'embedded_resources.h')
    c_file = os.path.join(output_dir, 'embedded_resources.c')
    
    with open(h_file, 'w') as h, open(c_file, 'w') as c:
        # Write header file
        h.write('// Auto-generated embedded resources\n')
        h.write('#pragma once\n')
        h.write('#ifdef __cplusplus\n')
        h.write('extern "C" {\n')
        h.write('#endif\n\n')
        h.write('#include <stddef.h>\n\n')
        
        # Write source file
        c.write('// Auto-generated embedded resources\n')
        c.write('#include "embedded_resources.h"\n\n')
        
        for input_file in input_files:
            # Create valid C identifier from filename
            basename = os.path.basename(input_file)
            var_name = 'embedded_' + basename.replace('.', '_').replace('-', '_')
            
            print(f"Embedding: {basename} as {var_name}")
            
            with open(input_file, 'rb') as f:
                data = f.read()
            
            is_text = input_file.endswith(('.toml', '.txt', '.json'))

            if is_text:
                # Embed as null-terminated C string
                c.write(f'const char {var_name}[] = {{\n')
                for i in range(0, len(data), 12):
                    chunk = data[i:i+12]
                    c.write('  ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',\n')
                c.write('  0x00\n};\n\n')

                h.write(f'// {basename}\n')
                h.write(f'extern const char {var_name}[];\n\n')
            else:
                # Write C array in source file
                c.write(f'const unsigned char {var_name}[] = {{\n')
                for i in range(0, len(data), 12):
                    chunk = data[i:i+12]
                    c.write('  ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',\n')
                c.write('};\n\n')
                c.write(f'const size_t {var_name}_len = {len(data)};\n\n')

                # Write header declarations
                h.write(f'// {basename}\n')
                h.write(f'extern const unsigned char {var_name}[];\n')
                h.write(f'extern const size_t {var_name}_len;\n\n')
        
        h.write('#ifdef __cplusplus\n')
        h.write('}\n')
        h.write('#endif\n')

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: embed_resources.py <input_files...> <output_dir>")
        sys.exit(1)
    
    input_files = sys.argv[1:-1]
    output_dir = sys.argv[-1]
    
    # Validate inputs
    for input_file in input_files:
        if not os.path.exists(input_file):
            print(f"Error: Input file not found: {input_file}")
            sys.exit(1)
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    generate_resources(input_files, output_dir)
    print(f"Generated embedded_resources.h and embedded_resources.c in {output_dir}")

