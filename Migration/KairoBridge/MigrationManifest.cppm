module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Bridge.Manifest;

import Kairo.Bridge.Types;

export namespace kairo::bridge
{
    struct MigrationRecord final
    {
        SourceIdentity Source;
        CanonicalKind Kind = CanonicalKind::Other;
        std::string CanonicalID;
        std::string TargetID;
        MigrationDisposition Disposition = MigrationDisposition::NeedsAttention;
        std::string Converter;
        std::uint32_t ConverterVersion = 1u;
        std::string SourceFingerprint;

        void Validate() const
        {
            Source.Validate();
            if (CanonicalID.empty())
                throw std::invalid_argument("Migration records require a canonical ID.");
            if (Converter.empty())
                throw std::invalid_argument("Migration records require a converter identifier.");
            if (ConverterVersion == 0u)
                throw std::invalid_argument("Migration converter versions must be positive.");
            if ((Disposition == MigrationDisposition::Native ||
                 Disposition == MigrationDisposition::Compatibility) && TargetID.empty())
                throw std::invalid_argument(
                    "Runnable migration records require a target Kairo identity.");
        }
    };

    struct MigrationCoverage final
    {
        std::size_t Total = 0u;
        std::size_t Native = 0u;
        std::size_t Compatibility = 0u;
        std::size_t NeedsAttention = 0u;
        std::size_t Unsupported = 0u;

        [[nodiscard]] std::size_t Runnable() const noexcept
        {
            return Native + Compatibility;
        }

        [[nodiscard]] double RunnablePercent() const noexcept
        {
            if (Total == 0u) return 100.0;
            return static_cast<double>(Runnable()) * 100.0 /
                static_cast<double>(Total);
        }

        [[nodiscard]] double NativePercent() const noexcept
        {
            if (Total == 0u) return 100.0;
            return static_cast<double>(Native) * 100.0 /
                static_cast<double>(Total);
        }
    };

    /// Durable source-to-Kairo mapping used for incremental migration. The
    /// canonical identity survives converter upgrades, while SourceFingerprint
    /// determines whether the source object needs to be translated again.
    struct MigrationManifest final
    {
        std::uint32_t SchemaVersion = 1u;
        SourceEngine Source = SourceEngine::Unknown;
        std::string SourceProjectName;
        std::vector<MigrationRecord> Records;

        void SortDeterministically()
        {
            std::sort(Records.begin(), Records.end(),
                [](const MigrationRecord& left, const MigrationRecord& right)
                {
                    const std::string leftKey = SourceIdentityKey(left.Source);
                    const std::string rightKey = SourceIdentityKey(right.Source);
                    if (leftKey != rightKey) return leftKey < rightKey;
                    return left.CanonicalID < right.CanonicalID;
                });
        }

        void Upsert(MigrationRecord record)
        {
            record.Validate();
            if (Source == SourceEngine::Unknown) Source = record.Source.Engine;
            if (record.Source.Engine != Source)
                throw std::invalid_argument(
                    "Migration manifest cannot mix source engines.");

            const std::string key = SourceIdentityKey(record.Source);
            const auto existing = std::find_if(Records.begin(), Records.end(),
                [&](const MigrationRecord& candidate)
                {
                    return SourceIdentityKey(candidate.Source) == key;
                });
            if (existing == Records.end())
                Records.push_back(std::move(record));
            else
                *existing = std::move(record);
        }

        [[nodiscard]] MigrationCoverage Coverage() const noexcept
        {
            MigrationCoverage coverage;
            coverage.Total = Records.size();
            for (const MigrationRecord& record : Records)
            {
                switch (record.Disposition)
                {
                    case MigrationDisposition::Native: ++coverage.Native; break;
                    case MigrationDisposition::Compatibility: ++coverage.Compatibility; break;
                    case MigrationDisposition::NeedsAttention: ++coverage.NeedsAttention; break;
                    case MigrationDisposition::Unsupported: ++coverage.Unsupported; break;
                }
            }
            return coverage;
        }

        void Validate() const
        {
            if (SchemaVersion == 0u)
                throw std::invalid_argument("Migration manifest schema version must be positive.");
            if (Source == SourceEngine::Unknown)
                throw std::invalid_argument("Migration manifests require a known source engine.");
            if (SourceProjectName.empty())
                throw std::invalid_argument("Migration manifests require a source project name.");

            std::unordered_set<std::string> sourceKeys;
            std::unordered_set<std::string> canonicalIDs;
            for (const MigrationRecord& record : Records)
            {
                record.Validate();
                if (record.Source.Engine != Source)
                    throw std::invalid_argument(
                        "Migration record engine does not match its manifest.");
                if (!sourceKeys.insert(SourceIdentityKey(record.Source)).second)
                    throw std::invalid_argument(
                        "Migration manifests cannot contain duplicate source identities.");
                if (!canonicalIDs.insert(record.CanonicalID).second)
                    throw std::invalid_argument(
                        "Migration manifests cannot contain duplicate canonical IDs.");
            }
        }
    };
}
