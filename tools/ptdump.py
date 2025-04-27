import struct
import zlib
import sys

def read_value(fmt, f):
    return struct.unpack(fmt, f.read(struct.calcsize(fmt)))[0]

def read_string(f):
    length = read_value('<I', f)
    return f.read(length).decode('utf-8')

def read_array(f):
    raw_len = read_value('<I', f)
    encoding = read_value('<I', f)
    comp_len = read_value('<I', f)
    compressed = f.read(comp_len)
    decompressed = zlib.decompress(compressed)
    arr = []
    for i in range(0, len(decompressed), 4):
        arr.append(struct.unpack('<f', decompressed[i:i+4])[0])
    return arr

def read_property(f):
    type_byte = f.read(1)
    if type_byte == b'L':
        return read_value('<q', f)
    elif type_byte == b'D':
        return read_value('<d', f)
    elif type_byte == b'S':
        return read_string(f)
    elif type_byte == b'f':
        return read_array(f)
    else:
        raise ValueError(f"Unknown property type: {type_byte}")

def format_property(p):
    if isinstance(p, int):
        return str(p)
    elif isinstance(p, float):
        return f"{p:.2f}"
    elif isinstance(p, str):
        return f"\"{p}\""
    elif isinstance(p, list):
        return ' '.join(f"{v:.2f}" for v in p)
    else:
        raise ValueError(f"Unknown property type: {type(p)}")

def read_node(f, depth=0):
    start = f.tell()
    end_offset = read_value('<I', f)
    num_properties = read_value('<I', f)
    property_list_len = read_value('<I', f)
    name_len = read_value('<B', f)
    name = f.read(name_len).decode('utf-8')

    props = []
    for _ in range(num_properties):
        props.append(read_property(f))

    indent = '  ' * depth
    props_str = ' '.join(format_property(p) for p in props)
    print(f"{indent}{name}: {props_str} {{")

    node_end = start + end_offset + 13
    while f.tell() < node_end:
        read_node(f, depth + 1)

    print(f"{indent}}}")

def main():
    filename = sys.argv[1]
    with open(filename, "rb") as f:
        read_node(f)

if __name__ == "__main__":
    main()
