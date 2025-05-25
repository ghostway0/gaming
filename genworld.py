import sys
import math
import io
from tools.pt import Instance, Transform, generate_model, Vertex, Model

def generate_room(
    width=10.0, depth=10.0, height=3.0, wall_thickness=0.2
):
    models = []
    instances = []

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

    wall_model = make_box(-width/2, 0.0, 0.0,
                            width/2, height, wall_thickness)

    models.append(floor_model)
    models.append(wall_model)

    instances.append(Instance(
        transform=Transform(position=(0.0, 0.0, 0.0), rotation=(0.0, 0.0, 0.0, 1.0), scale=1.0),
        model_id=0,
    ))

    half_w, half_d = width / 2, depth / 2

    def quat_yaw(degrees):
        radians = math.radians(degrees)
        return (0.0, math.sin(radians / 2), 0.0, math.cos(radians / 2))

    # Front (+Z)
    instances.append(Instance(
        transform=Transform(position=(0.0, 0.0, half_d), rotation=quat_yaw(0), scale=1.0),
        model_id=1,
    ))
    # Back (-Z)
    instances.append(Instance(
        transform=Transform(position=(0.0, 0.0, -half_d), rotation=quat_yaw(0), scale=1.0),
        model_id=1,
    ))
    # Left (-X)
    instances.append(Instance(
        transform=Transform(position=(-half_w, 0.0, 0.0), rotation=quat_yaw(90), scale=1.0),
        model_id=1,
    ))
    # Right (+X)
    instances.append(Instance(
        transform=Transform(position=(half_w, 0.0, 0.0), rotation=quat_yaw(-90), scale=1.0),
        model_id=1,
    ))

    return models, instances


models, instances = generate_room()
stream = io.BytesIO()
generate_model(stream, models, instances)
with open(sys.argv[1], "wb") as f:
    f.write(stream.getvalue())
