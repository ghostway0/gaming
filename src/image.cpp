#include <cstddef>
#include <optional>

#include <absl/log/log.h>

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

  uint32_t idx = glyph_map[codepoint];

  if (idx >= num_glyphs) {
    return std::nullopt;
  }

  return glyphs[idx];
}

Image createFontAtlas(const Font &font) {
  if (font.glyphs.empty()) {
    return Image();
  }

  const size_t cols = std::ceil(std::sqrt(font.glyphs.size()));
  const size_t rows = std::ceil(font.glyphs.size() / float(cols));

  size_t out_width = cols * font.glyph_sizes.x;
  size_t out_height = rows * font.glyph_sizes.y;

  Image atlas(out_width, out_height, PixelFormat::Grayscale,
              std::vector<uint8_t>(out_width * out_height, 0));

  for (size_t i = 0; i < font.glyphs.size(); ++i) {
    const Glyph &glyph = font.glyphs[i];
    const Image &glyph_img = glyph.image;

    if (glyph_img.pixelFormat() != PixelFormat::Grayscale) continue;

    size_t gx = i % cols;
    size_t gy = i / cols;

    for (size_t y = 0; y < glyph_img.h(); ++y) {
      auto src_row = glyph_img.data(0, y).subspan(0, glyph_img.w());
      auto dst_row =
          atlas.data(gx * font.glyph_sizes.x, gy * font.glyph_sizes.y + y)
              .subspan(0, glyph_img.w());

      std::copy(src_row.begin(), src_row.end(), dst_row.begin());
    }
  }

  return atlas;
}

std::optional<size_t> Font::findGlyphIndex(uint32_t codepoint) {
  if (glyph_map[codepoint] == 0) {
    return std::nullopt;
  }
  return glyph_map[codepoint];
}
