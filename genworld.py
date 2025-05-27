import sys
import math
import io
from tools.pt import *

def quat_yaw(degrees):
    radians = math.radians(degrees)
    return (0.0, math.sin(radians / 2), 0.0, math.cos(radians / 2))

def generate_basketball_rim() -> Model:
    vertices = []
    indices = []
    
    rim_radius = 0.225  # 18 inches / 2 = 9 inches = 0.225 meters
    rim_height = 3.048  # 10 feet = 3.048 meters
    rim_thickness = 0.01  # 1cm thick rim tube
    
    rim_resolution = 32  # Number of segments around the rim circle
    tube_resolution = 12  # Number of segments around the tube cross-section
    
    for i in range(rim_resolution):
        theta = (i * 2 * math.pi) / rim_resolution
        cos_theta = math.cos(theta)
        sin_theta = math.sin(theta)

        major_x = rim_radius * cos_theta
        major_z = rim_radius * sin_theta

        for j in range(tube_resolution):
            phi = (j * 2 * math.pi) / tube_resolution
            cos_phi = math.cos(phi)
            sin_phi = math.sin(phi)

            x = major_x + rim_thickness * cos_phi * cos_theta
            y = rim_height + rim_thickness * sin_phi
            z = major_z + rim_thickness * cos_phi * sin_theta

            normal_x = cos_phi * cos_theta
            normal_y = sin_phi
            normal_z = cos_phi * sin_theta

            u = i / rim_resolution
            v = j / tube_resolution

            vertex = Vertex(
                position=(x, y, z),
                normal=(normal_x, normal_y, normal_z),
                uv=(u, v)
            )
            vertices.append(vertex)

    for i in range(rim_resolution):
        for j in range(tube_resolution):
            # Current vertex indices
            curr = i * tube_resolution + j
            next_i = ((i + 1) % rim_resolution) * tube_resolution + j
            next_j = i * tube_resolution + ((j + 1) % tube_resolution)
            next_both = ((i + 1) % rim_resolution) * tube_resolution + ((j + 1) % tube_resolution)
            
            indices.append((curr, next_j, next_i))
            indices.append((next_i, next_j, next_both))

    return Model(vertices=vertices, indices=indices)

def generate_room(
    width=10.0, depth=10.0, height=3.0, wall_thickness=0.2
):
    models = []

    def make_box(x0, y0, z0, x1, y1, z1):
        vertices = []
        indices = []
        index_offset = 0

        def face(verts, normal):
            nonlocal index_offset
            uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
            for pos, uv in zip(verts, uvs):
                x, y, z = pos
                nx, ny, nz = normal
                vertices.append(Vertex(position=(x, y, z), normal=(nx, ny, nz), uv=uv))
            indices.append((index_offset + 2, index_offset + 1, index_offset + 0))
            indices.append((index_offset + 3, index_offset + 2, index_offset + 0))
            index_offset += 4

        # Define six faces (top, bottom, front, back, left, right)
        face([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0.0, 1.0, 0.0))   # Top
        face([(x0, y0, z1), (x1, y0, z1), (x1, y0, z0), (x0, y0, z0)], (0.0, -1.0, 0.0))  # Bottom
        face([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0.0, 0.0, 1.0))   # Front
        face([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0.0, 0.0, -1.0))  # Back
        face([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (1.0, 0.0, 0.0))   # Left
        face([(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (-1.0, 0.0, 0.0))  # Right

        return Model(vertices, indices)

    floor_model = make_box(-width/2, -wall_thickness, -depth/2,
                             width/2, 0.0, depth/2)

    wall_model = make_box(
        -width/2 + wall_thickness / 2, 0.0, 0.0,
         width/2 - wall_thickness / 2, height, wall_thickness
    )

    models.append(floor_model)
    models.append(wall_model)

    return models


def generate_uv_sphere(radius=0.01, stacks=16, sectors=32):
    vertices = []
    indices = []

    for i in range(stacks + 1):
        stack_angle = math.pi / 2 - i * math.pi / stacks  # from pi/2 to -pi/2
        xy = radius * math.cos(stack_angle)
        z = radius * math.sin(stack_angle)

        for j in range(sectors + 1):
            sector_angle = j * 2 * math.pi / sectors  # from 0 to 2pi

            x = xy * math.cos(sector_angle)
            y = xy * math.sin(sector_angle)

            nx, ny, nz = x / radius, y / radius, z / radius
            u = j / sectors
            v = i / stacks

            vertex = Vertex(position=(x, y, z), normal=(nx, ny, nz), uv=(u, v))
            vertices.append(vertex)

    for i in range(stacks):
        for j in range(sectors):
            first = i * (sectors + 1) + j
            second = first + sectors + 1

            indices.append((second, second, first))
            indices.append((second, second + 1, first))

    return Model(vertices, indices)

floor, wall = generate_room()
sphere = generate_uv_sphere()
basket = generate_basketball_rim()

rsrc = [
    floor,
    wall,
    Texture("groot_diffuse.png"),
    sphere,
    Texture("basketball.png"),
    basket,
]

instances = []
half_w, half_d = 10.0 / 2, 10.0 / 2
wall_thickness = 0.2

instances.append(Instance(
    comps=[
        Component("Transform", Transform(
            position=(0.0, 0.0, 0.0),
            rotation=(0.0, 0.0, 0.0, 1.0),
            scale=1.0
        ).to_property_tree()),
        Component("PhysicsComponent", Physics(get_aabb(rsrc[0].vertices), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), 1, restitution=0.9).to_property_tree()),
        Component("MeshRef", RRef("Global", 0).to_property_tree("MeshRef")),
        Component("TextureRef", RRef("Global", 2).to_property_tree("TextureRef"))
    ],
))

instances.append(Instance(
    comps=[
        Component("Transform", Transform(
            position=(0.0, 0.0, half_d - wall_thickness / 2),
            rotation=quat_yaw(0),
            scale=1.0
        ).to_property_tree()),
        Component("PhysicsComponent", Physics(
            translate_aabb(rotate_aabb(get_aabb(rsrc[1].vertices), quat_yaw(0)), (0.0, 0.0, half_d - wall_thickness / 2)),
            (0.0, 0.0, 0.0),
            (0.0, 0.0, 0.0),
            1,
        ).to_property_tree()),
        Component("MeshRef", RRef("Global", 1).to_property_tree("MeshRef")),
        Component("TextureRef", RRef("Global", 2).to_property_tree("TextureRef"))
    ],
))

instances.append(Instance(
    comps=[
        Component("Transform", Transform(
            position=(0.0, 0.0, -half_d + wall_thickness / 2),
            rotation=quat_yaw(0),
            scale=1.0
        ).to_property_tree()),
        Component("PhysicsComponent", Physics(
            translate_aabb(rotate_aabb(get_aabb(rsrc[1].vertices), quat_yaw(0)), (0.0, 0.0, -half_d + wall_thickness / 2)),
            (0.0, 0.0, 0.0),
            (0.0, 0.0, 0.0), 1
        ).to_property_tree()),
        Component("MeshRef", RRef("Global", 1).to_property_tree("MeshRef")),
        Component("TextureRef", RRef("Global", 2).to_property_tree("TextureRef"))
    ],
))

instances.append(Instance(
    comps=[
        Component("Transform", Transform(
            position=(-half_w + wall_thickness / 2, 0.0, 0.0),
            rotation=quat_yaw(90),
            scale=1.0
        ).to_property_tree()),
        Component("PhysicsComponent", Physics(
            translate_aabb(rotate_aabb(get_aabb(rsrc[1].vertices), quat_yaw(90)), (-half_w + wall_thickness / 2, 0.0, 0.0)),
            (0.0, 0.0, 0.0),
            (0.0, 0.0, 0.0), 1
        ).to_property_tree()),
        Component("MeshRef", RRef("Global", 1).to_property_tree("MeshRef")),
        Component("TextureRef", RRef("Global", 2).to_property_tree("TextureRef"))
    ],
))

instances.append(Instance(
    comps=[
        Component("Transform", Transform(
            position=(half_w - wall_thickness / 2, 0.0, 0.0),
            rotation=quat_yaw(-90),
            scale=1.0
        ).to_property_tree()),
        Component("PhysicsComponent", Physics(
            translate_aabb(rotate_aabb(get_aabb(rsrc[1].vertices), quat_yaw(-90)), (half_w - wall_thickness / 2, 0.0, 0.0)),
            (0.0, 0.0, 0.0),
            (0.0, 0.0, 0.0), 1
        ).to_property_tree()),
        Component("MeshRef", RRef("Global", 1).to_property_tree("MeshRef")),
        Component("TextureRef", RRef("Global", 2).to_property_tree("TextureRef"))
    ],
))

instances.append(Instance(
    comps=[
        Component("Transform", Transform(
            position=(1.0, 1.5, 0.0),
            rotation=quat_yaw(0),
            scale=5.0
        ).to_property_tree()),
        Component("PhysicsComponent", Physics(
            translate_aabb(scale_aabb(get_aabb(rsrc[3].vertices), 5.0), (1.0, 1.5, 0.0)),
            (0.0, 0.0, 0.0),
            (0.0, 0.0, 0.0), 2,
            ty=2,
            mass=0.1,
            restitution=0.9
        ).to_property_tree()),
        Component("MeshRef", RRef("Global", 4).to_property_tree("MeshRef")),
        Component("TextureRef", RRef("Global", 3).to_property_tree("TextureRef"))
    ],
))

stream = io.BytesIO()
generate_model(stream, instances, [r.to_property_tree() for r in rsrc])
with open(sys.argv[1], "wb") as f:
    f.write(stream.getvalue())
