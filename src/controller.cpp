#include <glm/glm.hpp>

#include "sunset/ecs.h"
#include "sunset/geometry.h"
#include "sunset/io_provider.h"
#include "sunset/physics.h"
#include "sunset/controller.h"

FreeController::FreeController(ECS &ecs, EventQueue &event_queue) {
  event_queue.subscribe(std::function([&](KeyPressed const &pressed) {
    ecs.forEach(std::function(
        [&](Entity entity, Player *player, Transform *transform) {
          glm::vec3 forward =
              glm::rotate(transform->rotation, glm::vec3(0, 0, -1));
          glm::vec3 right =
              glm::rotate(transform->rotation, glm::vec3(1, 0, 0));

          if (pressed.map.test(static_cast<size_t>(Key::W))) {
            PhysicsSystem::instance().moveObject(
                ecs, entity, player->speed * forward, event_queue);
          }

          if (pressed.map.test(static_cast<size_t>(Key::S))) {
            PhysicsSystem::instance().moveObject(
                ecs, entity, player->speed * -forward, event_queue);
          }

          if (pressed.map.test(static_cast<size_t>(Key::D))) {
            PhysicsSystem::instance().moveObject(
                ecs, entity, player->speed * right, event_queue);
          }

          if (pressed.map.test(static_cast<size_t>(Key::A))) {
            PhysicsSystem::instance().moveObject(
                ecs, entity, player->speed * -right, event_queue);
          }
        }));

    event_queue.subscribe(std::function([&](MouseMoved const &moved) {
      ecs.forEach(std::function(
          [&](Entity entity, Player *player, Transform *transform) {
            float sensitivity = player->sensitivity;

            float yaw = static_cast<float>(moved.dx) * sensitivity;
            float pitch = -static_cast<float>(moved.dy) * sensitivity;

            glm::quat rotation_yaw =
                glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));
            glm::vec3 forward =
                glm::normalize(transform->rotation * glm::vec3(0, 0, -1));
            glm::vec3 right =
                glm::normalize(glm::cross(glm::vec3(0, 1, 0), forward));

            glm::quat rotation_pitch =
                glm::angleAxis(glm::radians(pitch), right);

            transform->rotation =
                rotation_yaw * rotation_pitch * transform->rotation;
            transform->rotation = glm::normalize(transform->rotation);
          }));
    }));
  }));
}

void FreeController::update(ECS &ecs) {}

// BeanController
// PlayerController<BeanController>
// FollowerController<BeanController>
