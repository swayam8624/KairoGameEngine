#include <catch2/catch_test_macros.hpp>

#include <filesystem>

import Kairo.Bridge;

TEST_CASE("Durable source identities survive source-path moves")
{
    using namespace kairo::bridge;

    const SourceIdentity before{
        SourceEngine::Unity,
        "fedcbafedcbafedcbafedcbafedcbafe",
        "Assets/Car.mat"
    };
    const SourceIdentity after{
        SourceEngine::Unity,
        "fedcbafedcbafedcbafedcbafedcbafe",
        "Assets/Vehicles/Sports/Car.mat"
    };

    REQUIRE(SourceIdentityKey(before) == SourceIdentityKey(after));

    MigrationManifest manifest;
    manifest.Source = SourceEngine::Unity;
    manifest.SourceProjectName = "MigratedCity";

    MigrationRecord record;
    record.Source = before;
    record.Kind = CanonicalKind::Material;
    record.CanonicalID = "unity/fedcbafedcbafedcbafedcbafedcbafe";
    record.TargetID = "kairo:material:car";
    record.Disposition = MigrationDisposition::Native;
    record.Converter = "unity.material.pbr";
    record.ConverterVersion = 1u;
    record.SourceFingerprint = "sha256:before";
    manifest.Upsert(record);

    record.Source = after;
    record.ConverterVersion = 2u;
    record.SourceFingerprint = "sha256:after";
    manifest.Upsert(record);

    manifest.Validate();
    REQUIRE(manifest.Records.size() == 1u);
    REQUIRE(manifest.Records.front().Source.SourcePath ==
        std::filesystem::path("Assets/Vehicles/Sports/Car.mat"));
    REQUIRE(manifest.Records.front().CanonicalID ==
        "unity/fedcbafedcbafedcbafedcbafedcbafe");
}

TEST_CASE("Path-only source identities intentionally change when the source moves")
{
    using namespace kairo::bridge;

    const SourceIdentity before{SourceEngine::Godot, "", "scripts/player.gd"};
    const SourceIdentity after{SourceEngine::Godot, "", "gameplay/player.gd"};

    REQUIRE(SourceIdentityKey(before) != SourceIdentityKey(after));
}
