#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <unordered_map>

#include "sunset/ecs.h"
#include "sunset/geometry.h"

struct Keyframe {
  float time;
  glm::vec3 translation;
  glm::quat rotation;
  glm::vec3 scale;
};

struct BoneAnimation {
  std::vector<Keyframe> keyframes;
};

struct AnimationClip {
  float duration;
  std::unordered_map<std::string, BoneAnimation> bone_animations;
};

void stepSkeletal(ECS &ecs) {
  ecs.forEach(std::function([&](Entity entity, Transform *transform,
                                SkeletonComponent *skeleton) {
    glm::mat4 model = calculateModelMatrix(ecs, entity);
    size_t count = skeleton->bones.size();
    skeleton->final_transforms.resize(count);

    for (size_t i = 0; i < count; i++) {
      const Bone &bone = skeleton->bones[i];
      glm::mat4 parent_matrix = glm::mat4(1.0f);

      if (bone.parent_index.has_value()) {
        parent_matrix =
            skeleton->final_transforms[bone.parent_index.value()];
      }

      glm::mat4 global_transform = parent_matrix * bone.local_transform;
      skeleton->final_transforms[i] =
          model * global_transform * bone.inverse_bind_matrix;
    }
  }));
}
