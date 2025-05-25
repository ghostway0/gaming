from dataclasses import dataclass
from .ptasm import write_node, Node

@dataclass
class Vertex:
    position: tuple[float, float, float]
    normal: tuple[float, float, float]
    uv: tuple[float, float]

def flatten(vertices, indices):
    positions = []
    normals = []
    uvs = []
    flat_indices = []

    for v in vertices:
        positions.extend(v.position)
        normals.extend(v.normal)
        uvs.extend(v.uv)

    for tri in indices:
        tri = list(tri)
        tri[-1] = -tri[-1] - 1
        flat_indices.extend(tri)

    return positions, normals, uvs, flat_indices

@dataclass
class Model:
    vertices: list[Vertex]
    indices: list[tuple]

def generate_model(s, models: list[Model]):
    mm = []
    for i, m in enumerate(models):
        positions, normals, uvs, flat_indices = flatten(m.vertices, m.indices)

        mm.append(Node("Model", props=[f"P{i}"], children=[
            Node("Meshes", children=[
                Node("Mesh", props=[f"P{i}"], children=[
                    Node("Vertices", props=[positions], children=[]),
                    Node("Indices", props=[flat_indices], children=[]),
                    Node("UVs", props=[uvs], children=[]),
                    Node("Normals", props=[normals], children=[]),
                    Node("MaterialId", props=[0], children=[])
                ])
            ])
        ]))

    scene = Node("Scene", children=[
        Node("Textures", children=[
            Node("Texture", props=["groot_diffuse", "groot_diffuse.png"])
        ]),

        Node("Materials", children=[
            Node("Material", props=["GrootMat", 0])
        ]),

        Node("Models", children=mm)
    ])

    write_node(s, scene)
