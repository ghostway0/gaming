#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include "sunset/image.h"

namespace {

static constexpr uint32_t kPSF2Magic = 0x864AB572;

struct PSF2Header {
  uint8_t magic[4];
  uint32_t version;
  uint32_t headersize;
  uint32_t flags;
  uint32_t length;
  uint32_t charsize;
  uint32_t height;
  uint32_t width;
};

Image flipImage(Image &image) {
  Image tmp;
  tmp.resize(image.w(), image.h(), image.pixelFormat());
  for (size_t y = 0; y < image.h(); ++y) {
    for (size_t x = 0; x < image.w(); ++x) {
      std::span<const uint8_t> src = image.data(x, y);
      std::span<uint8_t> dst = tmp.data(x, y);
      std::copy(src.data(), src.data() + image.channels(), dst.data());
    }
  }
  return tmp;
}

absl::Status loadGlyphs(std::ifstream &file, const PSF2Header &hdr,
                        Font &font) {
  size_t count = font.num_glyphs;
  size_t row_size = (hdr.width + 7) / 8;
  std::vector<uint8_t> bitmap(static_cast<size_t>(hdr.height) * row_size);

  for (size_t i = 0; i < count; ++i) {
    font.glyph_map[i] = static_cast<uint32_t>(i);
    Glyph &glyph = font.glyphs[i];
    glyph.image.resize(hdr.width, hdr.height, PixelFormat::Grayscale);
    glyph.advance_x = static_cast<int>(hdr.width);

    file.read(reinterpret_cast<char *>(bitmap.data()), bitmap.size());
    if (!file) return absl::InternalError("Failed to read glyph bitmap");

    for (uint32_t y = 0; y < hdr.height; ++y) {
      for (uint32_t x = 0; x < hdr.width; ++x) {
        uint8_t byte = bitmap[y * row_size + x / 8];
        bool set = byte & (1u << (7 - (x % 8)));
        uint8_t value = set ? 255u : 0u;
        glyph.image.data(x, y)[0] = value;
      }
    }

    glyph.image = flipImage(glyph.image);

    std::streamoff skip = static_cast<std::streamoff>(hdr.charsize) -
                          static_cast<std::streamoff>(bitmap.size());
    file.seekg(skip, std::ios::cur);
    if (!file) return absl::InternalError("Failed to skip glyph padding");
  }
  return absl::OkStatus();
}

absl::Status loadUnicodeTable(std::ifstream &file, Font &font) {
  for (size_t g = 0; g < font.num_glyphs; ++g) {
    uint8_t length = 0;
    file.read(reinterpret_cast<char *>(&length), sizeof(length));
    if (!file || file.eof()) break;
    if (length == 0xFF) continue;
    for (uint8_t i = 0; i < length; ++i) {
      uint32_t codepoint = 0;
      file.read(reinterpret_cast<char *>(&codepoint), sizeof(codepoint));
      if (!file)
        return absl::InternalError("Failed reading unicode codepoint");
      if (codepoint <= 0x10FFFF) {
        font.glyph_map[codepoint] = static_cast<uint32_t>(g);
      }
    }
  }
  return absl::OkStatus();
}

} // namespace

#define PSF2_HAS_UNICODE_TABLE 1 << 0

absl::StatusOr<Font> loadPSF2Font(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError("Cannot open font file: " + path);
  }

  PSF2Header hdr;
  file.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
  if (!file)
    return absl::InvalidArgumentError("Failed to read PSF2 header");

  if (reinterpret_cast<const uint32_t &>(hdr.magic) != kPSF2Magic) {
    return absl::InvalidArgumentError("Invalid PSF2 magic number");
  }

  Font font;
  font.num_glyphs = hdr.length;
  font.glyphs.resize(font.num_glyphs);
  font.glyph_map.assign(0x110000, 0);

  if (auto status = loadGlyphs(file, hdr, font); !status.ok()) {
    return status;
  }

  file.seekg(
      static_cast<std::streamoff>(hdr.headersize) +
          static_cast<std::streamoff>(font.num_glyphs) * hdr.charsize,
      std::ios::beg);
  if (!file) return absl::InternalError("Failed seeking to unicode table");

  if (hdr.flags & PSF2_HAS_UNICODE_TABLE) {
    if (auto status = loadUnicodeTable(file, font); !status.ok()) {
      return status;
    }
  }

  return font;
}

const Glyph *getGlyph(const Font &font, uint32_t codepoint) noexcept {
  if (codepoint > 0x10FFFF) return nullptr;
  uint32_t idx = font.glyph_map[codepoint];
  return idx < font.num_glyphs ? &font.glyphs[idx] : nullptr;
}
