#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

import Kairo.Bridge;

namespace
{
    struct TemporaryUnrealProject final
    {
        std::filesystem::path Root;

        explicit TemporaryUnrealProject(std::string name)
            : Root(std::filesystem::temp_directory_path() / std::move(name))
        {
            std::filesystem::remove_all(Root);
            std::filesystem::create_directories(Root);
        }

        ~TemporaryUnrealProject()
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

TEST_CASE("Unreal adapter preserves project package and plugin mount identities")
{
    using namespace kairo::bridge;

    TemporaryUnrealProject project("kairo_bridge_unreal_scan");
    project.Write("Metro.uproject",
        "{\n"
        "  \"FileVersion\": 3,\n"
        "  \"EngineAssociation\": \"5.5\",\n"
        "  \"Modules\": [\n"
        "    { \"Name\": \"Metro\", \"Type\": \"Runtime\" },\n"
        "    { \"Name\": \"MetroEditor\", \"Type\": \"Editor\" }\n"
        "  ],\n"
        "  \"Plugins\": [\n"
        "    { \"Name\": \"EnhancedInput\", \"Enabled\": true },\n"
        "    { \"Name\": \"GameplayAbilities\", \"Enabled\": true }\n"
        "  ]\n"
        "}\n");
    project.Write("Content/Maps/City.umap", "opaque-map-package");
    project.Write("Content/Vehicles/Car.uasset", "opaque-asset-package");
    project.Write("Content/SourceMeshes/Car.fbx", "source-mesh");
    project.Write("Content/Textures/Car.png", "texture");
    project.Write("Source/Metro/Player.cpp", "void Tick() {}\n");
    project.Write("Source/Metro/Metro.Build.cs", "public class Metro {}\n");
    project.Write("Plugins/Traffic/Traffic.uplugin",
        "{ \"FileVersion\": 3, \"FriendlyName\": \"Traffic\" }\n");
    project.Write("Plugins/Traffic/Content/AI/TrafficController.uasset", "plugin-package");
    project.Write("Plugins/Traffic/Source/Traffic/Traffic.cpp", "void PluginTick() {}\n");
    project.Write("Intermediate/ShouldNotScan.cpp", "generated\n");
    project.Write("Saved/Autosave.umap", "generated\n");

    const auto descriptor = DetectSourceProject(project.Root);
    REQUIRE(descriptor.Engine == SourceEngine::Unreal);
    REQUIRE(descriptor.DisplayName == "Metro");

    const UnrealProjectScan scan = ScanUnrealProject(descriptor);
    REQUIRE(scan.FileVersion == 3u);
    REQUIRE(scan.EngineAssociation == "5.5");
    REQUIRE(scan.Modules == std::vector<std::string>{"Metro", "MetroEditor"});
    REQUIRE(scan.Plugins == std::vector<std::string>{"EnhancedInput", "GameplayAbilities"});

    bool foundWorld = false;
    bool foundGamePackage = false;
    bool foundRawMesh = false;
    bool foundSource = false;
    bool foundPluginDescriptor = false;
    bool foundPluginPackage = false;
    for (const UnrealAssetRecord& asset : scan.Assets)
    {
        REQUIRE(asset.Source.SourcePath != std::filesystem::path("Intermediate/ShouldNotScan.cpp"));
        REQUIRE(asset.Source.SourcePath != std::filesystem::path("Saved/Autosave.umap"));

        if (asset.Source.SourcePath == std::filesystem::path("Content/Maps/City.umap"))
        {
            foundWorld = true;
            REQUIRE(asset.Kind == CanonicalKind::World);
            REQUIRE(asset.IsBinaryPackage);
            REQUIRE(asset.Source.StableID == "/Game/Maps/City");
            REQUIRE(asset.MountPoint == "/Game");
        }
        else if (asset.Source.SourcePath == std::filesystem::path("Content/Vehicles/Car.uasset"))
        {
            foundGamePackage = true;
            REQUIRE(asset.IsBinaryPackage);
            REQUIRE(asset.Source.StableID == "/Game/Vehicles/Car");
        }
        else if (asset.Source.SourcePath == std::filesystem::path("Content/SourceMeshes/Car.fbx"))
        {
            foundRawMesh = true;
            REQUIRE(asset.Kind == CanonicalKind::Mesh);
            REQUIRE_FALSE(asset.IsBinaryPackage);
            REQUIRE(asset.Source.StableID.empty());
        }
        else if (asset.Source.SourcePath == std::filesystem::path("Source/Metro/Player.cpp"))
        {
            foundSource = true;
            REQUIRE(asset.Kind == CanonicalKind::Script);
            REQUIRE(asset.Source.StableID.empty());
        }
        else if (asset.Source.SourcePath == std::filesystem::path("Plugins/Traffic/Traffic.uplugin"))
        {
            foundPluginDescriptor = true;
            REQUIRE(asset.IsPluginDescriptor);
            REQUIRE(asset.Source.StableID == "plugin:Traffic");
            REQUIRE(asset.MountPoint == "/Traffic");
        }
        else if (asset.Source.SourcePath ==
            std::filesystem::path("Plugins/Traffic/Content/AI/TrafficController.uasset"))
        {
            foundPluginPackage = true;
            REQUIRE(asset.IsBinaryPackage);
            REQUIRE(asset.Source.StableID == "/Traffic/AI/TrafficController");
            REQUIRE(asset.MountPoint == "/Traffic");
        }
    }
    REQUIRE(foundWorld);
    REQUIRE(foundGamePackage);
    REQUIRE(foundRawMesh);
    REQUIRE(foundSource);
    REQUIRE(foundPluginDescriptor);
    REQUIRE(foundPluginPackage);

    const CanonicalProjectIR ir = BuildUnrealDiscoveryIR(descriptor);
    REQUIRE(ir.Source == SourceEngine::Unreal);
    REQUIRE(ir.ProjectName == "Metro");

    bool foundWorldCanonical = false;
    bool foundPluginCanonical = false;
    for (const CanonicalNode& node : ir.Nodes)
    {
        if (node.Source.StableID == "/Game/Maps/City")
        {
            foundWorldCanonical = true;
            REQUIRE(node.CanonicalID == "unreal/package/Game/Maps/City");
        }
        if (node.Source.StableID == "plugin:Traffic")
        {
            foundPluginCanonical = true;
            REQUIRE(node.CanonicalID == "unreal/plugin/Traffic");
        }
    }
    REQUIRE(foundWorldCanonical);
    REQUIRE(foundPluginCanonical);
}

TEST_CASE("Unreal adapter rejects malformed project metadata")
{
    using namespace kairo::bridge;

    SECTION("missing positive FileVersion")
    {
        TemporaryUnrealProject project("kairo_bridge_unreal_missing_version");
        project.Write("Broken.uproject", "{ \"EngineAssociation\": \"5.5\" }\n");
        const auto descriptor = DetectSourceProject(project.Root);
        REQUIRE_THROWS_AS(ScanUnrealProject(descriptor), std::invalid_argument);
    }

    SECTION("unterminated array")
    {
        TemporaryUnrealProject project("kairo_bridge_unreal_bad_array");
        project.Write("Broken.uproject",
            "{ \"FileVersion\": 3, \"Modules\": [ { \"Name\": \"Broken\" } }\n");
        const auto descriptor = DetectSourceProject(project.Root);
        REQUIRE_THROWS_AS(ScanUnrealProject(descriptor), std::invalid_argument);
    }
}

TEST_CASE("Unreal package identity is independent of project filesystem root")
{
    using namespace kairo::bridge;

    const SourceIdentity first{SourceEngine::Unreal, "/Game/Vehicles/Car", "Content/Vehicles/Car.uasset"};
    const SourceIdentity relocated{SourceEngine::Unreal, "/Game/Vehicles/Car", "MigratedSource/Vehicles/Car.uasset"};
    REQUIRE(SourceIdentityKey(first) == SourceIdentityKey(relocated));
}
