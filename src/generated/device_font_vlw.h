#pragma once

#include <cstddef>
#include <cstdint>

namespace homedeck::generated {

extern const std::uint8_t kDeviceFontVlw[];
inline constexpr std::size_t kDeviceFontVlwSize = 2600914U;
inline constexpr std::uint32_t kDeviceFontGlyphCount = 7541U;
inline constexpr std::uint32_t kDeviceFontPixelSize = 20U;

// LargeDate 字体实际 glyph 渲染高度约为 pixelSize * 13/16 ≈ 127px。
// 这一比例来自 VLW glyph 的 ascent/descent 分析。
// 倒计时视图使用 pixelSize * 13/32 作为文本垂直半高来居中布局。
extern const std::uint8_t kDeviceLargeDateFontVlw[];
inline constexpr std::size_t kDeviceLargeDateFontVlwSize = 179095U;
inline constexpr std::uint32_t kDeviceLargeDateFontGlyphCount = 19U;
inline constexpr std::uint32_t kDeviceLargeDateFontPixelSize = 156U;

extern const std::uint8_t kDeviceMetricFontVlw[];
inline constexpr std::size_t kDeviceMetricFontVlwSize = 6097U;
inline constexpr std::uint32_t kDeviceMetricFontGlyphCount = 19U;
inline constexpr std::uint32_t kDeviceMetricFontPixelSize = 28U;

extern const std::uint8_t kConfigPortalFontVlw[];
inline constexpr std::size_t kConfigPortalFontVlwSize = 15481U;
inline constexpr std::uint32_t kConfigPortalFontGlyphCount = 95U;
inline constexpr std::uint32_t kConfigPortalFontPixelSize = 20U;

}  // namespace homedeck::generated
