#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    // Bound allocations before opening the payload. Detect truncation or
    // growth between size inspection and the actual read.
    // Shared by Craft checkpoint envelopes and RuntimeQuestSave.hpp. This
    // function only handles bytes; callers validate their own magic/version
    // and domain invariants. Returned vector owns the file contents.
    inline std::vector<std::uint8_t> ReadCraftSaveFile(const std::filesystem::path& path,std::size_t maximum)
    {
        if(!std::filesystem::is_regular_file(path))throw std::invalid_argument("Craft save is not a regular file.");
        // Reject file sizes that exceed the caller's format budget or the
        // representable stream read length before allocating a vector.
        const auto size=std::filesystem::file_size(path);
        if(!size||size>maximum||size>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
            throw std::length_error("Craft save size outside bounds.");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream file(path,std::ios::binary);
        if(!file||!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))||
            file.peek()!=std::char_traits<char>::eof()||file.bad())
            throw std::runtime_error("Craft save read failed.");
        return bytes;
    }

    // A sibling directory is an exclusive reservation among cooperating
    // writers. Unlike exists()+open(), create_directory is one operation.
    // Rename a closed file into place without deleting the previous save.
    // This is atomic replacement on supported local filesystems, NOT fsync
    // crash durability. A stale reservation requires explicit recovery.
    // Caller provides a fully encoded immutable span and a format limit.
    // Existing destination is replaced only after the staged stream closes;
    // the caller owns the parent directory and must handle reported failures.
    inline void WriteCraftSaveFile(const std::filesystem::path& path,
        std::span<const std::uint8_t> bytes,std::size_t maximum)
    {
        if(path.empty()||bytes.empty()||bytes.size()>maximum||
            bytes.size()>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
            throw std::invalid_argument("Invalid Craft save write request.");
        auto reservation=path;reservation+=".write-lock";
        const auto staged=reservation/"payload";
        if(!std::filesystem::create_directory(reservation))
            throw std::runtime_error("Craft save writer reservation already exists.");
        // Best-effort cleanup cannot mask the original exception. A stale
        // reservation is deliberately not silently removed on the next write.
        const auto cleanup=[&]() noexcept
        {
            std::error_code ignored;
            std::filesystem::remove(staged,ignored);
            std::filesystem::remove(reservation,ignored);
        };
        try
        {
            std::ofstream file(staged,std::ios::binary|std::ios::trunc);
            if(!file||!file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
                throw std::runtime_error("Craft save write failed.");
            file.close();
            if(!file)throw std::runtime_error("Craft save close failed.");
            // Publication boundary: same-filesystem rename, without a window
            // where the previous destination is deliberately removed.
            std::filesystem::rename(staged,path);
        }
        catch(...){cleanup();throw;}
        cleanup();
    }
}
