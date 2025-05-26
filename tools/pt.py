from dataclasses import dataclass
import numpy as np
from .ptasm import write_node, Node

vec3 = tuple[float, float, float]
vec4 = tuple[float, float, float, float]

@dataclass
class Vertex:
    position: vec3
    normal: vec3
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
class Texture:
    src: str

    def to_property_tree(self):
        return Node("Texture", children=[Node("Src", props=[self.src])])

@dataclass
class Model:
    vertices: list[Vertex]
    indices: list[tuple]

    def to_property_tree(self):
        positions, normals, uvs, flat_indices = flatten(self.vertices, self.indices)

        return Node("Mesh", props=[], children=[
            Node("Vertices", props=[positions], children=[]),
            Node("Indices", props=[flat_indices], children=[]),
            Node("UVs", props=[uvs], children=[]),
            Node("Normals", props=[normals], children=[]),
            Node("MaterialId", props=[0], children=[])
        ])

def scale_aabb(aabb: tuple[vec3, vec3], scale) -> tuple[vec3, vec3]:
    min_corner, max_corner = aabb

    new_min = (
        min_corner[0] * scale,
        min_corner[1] * scale,
        min_corner[2] * scale
    )
    new_max = (
        max_corner[0] * scale,
        max_corner[1] * scale,
        max_corner[2] * scale
    )
    return new_min, new_max

def get_aabb(vertices: list[Vertex]):
    if not vertices:
        raise ValueError("Vertex list is empty")

    min_x, min_y, min_z = vertices[0].position
    max_x, max_y, max_z = vertices[0].position

    for v in vertices[1:]:
        x, y, z = v.position
        min_x = min(min_x, x)
        min_y = min(min_y, y)
        min_z = min(min_z, z)
        max_x = max(max_x, x)
        max_y = max(max_y, y)
        max_z = max(max_z, z)

    return (min_x, min_y, min_z), (max_x, max_y, max_z)

@dataclass
class RRef:
    scope: str
    id: int

    def to_property_tree(self, ref_name):
        return Node(ref_name, children=[
            Node("Scope", props=[self.scope]),
            Node("ResourceId", props=[self.id]),
        ])

def translate_aabb(aabb: tuple[vec3, vec3], dir: vec3) -> tuple[vec3, vec3]:
    min_corner, max_corner = aabb
    dx, dy, dz = dir

    new_min = (
        min_corner[0] + dx,
        min_corner[1] + dy,
        min_corner[2] + dz
    )
    new_max = (
        max_corner[0] + dx,
        max_corner[1] + dy,
        max_corner[2] + dz
    )

    return new_min, new_max

def rotate_aabb(aabb: tuple[vec3, vec3], rot: vec4) -> tuple[vec3, vec3]:
    min_corner, max_corner = np.array(aabb[0]), np.array(aabb[1])
    x, y, z, w = rot

    q = np.array([x, y, z, w])
    q /= np.linalg.norm(q)

    xx, yy, zz = q[0] * q[0], q[1] * q[1], q[2] * q[2]
    xy, xz, yz = q[0] * q[1], q[0] * q[2], q[1] * q[2]
    wx, wy, wz = q[3] * q[0], q[3] * q[1], q[3] * q[2]

    rot_matrix = np.array([
        [1 - 2 * (yy + zz),     2 * (xy - wz),       2 * (xz + wy)],
        [    2 * (xy + wz), 1 - 2 * (xx + zz),       2 * (yz - wx)],
        [    2 * (xz - wy),     2 * (yz + wx),   1 - 2 * (xx + yy)]
    ])

    corners = np.array([
        [min_corner[0], min_corner[1], min_corner[2]],
        [min_corner[0], min_corner[1], max_corner[2]],
        [min_corner[0], max_corner[1], min_corner[2]],
        [min_corner[0], max_corner[1], max_corner[2]],
        [max_corner[0], min_corner[1], min_corner[2]],
        [max_corner[0], min_corner[1], max_corner[2]],
        [max_corner[0], max_corner[1], min_corner[2]],
        [max_corner[0], max_corner[1], max_corner[2]],
    ])

    rotated_corners = corners @ rot_matrix.T

    new_min = tuple(float(i) for i in np.min(rotated_corners, axis=0))
    new_max = tuple(float(i) for i in np.max(rotated_corners, axis=0))

    return new_min, new_max

@dataclass
class Physics:
    collider: tuple[vec3, vec3]
    velocity: vec3
    accl: vec3
    collision_source: int
    mass: float = 1.0
    ty: int = 1
    friction: float = 0.5
    restitution: float = 0.5

    def to_property_tree(self):
        return Node("PhysicsComponent", children=[
            Node("Velocity", props=list(self.velocity)),
            Node("Acceleration", props=list(self.accl)),
            Node("Mass", props=[self.mass]),
            Node("Type", props=[self.ty]),
            Node("Material", children=[Node("Friction", props=[self.friction]), Node("Restitution", props=[self.restitution])]),
            Node("Collider", children=[
                Node("Min", props=list(self.collider[0])),
                Node("Max", props=list(self.collider[1])),
            ]),
            Node("CollisionSource", props=[self.collision_source])
        ])

@dataclass
class Transform:
    position: vec3
    rotation: vec4
    scale: float

    def to_property_tree(self):
        return Node("Transform", children=[
            Node("Position", props=list(self.position)),
            Node("Rotation", props=list(self.rotation)),
            Node("Scale", props=[self.scale])
        ])


@dataclass
class Component:
    id: str
    data: Node

    def to_property_tree(self):
        self.data.name = self.id
        return self.data

@dataclass
class Instance:
    comps: list[Component]

    def to_property_tree(self):
        return Node("Instance", children=[Node("Components", children=[c.to_property_tree() for c in self.comps])])

@dataclass
class Scene:
    resources: list[Node]
    instances: list[Instance]

def generate_model(s, instances: list[Instance], rsrc: list[Node]):
    ii = [i.to_property_tree() for i in instances]

    scene = Node("Scene", children=[
        Node("Scope", props=["Global"]),
        Node("Instances", children=ii),
        Node("Resources", children=rsrc),
    ])

    write_node(s, scene)
