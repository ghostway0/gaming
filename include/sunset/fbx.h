#include <string>
#include <vector>
#include <cstdint>

struct Vec3 {
  float x, y, z;
};

struct Keyframe {
  int64_t time;
  float value;
};

struct AnimationCurve {
  std::string name;
  std::vector<Keyframe> keyframes;
};

struct Model {
  uint64_t id;
  std::string name;
  std::string type; // e.g., "Mesh", "Null", "LimbNode"
  Vec3 translation;
  // Optionally could add: rotation, scaling, etc.
};

struct Deformer {
  uint64_t id;
  std::string name;
  std::string type; // e.g., "Skin" or "Cluster" (bone)
};

struct AnimationLayer {
  uint64_t id;
  std::string name;
  std::vector<uint64_t> curve_ids; // Link to AnimationCurves
};

struct AnimationStack {
  uint64_t id;
  std::string name;
  std::vector<uint64_t> layer_ids; // Link to AnimationLayers
};

enum class ConnectionType {
  ObjectToObject,  // "OO"
  ObjectToProperty // "OP"
};

struct Connection {
  ConnectionType type;
  uint64_t src;
  uint64_t dst;
  std::string property; // Only used for OP (Object to Property)
};

struct FBXScene {
  std::vector<Model> models;
  std::vector<Deformer> deformers;
  std::vector<AnimationStack> anim_stacks;
  std::vector<AnimationLayer> anim_layers;
  std::vector<AnimationCurve> anim_curves;
  std::vector<Connection> connections;
};
