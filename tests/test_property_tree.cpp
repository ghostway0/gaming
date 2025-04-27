#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

#include "sunset/property_tree.h"

struct Vector3 {
  double x, y, z;
};

template <>
struct TypeDeserializer<Vector3> {
  static std::vector<FieldDescriptor<Vector3>> getFields() {
    return {
        makeSetter("x", &Vector3::x),
        makeSetter("y", &Vector3::y),
        makeSetter("z", &Vector3::z),
    };
  }
};

TEST(TestPropertyTree, SimpleDeserialize) {
  // Vector3: 1.0 2.0 3.0 {}
  constexpr std::array<uint8_t, 47> data = {
      0x22, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x1b, 0x00,
      0x00, 0x00, 0x07, 0x56, 0x65, 0x63, 0x74, 0x6f, 0x72, 0x33,
      0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x44,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x44, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40};

  std::istringstream input(std::string(data.data(), data.end()));

  absl::StatusOr<PropertyTree> pt_opt = readPropertyTree(input);
  ASSERT_TRUE(pt_opt.ok());

  absl::StatusOr<Vector3> vec = deserializeTree<Vector3>(*pt_opt);
  ASSERT_TRUE(vec.ok());

  EXPECT_DOUBLE_EQ(vec->x, 1.0);
  EXPECT_DOUBLE_EQ(vec->y, 2.0);
  EXPECT_DOUBLE_EQ(vec->z, 3.0);
}

struct NestedChild1 {
  int64_t _456;
  std::string ChildString;
};

struct NestedChild2 {
  double a, b, c, d;
};

struct Nested {
  int64_t _123;
  std::string Hello;
  double pi;
  NestedChild1 child1;
  NestedChild2 child2;
};

template <>
struct TypeDeserializer<Nested> {
  static std::vector<FieldDescriptor<Nested>> getFields() {
    return {
        makeSetter("123", &Nested::_123),
        makeSetter("Hello", &Nested::Hello),
        makeSetter("3.14", &Nested::pi),
        makeSetter("ChildNode1", &Nested::child1),
        makeSetter("ChildNode2", &Nested::child2),
    };
  }
};

template <>
struct TypeDeserializer<NestedChild1> {
  static std::vector<FieldDescriptor<NestedChild1>> getFields() {
    return {
        makeSetter("456", &NestedChild1::_456),
        makeSetter("ChildString", &NestedChild1::ChildString),
    };
  }
};

template <>
struct TypeDeserializer<NestedChild2> {
  static std::vector<FieldDescriptor<NestedChild2>> getFields() {
    return {
        makeSetter("a", &NestedChild2::a),
        makeSetter("b", &NestedChild2::b),
        makeSetter("c", &NestedChild2::c),
        makeSetter("d", &NestedChild2::d),
    };
  }
};

TEST(TestPropertyTree, NestedStructures) {
  // RootNode: 123 "Hello" 3.14 {
  //   ChildNode1: 456 "ChildString" {
  //   }
  //   ChildNode2: 1.00 2.00 3.00 4.00 {
  //   }
  // }

  constexpr std::array<uint8_t, 156> data = {
      0x8f, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00,
      0x00, 0x08, 0x52, 0x6f, 0x6f, 0x74, 0x4e, 0x6f, 0x64, 0x65, 0x4c,
      0x7b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x05, 0x00,
      0x00, 0x00, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x44, 0x1f, 0x85, 0xeb,
      0x51, 0xb8, 0x1e, 0x09, 0x40, 0x23, 0x00, 0x00, 0x00, 0x02, 0x00,
      0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x0a, 0x43, 0x68, 0x69, 0x6c,
      0x64, 0x4e, 0x6f, 0x64, 0x65, 0x31, 0x4c, 0xc8, 0x01, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x53, 0x0b, 0x00, 0x00, 0x00, 0x43, 0x68,
      0x69, 0x6c, 0x64, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x2e, 0x00,
      0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x0a,
      0x43, 0x68, 0x69, 0x6c, 0x64, 0x4e, 0x6f, 0x64, 0x65, 0x32, 0x44,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x44, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x44, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x08, 0x40, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x40};

  std::istringstream input(std::string(data.data(), data.end()));

  absl::StatusOr<PropertyTree> pt_opt = readPropertyTree(input);
  ASSERT_TRUE(pt_opt.ok());

  absl::StatusOr<Nested> nested = deserializeTree<Nested>(*pt_opt);
  ASSERT_TRUE(nested.ok());

  ASSERT_EQ(nested->_123, 123);
  ASSERT_EQ(nested->Hello, "Hello");
  ASSERT_EQ(nested->pi, 3.14);

  ASSERT_EQ(nested->child1._456, 456);
  ASSERT_EQ(nested->child1.ChildString, "ChildString");

  ASSERT_EQ(nested->child2.a, 1.0);
  ASSERT_EQ(nested->child2.b, 2.0);
  ASSERT_EQ(nested->child2.c, 3.0);
  ASSERT_EQ(nested->child2.d, 4.0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
