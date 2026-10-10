#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

import Kairo.Bridge;

namespace
{
    struct TemporaryGodotProject final
    {
        std::filesystem::path Root;

        explicit TemporaryGodotProject(std::string name)
            : Root(std::filesystem::temp_directory_path() / std::move(name))
        {
            std::filesystem::remove_all(Root);
            std::filesystem::create_directories(Root);
        }

        ~TemporaryGodotProject()
        {
            std::error_code ignored;
            std::filesystem::remove_all(Root, ignored);
        }

        void Write(const std::filesystem::path& relativePath, const std::string& content)
        {
            const auto path = Root / relativePath;
            std::filesystem::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream << content;
        }
    };
}

TEST_CASE("Godot adapter preserves UID resources and classifies path-only source assets")
{
    using namespace kairo::bridge;

    TemporaryGodotProject project("kairo_bridge_godot_scan");
    project.Write("project.godot",
        "config_version=5\n\n"
        "[application]\n"
        "config/name=\"Metro Demo\"\n"
        "run/main_scene=\"uid://maincity\"\n");
    project.Write("world/main.tscn",
        "[gd_scene load_steps=2 format=3 uid=\"uid://maincity\"]\n");
    project.Write("materials/asphalt.tres",
        "[gd_resource type=\"StandardMaterial3D\" format=3 uid=\"uid://asphaltmat\"]\n");
    project.Write("scripts/player.gd",
        "extends CharacterBody3D\n");
    project.Write("models/car.glb", "binary-fixture");
    project.Write("audio/horn.ogg", "audio-fixture");
    project.Write("README.md", "documentation must not become a migrated resource\n");
    project.Write(".godot/generated.tres",
        "[gd_resource type=\"StandardMaterial3D\" format=3 uid=\"uid://generated\"]\n");

    const auto descriptor = DetectSourceProject(project.Root);
    const GodotProjectScan scan = ScanGodotProject(descriptor);
    REQUIRE(scan.ConfigVersion == 5u);
    REQUIRE(scan.ProjectName == "Metro Demo");
    REQUIRE(scan.MainScene == "uid://maincity");
    REQUIRE(scan.Resources.size() == 5u);

    bool foundScene = false;
    bool foundMaterial = false;
    bool foundScript = false;
    bool foundMesh = false;
    bool foundAudio = false;
    for (const GodotResourceRecord& resource : scan.Resources)
    {
        REQUIRE(resource.Source.SourcePath != std::filesystem::path(".godot/generated.tres"));
        REQUIRE(resource.Source.SourcePath != std::filesystem::path("README.md"));

        if (resource.Source.SourcePath == std::filesystem::path("world/main.tscn"))
        {
            foundScene = true;
            REQUIRE(resource.Kind == CanonicalKind::Scene);
            REQUIRE(resource.HasExplicitUID);
            REQUIRE(resource.Source.StableID == "uid://maincity");
        }
        else if (resource.Source.SourcePath == std::filesystem::path("materials/asphalt.tres"))
        {
            foundMaterial = true;
            REQUIRE(resource.Kind == CanonicalKind::Material);
            REQUIRE(resource.ResourceType == "StandardMaterial3D");
            REQUIRE(resource.HasExplicitUID);
        }
        else if (resource.Source.SourcePath == std::filesystem::path("scripts/player.gd"))
        {
            foundScript = true;
            REQUIRE(resource.Kind == CanonicalKind::Script);
            REQUIRE_FALSE(resource.HasExplicitUID);
            REQUIRE(resource.Source.StableID.empty());
        }
        else if (resource.Source.SourcePath == std::filesystem::path("models/car.glb"))
        {
            foundMesh = true;
            REQUIRE(resource.Kind == CanonicalKind::Mesh);
        }
        else if (resource.Source.SourcePath == std::filesystem::path("audio/horn.ogg"))
        {
            foundAudio = true;
            REQUIRE(resource.Kind == CanonicalKind::AudioClip);
        }
    }
    REQUIRE(foundScene);
    REQUIRE(foundMaterial);
    REQUIRE(foundScript);
    REQUIRE(foundMesh);
    REQUIRE(foundAudio);

    const CanonicalProjectIR ir = BuildGodotDiscoveryIR(descriptor);
    REQUIRE(ir.Source == SourceEngine::Godot);
    REQUIRE(ir.ProjectName == "Metro Demo");
    REQUIRE(ir.Nodes.size() == 5u);

    bool foundUIDCanonicalID = false;
    bool foundPathCanonicalID = false;
    for (const CanonicalNode& node : ir.Nodes)
    {
        if (node.Source.SourcePath == std::filesystem::path("world/main.tscn"))
        {
            foundUIDCanonicalID = true;
            REQUIRE(node.CanonicalID == "godot/uid/maincity");
        }
        if (node.Source.SourcePath == std::filesystem::path("scripts/player.gd"))
        {
            foundPathCanonicalID = true;
            REQUIRE(node.CanonicalID == "godot/path/scripts/player.gd");
        }
    }
    REQUIRE(foundUIDCanonicalID);
    REQUIRE(foundPathCanonicalID);
}

TEST_CASE("Godot adapter rejects duplicate explicit resource UIDs")
{
    using namespace kairo::bridge;

    TemporaryGodotProject project("kairo_bridge_godot_duplicate_uid");
    project.Write("project.godot", "config_version=5\n");
    project.Write("a.tscn", "[gd_scene format=3 uid=\"uid://duplicate\"]\n");
    project.Write("b.tres",
        "[gd_resource type=\"StandardMaterial3D\" format=3 uid=\"uid://duplicate\"]\n");

    const auto descriptor = DetectSourceProject(project.Root);
    REQUIRE_THROWS_AS(ScanGodotProject(descriptor), std::invalid_argument);
}

TEST_CASE("Godot adapter rejects malformed explicit UIDs and missing config versions")
{
    using namespace kairo::bridge;

    SECTION("malformed UID")
    {
        TemporaryGodotProject project("kairo_bridge_godot_bad_uid");
        project.Write("project.godot", "config_version=5\n");
        project.Write("bad.tscn", "[gd_scene format=3 uid=\"broken uid\"]\n");
        const auto descriptor = DetectSourceProject(project.Root);
        REQUIRE_THROWS_AS(ScanGodotProject(descriptor), std::invalid_argument);
    }

    SECTION("missing config version")
    {
        TemporaryGodotProject project("kairo_bridge_godot_missing_config");
        project.Write("project.godot", "[application]\nconfig/name=\"Missing Version\"\n");
        const auto descriptor = DetectSourceProject(project.Root);
        REQUIRE_THROWS_AS(ScanGodotProject(descriptor), std::invalid_argument);
    }
}
