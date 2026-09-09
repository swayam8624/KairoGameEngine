module;

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

export module Kairo.Bridge.CanonicalIR;

import Kairo.Bridge.Types;

export namespace kairo::bridge
{
    /// Loss-preserving property carried by the canonical IR. Initial adapters
    /// use stable textual encodings so source-specific information can survive
    /// before a native Kairo schema exists. Typed property schemas can be layered
    /// on top without changing source identity or graph structure.
    struct CanonicalProperty final
    {
        std::string Key;
        std::string Value;
    };

    /// One semantic source object after translation out of an engine-specific
    /// representation. CanonicalID is KairoBridge-owned and must remain stable
    /// across incremental reimports of the same SourceIdentity.
    struct CanonicalNode final
    {
        std::string CanonicalID;
        SourceIdentity Source;
        CanonicalKind Kind = CanonicalKind::Other;
        std::string Name;
        std::string ParentCanonicalID;
        std::vector<std::string> Dependencies;
        std::vector<CanonicalProperty> Properties;

        void Validate() const
        {
            if (CanonicalID.empty())
                throw std::invalid_argument("Canonical migration nodes require an ID.");
            Source.Validate();
            if (ParentCanonicalID == CanonicalID)
                throw std::invalid_argument("Canonical migration nodes cannot parent themselves.");

            std::unordered_set<std::string> dependencySet;
            for (const std::string& dependency : Dependencies)
            {
                if (dependency.empty())
                    throw std::invalid_argument("Canonical dependencies cannot be empty.");
                if (dependency == CanonicalID)
                    throw std::invalid_argument("Canonical migration nodes cannot depend on themselves.");
                if (!dependencySet.insert(dependency).second)
                    throw std::invalid_argument("Canonical dependencies cannot contain duplicates.");
            }

            std::unordered_set<std::string> propertyKeys;
            for (const CanonicalProperty& property : Properties)
            {
                if (property.Key.empty())
                    throw std::invalid_argument("Canonical properties require a key.");
                if (!propertyKeys.insert(property.Key).second)
                    throw std::invalid_argument("Canonical property keys must be unique per node.");
            }
        }
    };

    struct CanonicalProjectIR final
    {
        std::uint32_t SchemaVersion = 1u;
        SourceEngine Source = SourceEngine::Unknown;
        std::string ProjectName;
        std::vector<CanonicalNode> Nodes;

        void SortDeterministically()
        {
            for (CanonicalNode& node : Nodes)
            {
                std::sort(node.Dependencies.begin(), node.Dependencies.end());
                std::sort(node.Properties.begin(), node.Properties.end(),
                    [](const CanonicalProperty& left, const CanonicalProperty& right)
                    {
                        if (left.Key != right.Key) return left.Key < right.Key;
                        return left.Value < right.Value;
                    });
            }

            std::sort(Nodes.begin(), Nodes.end(),
                [](const CanonicalNode& left, const CanonicalNode& right)
                {
                    if (left.CanonicalID != right.CanonicalID)
                        return left.CanonicalID < right.CanonicalID;
                    return SourceIdentityKey(left.Source) < SourceIdentityKey(right.Source);
                });
        }

        void Validate() const
        {
            if (SchemaVersion == 0u)
                throw std::invalid_argument("Canonical migration schema version must be positive.");
            if (Source == SourceEngine::Unknown)
                throw std::invalid_argument("Canonical projects require a known source engine.");
            if (ProjectName.empty())
                throw std::invalid_argument("Canonical projects require a project name.");

            std::unordered_set<std::string> canonicalIDs;
            std::unordered_set<std::string> sourceIDs;
            for (const CanonicalNode& node : Nodes)
            {
                node.Validate();
                if (node.Source.Engine != Source)
                    throw std::invalid_argument(
                        "Canonical project nodes must match the project's source engine.");
                if (!canonicalIDs.insert(node.CanonicalID).second)
                    throw std::invalid_argument("Canonical project node IDs must be unique.");
                if (!sourceIDs.insert(SourceIdentityKey(node.Source)).second)
                    throw std::invalid_argument(
                        "One source identity cannot map to multiple canonical nodes.");
            }

            for (const CanonicalNode& node : Nodes)
            {
                if (!node.ParentCanonicalID.empty() &&
                    !canonicalIDs.contains(node.ParentCanonicalID))
                    throw std::invalid_argument(
                        "Canonical parent references must resolve inside the project IR.");
                for (const std::string& dependency : node.Dependencies)
                    if (!canonicalIDs.contains(dependency))
                        throw std::invalid_argument(
                            "Canonical dependency references must resolve inside the project IR.");
            }
        }
    };
}
