#pragma once

#include <cstddef>
#include <cstdint>

namespace sdk::studio {

constexpr std::int32_t kStudioHeaderId = 0x54534449;
constexpr std::size_t kHeaderLengthOffset = 0x4C;
constexpr std::size_t kHeaderBoneCountOffset = 0x9C;
constexpr std::size_t kHeaderBoneIndexOffset = 0xA0;
constexpr std::size_t kBoneParentOffset = 0x04;
constexpr std::size_t kBoneStride = 0xD8;
constexpr int kMaxBones = 256;
constexpr int kMaxStudioLength = 64 * 1024 * 1024;

} // namespace sdk::studio
