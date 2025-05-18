#include <cstddef>
#include <optional>

#include "sunset/image.h"

Image::Image(size_t w, size_t h, PixelFormat format,
             std::vector<uint8_t> &&data)
    : w_{w}, h_{h}, format_{format}, data_{data} {}

Image::Image() : w_{0}, h_{0}, format_{0}, data_{} {}

void Image::resize(int width, int height, PixelFormat fmt) {
  w_ = width;
  h_ = height;
  format_ = fmt;
  data_.assign(static_cast<size_t>(w_) * h_ * channels(), 0);
}

std::span<uint8_t> Image::data(size_t x, size_t y) {
  return std::span(
      data_.data() + (static_cast<size_t>(y) * w_ + x) * channels(),
      data_.size() - x * y);
}

std::span<const uint8_t> Image::data(size_t x, size_t y) const {
  return std::span(
      data_.data() + (static_cast<size_t>(y) * w_ + x) * channels(),
      data_.size() - x * y);
}

std::optional<const Glyph> Font::getGlyph(
    uint32_t codepoint) const noexcept {
  if (codepoint > 0x10FFFF) {
    return std::nullopt;
  }

  uint32_t idx = this->glyph_map[codepoint];

  if (idx >= this->num_glyphs) {
    return std::nullopt;
  }

  return this->glyphs[idx];
}
