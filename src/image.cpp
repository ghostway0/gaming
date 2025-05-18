#include <cstddef>
#include <optional>

#include "sunset/image.h"

Image::Image(size_t w, size_t h, PixelFormat format,
             std::vector<uint8_t> &&data)
    : w_{w}, h_{h}, format_{format}, data_{data} {}

Image::Image(PixelFormat format) : w_{0}, h_{0}, format_{format}, data_{} {}

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

Image createFontAtlas(const Font &font) {
  if (font.glyphs.empty()) {
    return Image();
  }

  int glyph_w = 0, glyph_h = 0;
  for (const auto &glyph : font.glyphs) {
    glyph_w = std::max(glyph_w, static_cast<int>(glyph.image.w()));
    glyph_h = std::max(glyph_h, static_cast<int>(glyph.image.h()));
  }

  const int cols =
      static_cast<int>(std::ceil(std::sqrt(font.glyphs.size())));
  const int rows =
      static_cast<int>(std::ceil(font.glyphs.size() / float(cols)));

  size_t out_width = cols * glyph_w;
  size_t out_height = rows * glyph_h;

  Image atlas(out_width, out_height, PixelFormat::Grayscale,
              std::vector<uint8_t>(out_width * out_height, 0));

  for (size_t i = 0; i < font.glyphs.size(); ++i) {
    const Glyph &glyph = font.glyphs[i];
    const Image &glyph_img = glyph.image;

    if (glyph_img.pixelFormat() != PixelFormat::Grayscale) continue;

    int gx = static_cast<int>(i % cols);
    int gy = static_cast<int>(i / cols);
    int dst_x = gx * glyph_w;
    int dst_y = gy * glyph_h;

    for (size_t y = 0; y < glyph_img.h(); ++y) {
      auto src_row = glyph_img.data(0, y).subspan(0, glyph_img.w());
      auto dst_row = atlas.data(dst_x, dst_y + y).subspan(0, glyph_img.w());

      std::copy(src_row.begin(), src_row.end(), dst_row.begin());
    }
  }

  return atlas;
}
