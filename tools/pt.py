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

@dataclass
class Transform:
    position: tuple[float, float, float]
    rotation: tuple[float, float, float, float]
    scale: float

@dataclass
class Instance:
    transform: Transform
    model_id: int
    physics_type: int = 1

def generate_model(s, models: list[Model], instances: list[Instance]):
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
    
    ii = []
    for i, m in enumerate(instances):
        ii.append(Node("Instance", children=[
            Node("Transform", props=[], children=[
                Node("Position", props=list(m.transform.position)),
                Node("Rotation", props=list(m.transform.rotation)),
                Node("Scale", props=[m.transform.scale]),
            ]),
            Node("ModelId", props=[m.model_id]),
            Node("PhysicsType", props=[m.physics_type]),
        ]))


    scene = Node("Scene", children=[
        Node("Textures", children=[
            Node("Texture", props=["groot_diffuse", "groot_diffuse.png"])
        ]),

        Node("Materials", children=[
            Node("Material", props=["GrootMat", 0])
        ]),

        Node("Models", children=mm),

        Node("Instances", children=ii)
    ])

    write_node(s, scene)
