from dataclasses import dataclass, field
from typing import List, Union
import io
import re
import struct
import zlib
import sys

Property = Union[int, float, str, bytes, List[float], List[int]]

@dataclass
class Node:
    name: str
    props: List[Property] = field(default_factory=list)
    children: List["Node"] = field(default_factory=list)


def parse_properties(properties_str: str) -> List[Property]:
    result = []
    i = 0
    n = len(properties_str)

    while i < n:
        if properties_str[i].isspace():
            i += 1
            continue

        if properties_str[i] == '"':
            # Parse quoted string
            i += 1
            start = i
            while i < n and properties_str[i] != '"':
                i += 1
            result.append(properties_str[start:i])
            i += 1  # skip closing quote

        elif properties_str[i] == '[':
            # Parse array
            i += 1
            start = i
            depth = 1
            while i < n and depth > 0:
                if properties_str[i] == ']':
                    depth -= 1
                    if depth == 0:
                        break
                elif properties_str[i] == '[':
                    depth += 1
                i += 1
            array_str = properties_str[start:i].strip()
            array_items = [x.strip() for x in array_str.split(',') if x.strip()]
            try:
                arr = [int(x) for x in array_items]
                result.append(bytes(arr))  # treat as byte array
            except ValueError:
                result.append([float(x) for x in array_items])  # treat as float list
            i += 1  # skip closing bracket

        else:
            # Parse plain token (number or string identifier)
            start = i
            while i < n and not properties_str[i].isspace():
                i += 1
            token = properties_str[start:i]
            try:
                if '.' in token:
                    result.append(float(token))
                else:
                    result.append(int(token))
            except ValueError:
                result.append(token)

    return result

def extract_named_blocks(s: str) -> List[str]:
    blocks = []
    depth = 0
    start = 0
    i = 0
    while i < len(s):
        if s[i] == '{':
            if depth == 0:
                j = i
                while j > 0 and s[j - 1] not in '\n':
                    j -= 1
                start = j
            depth += 1
        elif s[i] == '}':
            depth -= 1
            if depth == 0:
                blocks.append(s[start:i + 1].strip())
        i += 1
    return blocks

def parse_node(node_str: str) -> Node:
    pattern = r'^([A-Za-z0-9_]+):\s*(.*?)\s*{'
    match = re.match(pattern, node_str.strip(), re.DOTALL)
    if not match:
        raise ValueError(f"Invalid node format: {node_str}")
    
    name = match.group(1)
    props = match.group(2).strip()

    brace_index = node_str.find('{', match.end() - 1)
    if brace_index == -1:
        raise ValueError(f"No child block found for node {name}")

    content = node_str[brace_index + 1 : -1].strip()
    properties = parse_properties(props)

    children = []
    child_blocks = extract_named_blocks(content)
    for block in child_blocks:
        child = parse_node(block)
        children.append(child)
    
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

def main():
    textfile = sys.argv[1]
    binfile = sys.argv[2]
    with open(textfile, "r") as f:
        text = f.read()
    root = parse_node(text)
    print(root)
    stream = io.BytesIO()
    write_node(stream, root)
    with open(binfile, "wb") as f:
        f.write(stream.getvalue())

if __name__ == "__main__":
    main()
