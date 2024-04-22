def fletcher_checksum(data):
    sum1 = 0
    sum2 = 0
    for byte in data:
        sum1 = (sum1 + byte) % 255
        sum2 = (sum2 + sum1) % 255
    return (sum2 << 8) | sum1

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python fletcher_checksum.py <file>")
        sys.exit(1)

    filename = sys.argv[1]
    try:
        with open(filename, "rb") as f:
            data = f.read()
            checksum = fletcher_checksum(data)
            print("Fletcher checksum:", checksum)
    except FileNotFoundError:
        print("File not found:", filename)
        sys.exit(1)
