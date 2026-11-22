// CPU-only regression for LoadRenderMaterial's PRE-allocation byte cap.
// No 1 GiB buffer, GPU device, file-backed sparse image, or asset import is
// needed to inject malicious std::streamoff lengths.
#include "../MaterialFileSizeBudget.hpp"

#include <cstdio>
#include <cstdint>
#include <ios>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    void Require(bool ok, const char* message)
    {
        if (!ok) throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        using kairo::runtime::renderbridge::detail::CheckedMaterialFileBytes;
        using kairo::runtime::renderbridge::detail::MaximumMaterialArtifactFileBytes;

        // A real empty or ordinary material file is allowed through the
        // SIZE gate; the subsequent derived-artifact parser rejects bad data.
        Require(CheckedMaterialFileBytes(std::streamoff{ 0 }) == 0u,
            "Zero-length file was incorrectly rejected by the size gate");
        Require(CheckedMaterialFileBytes(std::streamoff{ 176 }) == 176u,
            "A canonical material-length input was rejected");
        Require(CheckedMaterialFileBytes(
            static_cast<std::streamoff>(MaximumMaterialArtifactFileBytes)) ==
            MaximumMaterialArtifactFileBytes,
            "Exactly 1 MiB must be allowed to reach the binary parser");

        bool negativeRejected = false;
        try { (void)CheckedMaterialFileBytes(std::streamoff{ -1 }); }
        catch (const std::runtime_error&) { negativeRejected = true; }
        Require(negativeRejected, "Negative tellg length did not fail");

        bool oversizedRejected = false;
        try
        {
            (void)CheckedMaterialFileBytes(
                static_cast<std::streamoff>(MaximumMaterialArtifactFileBytes + 1u));
        }
        catch (const std::length_error&) { oversizedRejected = true; }
        Require(oversizedRejected, "First byte above limit was not rejected");

        bool giganticRejected = false;
        try
        {
            (void)CheckedMaterialFileBytes(
                std::numeric_limits<std::streamoff>::max());
        }
        catch (const std::length_error&) { giganticRejected = true; }
        Require(giganticRejected, "A malicious enormous file could allocate");

        std::puts("Kairo material artifact size budget tests passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Material file budget regression: %s\n",
            error.what());
        return 1;
    }
}
