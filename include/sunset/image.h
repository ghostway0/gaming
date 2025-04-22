#pragma once

#include <span>
#include <cstdint>
#include <vector>

enum class PixelFormat { Grayscale = 1, RGB = 3, RGBA = 4 };

class Image {
 public:
  Image(size_t w, size_t h, PixelFormat format, std::vector<uint8_t> &&data)
      : w_{w}, h_{h}, format_{format}, data_{data} {}

  Image() : w_{0}, h_{0}, format_{0}, data_{} {}

  size_t channels() const { return static_cast<size_t>(format_); }

  void resize(int width, int height, PixelFormat fmt) {
    w_ = width;
    h_ = height;
    format_ = fmt;
    data_.assign(static_cast<size_t>(w_) * h_ * channels(), 0);
  }

  std::span<uint8_t> data(size_t x = 0, size_t y = 0) {
    return std::span(
        data_.data() + (static_cast<size_t>(y) * w_ + x) * channels(),
        data_.size() - x * y);
  }
  
  std::span<const uint8_t> data(size_t x = 0, size_t y = 0) const {
    return std::span(
        data_.data() + (static_cast<size_t>(y) * w_ + x) * channels(),
        data_.size() - x * y);
  }

  size_t w() { return w_; }

  size_t h() { return h_; }

  PixelFormat pixelFormat() { return format_; }

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
};
