#pragma once

#include <span>
#include <cstdint>
#include <vector>

#include <absl/status/statusor.h>

enum class PixelFormat { Grayscale = 1, RGB = 3, RGBA = 4 };

class Image {
 public:
  Image(size_t w, size_t h, PixelFormat format, std::vector<uint8_t> &&data);

  Image();

  void resize(int width, int height, PixelFormat fmt);

  std::span<uint8_t> data(size_t x = 0, size_t y = 0);

  std::span<const uint8_t> data(size_t x = 0, size_t y = 0) const;

  size_t channels() const { return static_cast<size_t>(format_); }

  size_t w() const { return w_; }

  size_t h() const { return h_; }

  PixelFormat pixelFormat() const { return format_; }

 private:
  size_t w_ = 0;
  size_t h_ = 0;
  PixelFormat format_ = PixelFormat::Grayscale;
  std::vector<uint8_t> data_;
};

struct Glyph {
  Image image;
  int advance_x = 0;
};

struct Font {
  size_t num_glyphs = 0;
  std::vector<Glyph> glyphs;
  std::vector<uint32_t> glyph_map;

  std::optional<const Glyph> getGlyph(uint32_t codepoint) const noexcept;
};

// psf2.cpp
absl::StatusOr<Font> loadPSF2Font(const std::string &path);
