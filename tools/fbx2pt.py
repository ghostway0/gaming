from dataclasses import dataclass, field
from typing import List, Optional, Union
import io
import struct
import zlib
import sys

Property = Union[int, float, str, bytes, List[float], List[int]]

@dataclass
class Node:
    name: str
    props: List[Property] = field(default_factory=list)
    children: List["Node"] = field(default_factory=list)


def parse_property(properties_str: str) -> List[Property]:
    result = []
    parts = iter(properties_str.split(' '))
    for part in parts:
        if not part: continue

        if part.startswith('[') and part.endswith(']'):
            result.append(bytes(int(i) for i in part[1:-1].split(',')))
        elif part.startswith('"'):
            s = part[1:]
            part = next(parts)
            while not part.endswith('"'):
                s += " " + part
                part = next(parts)
            result.append(s + " " + part[:-1])
        elif '.' in part:
            # Float property
            result.append(float(part))
        else:
            try:
                result.append(int(part))
            except:
                result.append(part)
    return result

def write_value(fmt, value):
    return struct.pack(fmt, value)

def write_string(s):
    return write_value('<I', len(s)) + s.encode('utf-8')

def write_array(data, fmt):
    array_bytes = b''.join(struct.pack(fmt, v) for v in data)
    compressed = zlib.compress(array_bytes)
    return (write_value('<I', len(array_bytes)) +
            write_value('<I', 1) +
            write_value('<I', len(compressed)) +
            compressed)

def write_property(prop):
    if isinstance(prop, bool):  # FBX 'C' = byte bool
        return b'C' + write_value('<B', int(prop))
    elif isinstance(prop, int):
        if -(2**15) <= prop < 2**15:
            return b'Y' + write_value('<h', prop)  # int16
        elif -(2**31) <= prop < 2**31:
            return b'I' + write_value('<i', prop)  # int32
        else:
            return b'L' + write_value('<q', prop)  # int64
    elif isinstance(prop, float):
        return b'D' + write_value('<d', prop)  # double
    elif isinstance(prop, str):
        return b'S' + write_string(prop)
    elif isinstance(prop, list):
        if all(isinstance(x, float) for x in prop):
            return b'f' + write_array(prop, '<f')
        elif all(isinstance(x, int) for x in prop):
            return b'i' + write_array(prop, '<i')
        else:
            raise ValueError("Unsupported list element types in property")
    elif isinstance(prop, bytes) or isinstance(prop, bytearray):
        return b'c' + write_array(list(prop), '<B')
    else:
        raise ValueError(f"Unsupported property type: {type(prop)}")

def write_property_tree(stream, name, properties=None, children=None):
    if properties is None:
        properties = []
    if children is None:
        children = []

    start = stream.tell()

    props_data = b''.join(write_property(p) for p in properties)
    props_len = len(props_data)
                             

    name_bytes = name.encode('utf-8')
    name_len = len(name_bytes)

    end_offset_pos = stream.tell()
    stream.write(b'\x00\x00\x00\x00')  # end_offset placeholder
    stream.write(write_value('<I', len(properties)))  # num_properties
    stream.write(write_value('<I', props_len))  # property_list_len
    stream.write(write_value('<B', name_len))  # name_len
    stream.write(name_bytes)  # name
    stream.write(props_data)

    for child in children:
        write_property_tree(stream, child.name, child.props, child.children)

    end_offset = stream.tell() - start - 13

    stream.seek(end_offset_pos)
    stream.write(write_value('<I', end_offset))

    stream.seek(0, io.SEEK_END)

def write_node(stream, node):
    write_property_tree(stream, node.name, node.props, node.children)

def read_uint8(f): return struct.unpack('<B', f.read(1))[0]
def read_uint32(f): return struct.unpack('<I', f.read(4))[0]
def read_uint64(f): return struct.unpack('<Q', f.read(8))[0]
def read_int64(f): return struct.unpack('<q', f.read(8))[0]
def read_double(f): return struct.unpack('<d', f.read(8))[0]

def read_string(f, length): return f.read(length).decode()
def read_array_property(f, element_size, fmt_char):
    array_length = read_uint32(f)
    encoding = read_uint32(f)
    compressed_length = read_uint32(f)

    data = f.read(compressed_length)
    if encoding == 1:
        data = zlib.decompress(data)

    count = len(data) // element_size
    assert count == array_length
    return list(struct.unpack('<' + fmt_char * count, data))

def read_property(f, type_code):
    if type_code == b'S':  # string
        length = read_uint32(f)
        return read_string(f, length)
    elif type_code == b'I':  # int32
        return struct.unpack('<i', f.read(4))[0]
    elif type_code == b'L':  # int64
        return read_int64(f)
    elif type_code == b'd':  # double array
        return read_array_property(f, 8, 'd')
    elif type_code == b'f':  # float array
        return read_array_property(f, 4, 'f')
    elif type_code == b'i':  # int32 array
        return read_array_property(f, 4, 'i')
    elif type_code == b'l':  # int64 array
        return read_array_property(f, 8, 'q')
    elif type_code == b'b':  # bool array
        return read_array_property(f, 1, '?')
    elif type_code == b'F':  # float scalar
        return struct.unpack('<f', f.read(4))[0]
    elif type_code == b'D':  # double scalar
        return read_double(f)
    elif type_code == b'Y':  # int16 scalar
        return struct.unpack('<h', f.read(2))[0]
    elif type_code == b'C':  # bool scalar
        return bool(struct.unpack('<B', f.read(1))[0])
    elif type_code == b'R':  # raw binary data (length-prefixed)
        length = read_uint32(f)
        return f.read(length)
    else:
        raise ValueError(f"Unhandled property type: {type_code}")

def read_node(f, depth=0):
    start = f.tell()

    end_offset = read_uint32(f)
    if end_offset == 0:
        f.seek(start)
        return None

    num_props = read_uint32(f)
    props_len = read_uint32(f)
    name_len = read_uint8(f)
    name = read_string(f, name_len)

    props = []
    for _ in range(num_props):
        type_code = f.read(1)
        prop = read_property(f, type_code)
        props.append(prop)

    node = Node(name=name, props=props)

    while f.tell() < end_offset:
        try:
            if child := read_node(f, depth+1):
                if depth < 256:
                    node.children.append(child)
            else:
                break
        except Exception:
            break

    return node

def peek(f, n):
    pos = f.tell()
    b = f.read(n)
    f.seek(pos)
    return b

def parse_fbx(s) -> Node:
    f.seek(0, 2)
    size = f.tell()
    f.seek(0)

    header = s.read(23)
    if not header.startswith(b'Kaydara FBX Binary'):
        raise ValueError("Not a binary FBX file")

    version = read_uint32(s)
    print(f"FBX file version: {version}")

    result = Node("RootNode")

    while True:
        if f.tell() >= size:
            break

        if peek(s, 13)[:13] == b'\x00' * 13:
            s.read(13)

        try:
            if node := read_node(s):
                result.children.append(node)
                # print(node)
            else:
                continue
        except:
            pass

    return result

def locate(node: Node, target_name: str, path='') -> List[str] | None:
    full_path = f"{path}.{node.name}" if path else node.name
    s = []

    if target_name in node.name:
        s.append(full_path)

    for child in node.children:
        result = locate(child, target_name, full_path)
        if result:
            s += result
    return s or None

def find_property(node: Node, path: str):
    curr = node
    for p in path.split('.'):
        for c in curr.children:
            if p in c.name:
                curr = c
                break
        else:
            raise ValueError
    return curr

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python fbx2pt.py <file.fbx> <file.bin>")
    else:
        with open(sys.argv[1], 'rb') as f:
            node = parse_fbx(f)

        v = find_property(node, "Vertices").props[0]
        i = find_property(node, "PolygonVertexIndex").props[0]
        uv = find_property(node, "LayerElementUV.UV").props[0]
        uvi = find_property(node, "LayerElementUV.UVIndex").props[0]
        stream = io.BytesIO()
        write_node(stream, node)
        with open(sys.argv[2], "wb") as f:
            f.write(stream.getvalue())
