import sys
import io
from tools.pt import generate_model, Vertex, Model


def generate_room(
    width=10.0, depth=10.0, height=3.0, wall_thickness=0.2
) -> list[Model]:
    models = []

    def make_surface(verts, normal, uvs):
        vertices = []
        indices = []

        for pos, uv in zip(verts, uvs):
            x, y, z = pos
            nx, ny, nz = normal
            vertices.append(Vertex(position=(x, y, z), normal=(nx, ny, nz), uv=uv))

        indices.append((2, 1, 0))
        indices.append((3, 2, 0))

        return Model(vertices, indices)

    def make_box(x0, y0, z0, x1, y1, z1):
        """Create a box from (x0, y0, z0) to (x1, y1, z1)."""
        box = []

        def face(verts, normal):
            uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
            box.append(make_surface(verts, normal, uvs))

        # Six faces (top, bottom, front, back, left, right)
        face(
            [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0.0, 1.0, 0.0)
        )  # Top
        face(
            [(x0, y0, z1), (x1, y0, z1), (x1, y0, z0), (x0, y0, z0)], (0.0, -1.0, 0.0)
        )  # Bottom
        face(
            [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0.0, 0.0, 1.0)
        )  # Front
        face(
            [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0.0, 0.0, -1.0)
        )  # Back
        face(
            [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (1.0, 0.0, 0.0)
        )  # Left
        face(
            [(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (-1.0, 0.0, 0.0)
        )  # Right

        return box

    half_w = width / 2
    half_d = depth / 2

    # Floor (extruded downward)
    models += make_box(-half_w, -wall_thickness, -half_d, half_w, 0.0, half_d)

    # Front wall (z = +depth/2)
    models += make_box(-half_w, 0.0, half_d, half_w, height, half_d + wall_thickness)

    # Back wall (z = -depth/2)
    models += make_box(-half_w, 0.0, -half_d - wall_thickness, half_w, height, -half_d)

    # Left wall (x = -width/2)
    models += make_box(-half_w - wall_thickness, 0.0, -half_d, -half_w, height, half_d)

    # Right wall (x = +width/2)
    models += make_box(half_w, 0.0, -half_d, half_w + wall_thickness, height, half_d)

    return models


models = generate_room()
stream = io.BytesIO()
generate_model(stream, models)
with open(sys.argv[1], "wb") as f:
    f.write(stream.getvalue())
