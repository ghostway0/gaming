#include <array>
#include <cstdint>
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
  std::array<uint8_t, 38> data = {0x23, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x56,
                                  0x65, 0x63, 0x74, 0x6F, 0x72, 0x33, 0x46,
                                  0x00, 0x00, 0x80, 0x3F, 0x46, 0x00, 0x00,
                                  0x00, 0x40, 0x46, 0x00, 0x00, 0x40, 0x40};

  std::istringstream input(std::string(data.data(), data.end()));

  absl::StatusOr<PropertyTree> pt_opt = readPropertyTree(input);
  ASSERT_TRUE(pt_opt.ok());

  absl::StatusOr<Vector3> vec = deserializeTree<Vector3>(*pt_opt);
  ASSERT_TRUE(vec.ok());

  EXPECT_DOUBLE_EQ(vec->x, 1.0);
  EXPECT_DOUBLE_EQ(vec->y, 2.0);
  EXPECT_DOUBLE_EQ(vec->z, 3.0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
