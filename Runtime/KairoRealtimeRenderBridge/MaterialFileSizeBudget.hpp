#pragma once

// Guard the C++ byte-vector allocation BEFORE parsing a project material
// artifact. A material record is fixed-schema (PBR factors and at most five
// 16-byte typed texture IDs); valid files are far smaller than 1 MiB.
//
// DerivedArtifact::ParseDerivedArtifact validates its own payload length,
// but that check runs AFTER the caller has already read the entire input.
// A malicious sparse/oversized file must not force a gigabyte allocation
// simply because it is registered under a MaterialAssetHandle.
//
// This helper does not import assets or own a RendererRuntime. Tests can
// inject synthetic std::streamoff lengths without allocating huge files.

#include <cstddef>
#include <cstdint>
#include <ios>
#include <stdexcept>

namespace kairo::runtime::renderbridge::detail
{
    inline constexpr std::size_t MaximumMaterialArtifactFileBytes =
        1024u * 1024u; // 1 MiB, generous above canonical fixed-size payload

    [[nodiscard]] inline std::size_t CheckedMaterialFileBytes(
        std::streamoff length)
    {
        if (length < 0)
            throw std::runtime_error("Cannot measure material artifact file size.");
        if (static_cast<std::uintmax_t>(length) >
            static_cast<std::uintmax_t>(MaximumMaterialArtifactFileBytes))
            throw std::length_error(
                "Material artifact file exceeds the 1 MiB safety limit.");
        return static_cast<std::size_t>(length);
    }
}
