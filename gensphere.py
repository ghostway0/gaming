import math
import sys
import io
from tools.pt import Model, generate_model, Vertex


def generate_uv_sphere(radius=1.0, stacks=16, sectors=32):
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

    return vertices, indices


vertices, indices = generate_uv_sphere()
stream = io.BytesIO()
generate_model(stream, [Model(vertices, indices)])
with open(sys.argv[1], "wb") as f:
    f.write(stream.getvalue())
