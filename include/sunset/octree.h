#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <optional>

#include <glm/glm.hpp>

#include "sunset/geometry.h"

// TODO:
// octree that all it does is have an AABB and Entity and partitions soly based on them.

template <typename T>
class OcTree;

template <typename T>
struct OcTreeNode {
  size_t depth;
  OcTreeNode *parent;
  std::optional<T> data;
  AABB bounds;
  bool dirty{false};
  std::array<std::unique_ptr<OcTreeNode<T>>, 8> children{};

  OcTreeNode(size_t depth, std::optional<T> data, OcTreeNode *parent,
             const AABB &bounds)
      : depth(depth),
        parent(parent),
        data(std::move(data)),
        bounds(bounds) {}
};

template <typename T>
class OcTree {
 public:
  explicit OcTree(size_t max_depth, T root_data, const AABB &root_bounds);

  T *getMutable(const glm::vec3 &position) {
    OcTreeNode<T> *node = findNode(position, true);
    return node && node->data ? &*node->data : nullptr;
  }

  const T *query(const glm::vec3 &position) const {
    const OcTreeNode<T> *node = findNode(position, false);
    return node && node->data ? &*node->data : nullptr;
  }

 private:
  size_t max_depth_;
  std::unique_ptr<OcTreeNode<T>> root_;

  bool maybeSplitNode(OcTreeNode<T> &node) {
    if (!node.dirty || !node.data) {
      return false;
    }

    if (node.depth >= max_depth_ || !node.data->shouldSplit(*this, node)) {
      return false;
    }

    for (size_t i = 0; i < 8; ++i) {
      AABB child_bounds = aabb_subdivide_i(node.bounds, i, 8);
      T child_data = node.data->split(*this, child_bounds);
      node.children[i] = std::make_unique<OcTreeNode<T>>(
          node.depth + 1, std::optional<T>{std::move(child_data)}, &node,
          child_bounds);

      if (node.children[i]->data->shouldSplit(*this, *node.children[i])) {
        maybeSplitNode(*node.children[i]);
      }
    }

    node.data = std::nullopt;
    return true;
  }

  OcTreeNode<T> *findNode(const glm::vec3 &position,
                          bool mutable_access) const {
    OcTreeNode<T> *current = root_.get();
    while (current) {
      if (current->data) {
        if (mutable_access) {
          current->dirty = maybeSplitNode(*current) ? false : true;
        }
        return current;
      }

      OcTreeNode<T> *next = nullptr;
      for (const auto &child : current->children) {
        if (child && child->bounds.contains(position)) {
          next = child.get();
          break;
        }
      }
      if (!next) {
        return nullptr;
      }
      current = next;
    }
    return nullptr;
  }
};
