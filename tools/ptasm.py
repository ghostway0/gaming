from dataclasses import dataclass, field
from typing import List, Union
import io
import re
import struct
import zlib
import sys

Property = Union[int, float, str, bytes]

@dataclass
class Node:
    name: str
    props: List[Property] = field(default_factory=list)
    children: List["Node"] = field(default_factory=list)


def parse_property(properties_str: str) -> List[Property]:
    result = []
    parts = properties_str.split(' ')
    for part in parts:
        if part.startswith('"') and part.endswith('"'):
            # String property
            result.append(part[1:-1])
        elif '.' in part:
            # Float property
            result.append(float(part))
        else:
            try:
                result.append(int(part))
            except:
                result.append(part)
    return result

def parse_node(node_str: str) -> Node:
    pattern = r'([A-Za-z0-9_]+):\s*(.*?)\s*{(.*)}'
    match = re.match(pattern, node_str.strip(), re.DOTALL)
    if not match:
        raise ValueError(f"Invalid node format: {node_str}")
    
    name = match.group(1)
    properties_str = match.group(2).strip()
    children_str = match.group(3).strip()

    properties = parse_property(properties_str)
    
    children = []
    if children_str:
        child_pattern = r'([A-Za-z0-9_]+:\s*.*?{.*?})'
        child_matches = re.findall(child_pattern, children_str, re.DOTALL)
        for child in child_matches:
            children.append(parse_node(child.strip()))

    return Node(name, properties, children)

def parse_ptasm(ptasm_str: str) -> Node:
    root_pattern = r'([A-Za-z0-9_]+):\s*(.*?)\s*{(.*)}'
    match = re.match(root_pattern, ptasm_str.strip(), re.DOTALL)
    if not match:
        raise ValueError(f"Invalid root node format: {ptasm_str}")
    
    root_name = match.group(1)
    root_properties_str = match.group(2).strip()
    root_children_str = match.group(3).strip()

    root_properties = parse_property(root_properties_str)
    
    root_children = []
    if root_children_str:
        child_pattern = r'([A-Za-z0-9_]+:\s*.*?{.*?})'
        child_matches = re.findall(child_pattern, root_children_str, re.DOTALL)
        for child in child_matches:
            root_children.append(parse_node(child.strip()))
    
    return Node(root_name, root_properties, root_children)

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
    if isinstance(prop, int):
        return b'L' + write_value('<q', prop)
    elif isinstance(prop, float):
        return b'D' + write_value('<d', prop)
    elif isinstance(prop, str):
        return b'S' + write_string(prop)
    elif isinstance(prop, list):
        return b'f' + write_array(prop, '<f')
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

def main():
    textfile = sys.argv[1]
    binfile = sys.argv[2]
    with open(textfile, "r") as f:
        text = f.read()
    root = parse_node(text)
    stream = io.BytesIO()
    write_node(stream, root)
    with open(binfile, "wb") as f:
        f.write(stream.getvalue())

if __name__ == "__main__":
    main()
