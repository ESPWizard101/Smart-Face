#include "FrameBuffer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace gfx {

void FrameBuffer::clear(Color color) {
    buffer_.fill(color == Color::On ? 0xFF : 0x00);
}

void FrameBuffer::setPixel(int x, int y, Color color) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    uint8_t& cell = buffer_[static_cast<size_t>((y / 8) * kWidth + x)];
    const auto mask = static_cast<uint8_t>(1u << (y % 8));
    if (color == Color::On) {
        cell |= mask;
    } else {
        cell &= static_cast<uint8_t>(~mask);
    }
}

bool FrameBuffer::getPixel(int x, int y) const {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return false;
    }
    return (buffer_[static_cast<size_t>((y / 8) * kWidth + x)] >> (y % 8)) & 1u;
}

void FrameBuffer::drawHLine(int x, int y, int width, Color color) {
    if (y < 0 || y >= kHeight || width <= 0) {
        return;
    }
    const int start = std::max(x, 0);
    const int end = std::min(x + width, kWidth);
    if (start >= end) {
        return;
    }
    const auto mask = static_cast<uint8_t>(1u << (y % 8));
    uint8_t* row = buffer_.data() + (y / 8) * kWidth;
    if (color == Color::On) {
        for (int i = start; i < end; ++i) {
            row[i] |= mask;
        }
    } else {
        const auto inverse = static_cast<uint8_t>(~mask);
        for (int i = start; i < end; ++i) {
            row[i] &= inverse;
        }
    }
}

void FrameBuffer::fillRect(int x, int y, int width, int height, Color color) {
    const int top = std::max(y, 0);
    const int bottom = std::min(y + height, kHeight);
    for (int row = top; row < bottom; ++row) {
        drawHLine(x, row, width, color);
    }
}

void FrameBuffer::fillRoundRect(int x, int y, int width, int height, int radius, Color color) {
    if (width <= 0 || height <= 0) {
        return;
    }
    radius = std::clamp(radius, 0, std::min(width, height) / 2);

    for (int row = 0; row < height; ++row) {
        // Distance (in rows) into the top or bottom corner band; -1 for the straight middle part.
        int cornerRow = -1;
        if (row < radius) {
            cornerRow = radius - 1 - row;
        } else if (row >= height - radius) {
            cornerRow = row - (height - radius);
        }

        int inset = 0;
        if (cornerRow >= 0) {
            // Sample the circle at the centre of the pixel row for a symmetric outline.
            const float dy = static_cast<float>(cornerRow) + 0.5f;
            const float r = static_cast<float>(radius);
            inset = radius - static_cast<int>(std::lround(std::sqrt(std::max(0.0f, r * r - dy * dy))));
        }
        drawHLine(x + inset, y + row, width - 2 * inset, color);
    }
}

void FrameBuffer::fillCircle(int centerX, int centerY, int radius, Color color) {
    if (radius < 0) {
        return;
    }
    const float r = static_cast<float>(radius) + 0.5f;
    for (int dy = -radius; dy <= radius; ++dy) {
        const float fy = static_cast<float>(dy);
        const int half = static_cast<int>(std::lround(std::sqrt(std::max(0.0f, r * r - fy * fy)) - 0.5f));
        drawHLine(centerX - half, centerY + dy, 2 * half + 1, color);
    }
}

void FrameBuffer::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color) {
    // Sort vertices by y so that y0 <= y1 <= y2.
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    if (y1 > y2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    if (y0 == y2) {  // degenerate: a single horizontal line
        const int left = std::min({x0, x1, x2});
        const int right = std::max({x0, x1, x2});
        drawHLine(left, y0, right - left + 1, color);
        return;
    }

    auto edgeX = [](int xa, int ya, int xb, int yb, int y) {
        if (ya == yb) {
            return static_cast<float>(xa);
        }
        return static_cast<float>(xa) + static_cast<float>(xb - xa) * static_cast<float>(y - ya) /
                                            static_cast<float>(yb - ya);
    };

    const int first = std::max(y0, 0);
    const int last = std::min(y2, kHeight - 1);
    for (int y = first; y <= last; ++y) {
        float a = edgeX(x0, y0, x2, y2, y);  // long edge
        float b = (y < y1) ? edgeX(x0, y0, x1, y1, y) : edgeX(x1, y1, x2, y2, y);
        if (a > b) {
            std::swap(a, b);
        }
        const int left = static_cast<int>(std::lround(a));
        const int right = static_cast<int>(std::lround(b));
        drawHLine(left, y, right - left + 1, color);
    }
}

}  // namespace gfx
