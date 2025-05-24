#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

struct Vertices {
  std::vector<float> vertices;
};

struct Indices {
  std::vector<int32_t> indices;
};

struct UVs {
  std::vector<float> uvs;
};

struct Normals {
  std::vector<float> normals;
};

struct Mesh {
  int32_t id;
  std::string name;
  Vertices vertices;
  Indices indices;
  UVs uvs;
  int32_t material_id;
};

struct Texture {
  std::string src;
};

struct Material {
  int32_t id;
  std::string name;
  int32_t texture_id;
};

struct Model {
  int32_t id;
  std::string name;
  std::vector<Mesh> meshes;
};

struct Scene {
  std::vector<Model> models;
  std::vector<Material> materials;
  std::vector<Texture> textures;
};
