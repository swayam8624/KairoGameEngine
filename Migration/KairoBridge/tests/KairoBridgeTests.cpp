#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

import Kairo.Bridge;

namespace
{
    struct TemporaryProject final
    {
        std::filesystem::path Root;

        explicit TemporaryProject(std::string name)
            : Root(std::filesystem::temp_directory_path() / std::move(name))
        {
            std::filesystem::remove_all(Root);
            std::filesystem::create_directories(Root);
        }

        ~TemporaryProject()
        {
            std::error_code ignored;
            std::filesystem::remove_all(Root, ignored);
        }

        void Touch(const std::filesystem::path& relativePath)
        {
            const auto path = Root / relativePath;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream << "fixture\n";
        }
    };
}

TEST_CASE("KairoBridge detects Unity, Unreal, and Godot project roots")
{
    using namespace kairo::bridge;

    SECTION("Unity")
    {
        TemporaryProject project("kairo_bridge_unity_detection");
        std::filesystem::create_directories(project.Root / "Assets");
        project.Touch("ProjectSettings/ProjectVersion.txt");

        const auto descriptor = DetectSourceProject(project.Root);
        REQUIRE(descriptor.Engine == SourceEngine::Unity);
        REQUIRE(descriptor.ProjectFile.filename() == "ProjectVersion.txt");
    }

    SECTION("Unreal")
    {
        TemporaryProject project("kairo_bridge_unreal_detection");
        project.Touch("OpenWorld.uproject");

        const auto descriptor = DetectSourceProject(project.Root);
        REQUIRE(descriptor.Engine == SourceEngine::Unreal);
        REQUIRE(descriptor.DisplayName == "OpenWorld");
    }

    SECTION("Godot")
    {
        TemporaryProject project("kairo_bridge_godot_detection");
        project.Touch("project.godot");

        const auto descriptor = DetectSourceProject(project.Root);
        REQUIRE(descriptor.Engine == SourceEngine::Godot);
        REQUIRE(descriptor.ProjectFile.filename() == "project.godot");
    }
}

TEST_CASE("KairoBridge refuses ambiguous engine roots")
{
    using namespace kairo::bridge;

    TemporaryProject project("kairo_bridge_ambiguous_detection");
    std::filesystem::create_directories(project.Root / "Assets");
    project.Touch("ProjectSettings/ProjectVersion.txt");
    project.Touch("project.godot");

    REQUIRE_THROWS_AS(DetectSourceProject(project.Root), std::invalid_argument);
}

TEST_CASE("Canonical IR validates stable identities and graph references")
{
    using namespace kairo::bridge;

    CanonicalProjectIR project;
    project.Source = SourceEngine::Unity;
    project.ProjectName = "MigratedCity";

    CanonicalNode world;
    world.CanonicalID = "node/world";
    world.Source = {SourceEngine::Unity, "unity-guid-world", "Assets/World.unity"};
    world.Kind = CanonicalKind::World;
    world.Name = "World";
    world.Properties.push_back({"streaming", "enabled"});

    CanonicalNode player;
    player.CanonicalID = "node/player";
    player.Source = {SourceEngine::Unity, "unity-guid-player", "Assets/Player.prefab"};
    player.Kind = CanonicalKind::Prefab;
    player.Name = "Player";
    player.ParentCanonicalID = "node/world";
    player.Dependencies.push_back("node/world");

    project.Nodes = {player, world};
    project.Validate();
    project.SortDeterministically();

    REQUIRE(project.Nodes.front().CanonicalID == "node/player");
    REQUIRE(project.Nodes.back().CanonicalID == "node/world");
}

TEST_CASE("Migration manifest supports deterministic incremental upserts and coverage")
{
    using namespace kairo::bridge;

    MigrationManifest manifest;
    manifest.Source = SourceEngine::Unity;
    manifest.SourceProjectName = "MigratedCity";

    MigrationRecord material;
    material.Source = {SourceEngine::Unity, "guid-material", "Assets/Car.mat"};
    material.Kind = CanonicalKind::Material;
    material.CanonicalID = "asset/material/car";
    material.TargetID = "kairo:material:car";
    material.Disposition = MigrationDisposition::Native;
    material.Converter = "unity.material.pbr";
    material.ConverterVersion = 1u;
    material.SourceFingerprint = "sha256:first";
    manifest.Upsert(material);

    MigrationRecord script;
    script.Source = {SourceEngine::Unity, "guid-script", "Assets/Traffic.cs"};
    script.Kind = CanonicalKind::Script;
    script.CanonicalID = "script/traffic";
    script.TargetID = "kairo:compat:unity-script:traffic";
    script.Disposition = MigrationDisposition::Compatibility;
    script.Converter = "unity.csharp.compat";
    script.ConverterVersion = 1u;
    script.SourceFingerprint = "sha256:script";
    manifest.Upsert(script);

    // Reimport may advance converter implementation and source content while
    // preserving the durable canonical ID referenced by the rest of the graph.
    material.ConverterVersion = 2u;
    material.SourceFingerprint = "sha256:second";
    manifest.Upsert(material);

    // A converter bug or migration rule change must never silently rename the
    // canonical identity for the same source object, because that would break
    // prefab/scene/material references throughout a large migrated production.
    MigrationRecord unstableIdentity = material;
    unstableIdentity.CanonicalID = "asset/material/car-renamed";
    REQUIRE_THROWS_AS(manifest.Upsert(unstableIdentity), std::invalid_argument);

    manifest.Validate();
    manifest.SortDeterministically();

    const MigrationCoverage coverage = manifest.Coverage();
    REQUIRE(manifest.Records.size() == 2u);
    REQUIRE(coverage.Total == 2u);
    REQUIRE(coverage.Native == 1u);
    REQUIRE(coverage.Compatibility == 1u);
    REQUIRE(coverage.Runnable() == 2u);
    REQUIRE(coverage.RunnablePercent() == 100.0);
}
