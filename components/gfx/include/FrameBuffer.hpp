#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace gfx {

enum class Color : uint8_t { Off, On };

class FrameBuffer {
public:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;
    static constexpr size_t kSize = static_cast<size_t>(kWidth) * kHeight / 8;

    void clear(Color color = Color::Off);

    void setPixel(int x, int y, Color color);
    bool getPixel(int x, int y) const;

    void drawHLine(int x, int y, int width, Color color);
    void fillRect(int x, int y, int width, int height, Color color);
    void fillRoundRect(int x, int y, int width, int height, int radius, Color color);
    void fillCircle(int centerX, int centerY, int radius, Color color);
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color);

    std::span<const uint8_t, kSize> data() const { return buffer_; }

private:
    alignas(4) std::array<uint8_t, kSize> buffer_{};
};

}  // namespace gfx
