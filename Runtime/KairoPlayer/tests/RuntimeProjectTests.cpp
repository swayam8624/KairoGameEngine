#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>

import Kairo.Player.RuntimeProject;
import Kairo.Player.RuntimePhysicsBridge;
import Kairo.Player.RuntimeLogicBridge;
import Kairo.EngineCore;
import Kairo.Foundation.Math;

namespace
{
    void Write(const std::filesystem::path& path, const char* text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << text;
        if (!output) throw std::runtime_error("Fixture write failed.");
    }

    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    struct PhysicsFixture final
    {
        kairo::engine::Scene Scene;
        kairo::engine::Entity Ball;
        kairo::engine::Entity Floor;
    };

    [[nodiscard]] PhysicsFixture MakePhysicsFixture()
    {
        PhysicsFixture fixture;
        fixture.Floor = fixture.Scene.CreateEntity("Floor");
        fixture.Scene.Transform(fixture.Floor).Local.Translation = { 0.0f, -0.5f, 0.0f };
        kairo::engine::ColliderComponent floorCollider;
        floorCollider.Shape = kairo::engine::ColliderShape::Box;
        floorCollider.HalfExtents = { 5.0f, 0.5f, 5.0f };
        fixture.Scene.SetCollider(fixture.Floor, floorCollider);

        fixture.Ball = fixture.Scene.CreateEntity("Ball");
        fixture.Scene.Transform(fixture.Ball).Local.Translation = { 0.0f, 3.0f, 0.0f };
        fixture.Scene.SetRigidBody(fixture.Ball, {});
        kairo::engine::ColliderComponent ballCollider;
        ballCollider.Shape = kairo::engine::ColliderShape::Sphere;
        ballCollider.Radius = 0.5f;
        fixture.Scene.SetCollider(fixture.Ball, ballCollider);
        return fixture;
    }

    void TestRuntimePhysics()
    {
        auto fixture = MakePhysicsFixture();
        kairo::player::RuntimePhysicsBridge physics(fixture.Scene);
        bool sawContact = false;
        for (unsigned frame = 0u; frame < 180u; ++frame)
        {
            const auto advanced = physics.Advance(1.0f / 60.0f);
            Require(advanced.Steps == 1u, "Fixed-rate frame did not execute exactly one physics step.");
            sawContact = sawContact || !physics.ContactEvents().empty();
        }
        const auto ballBody = physics.BodyFor(fixture.Ball);
        Require(ballBody.has_value(), "Authored dynamic body has no runtime mapping.");
        const auto& state = physics.World().Bodies().at(*ballBody).State;
        Require(state.Position.y < 1.0f && state.Position.y > 0.35f,
            "Falling sphere did not settle on the authored floor.");
        Require(sawContact, "Falling sphere generated no mapped contact events.");
        const auto hit = physics.Raycast({ 0.0f, 5.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 10.0f);
        Require(hit.has_value() && hit->first == fixture.Ball,
            "Entity-aware raycast did not return the nearest authored sphere.");
        physics.ApplyEntityImpulse(fixture.Ball, { 0.0, 2.0, 0.0 });
        Require(physics.World().Bodies().at(*ballBody).State.LinearVelocity.y > 0.0f,
            "Entity-aware impulse did not reach the mapped dynamic body.");

        auto sixtyHertz = MakePhysicsFixture();
        auto thirtyHertz = MakePhysicsFixture();
        kairo::player::RuntimePhysicsBridge simulationA(sixtyHertz.Scene);
        kairo::player::RuntimePhysicsBridge simulationB(thirtyHertz.Scene);
        for (unsigned frame = 0u; frame < 120u; ++frame) (void)simulationA.Advance(1.0f / 60.0f);
        for (unsigned frame = 0u; frame < 60u; ++frame) (void)simulationB.Advance(1.0f / 30.0f);
        const auto bodyA = simulationA.BodyFor(sixtyHertz.Ball).value();
        const auto bodyB = simulationB.BodyFor(thirtyHertz.Ball).value();
        const auto positionA = simulationA.World().Bodies().at(bodyA).State.Position;
        const auto positionB = simulationB.World().Bodies().at(bodyB).State.Position;
        Require(kairo::foundation::math::NearlyEqual(positionA, positionB, 1.0e-5f),
            "Fixed physics diverged when fed equivalent 60 Hz and 30 Hz frame time.");

        kairo::engine::Scene hierarchy;
        const auto parent = hierarchy.CreateEntity("Parent");
        hierarchy.Transform(parent).Local.Translation = { 2.0f, 0.0f, 0.0f };
        const auto child = hierarchy.CreateEntity("Child");
        hierarchy.SetParent(child, parent);
        hierarchy.Transform(child).Local.Translation = { 0.0f, 2.0f, 0.0f };
        hierarchy.SetRigidBody(child, {});
        kairo::engine::ColliderComponent childCollider;
        childCollider.Shape = kairo::engine::ColliderShape::Sphere;
        hierarchy.SetCollider(child, childCollider);
        kairo::player::RuntimePhysicsBridge hierarchyPhysics(hierarchy);
        (void)hierarchyPhysics.Advance(1.0f / 60.0f);
        Require(std::abs(hierarchy.Transform(child).Local.Translation.x) < 1.0e-5f,
            "Physics publication corrupted a child entity's parent-relative X coordinate.");

        bool settingsRejected = false;
        try
        {
            kairo::player::RuntimePhysicsBridge invalid(hierarchy,
                { .FixedDeltaSeconds = 0.0f });
        }
        catch (const std::invalid_argument&) { settingsRejected = true; }
        Require(settingsRejected, "Invalid runtime fixed delta was accepted.");

        kairo::engine::Scene incomplete;
        const auto bodyOnly = incomplete.CreateEntity("Body without collider");
        incomplete.SetRigidBody(bodyOnly, {});
        bool incompleteRejected = false;
        try { kairo::player::RuntimePhysicsBridge invalid(incomplete); }
        catch (const std::invalid_argument&) { incompleteRejected = true; }
        Require(incompleteRejected, "Runtime body without an authored collider was accepted.");
    }
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "kairo-player-runtime-tests";
    std::filesystem::remove_all(root);
    try
    {
        Write(root / "Project.kproject",
            "kairo-project 2\nname \"Runtime Test\"\nengine-version \"0.1.0\"\n"
            "assets \"Assets.kassets\"\nstartup-scene \"Scenes/Main.kscene\"\n"
            "input-map \"Config/Input.kinput\"\nrendering-profile \"desktop\"\n"
            "build-profile \"Development\" development \"Build/Development\"\n");
        const auto logicID = kairo::assets::AssetID::Parse(
            "00000000-0000-4000-8000-000000000901");
        Write(root / "Assets.kassets",
            "kairo-assets 1\n"
            "asset 00000000-0000-4000-8000-000000000901 document source 1 "
            "\"Logic/Runtime.kdoc\" \"kairo.document-v1\"\nend\n");
        Write(root / "Logic/Runtime.kdoc", "runtime logic source\n");
        Write(root / "Config/Input.kinput",
            "kairo-input 1\naction \"Jump\" button\nbind \"Jump\" key Space 1 0 0\n");
        Write(root / "Scenes/Main.kscene",
            "kairo-scene 2\nentity 1 \"Root\"\nenabled true\nlayer 0\n"
            "transform 0 0 0 0 0 0 1 1 1 1\n"
            "logic 00000000-0000-4000-8000-000000000901 true\nend\n");

        kairo::engine::LogicProgram logicProgram;
        logicProgram.Vectors = { { 2.0, 3.0, 4.0 } };
        logicProgram.Entities = { { 1u } };
        logicProgram.RegisterCount = 5u;
        logicProgram.Instructions = {
            { kairo::engine::LogicOpcode::LoadEntity, 3u, 0u },
            { kairo::engine::LogicOpcode::LoadVector3, 4u, 0u },
            { kairo::engine::LogicOpcode::SetEntityPosition, 3u, 4u },
            { kairo::engine::LogicOpcode::Halt }
        };
        logicProgram.Entries = { { kairo::engine::LogicEventKind::BeginPlay, {}, 0u } };
        kairo::engine::SaveCompiledLogicArtifact(
            kairo::engine::CompiledLogicPath(root, logicID),
            { logicID, kairo::assets::FingerprintFile(root / "Logic/Runtime.kdoc"), logicProgram });

        kairo::player::RuntimeProject project(root / "Project.kproject");
        Require(project.Descriptor().Name == "Runtime Test", "Project descriptor did not load.");
        Require(project.Assets().Size() == 1u, "Document asset manifest did not load.");
        Require(project.Scene().Size() == 1u, "Startup scene did not load.");
        Require(project.InputMap().FindAction("Jump") != nullptr,
            "Authored input action map did not load.");
        kairo::player::RuntimePhysicsBridge projectPhysics(project.Scene());
        kairo::player::RuntimeLogicBridge projectLogic(project, projectPhysics);
        Require(projectLogic.InstanceCount() == 1u, "Attached runtime logic instance did not load.");
        projectLogic.BeginPlay();
        Require(project.Scene().Transform({ 1u }).Local.Translation ==
            kairo::foundation::math::Vec3f{ 2.0f, 3.0f, 4.0f },
            "Begin Play bytecode did not mutate the runtime scene through its host.");

        Write(root / "Logic/Runtime.kdoc", "changed runtime logic source\n");
        bool staleRejected = false;
        try
        {
            kairo::player::RuntimeProject staleProject(root / "Project.kproject");
            kairo::player::RuntimePhysicsBridge stalePhysics(staleProject.Scene());
            kairo::player::RuntimeLogicBridge staleLogic(staleProject, stalePhysics);
        }
        catch (const std::invalid_argument&) { staleRejected = true; }
        Require(staleRejected, "Runtime accepted a compiled artifact for stale source bytes.");

        std::filesystem::remove(root / "Scenes/Main.kscene");
        bool missingRejected = false;
        try { kairo::player::RuntimeProject invalid(root / "Project.kproject"); }
        catch (const std::invalid_argument&) { missingRejected = true; }
        Require(missingRejected, "Missing startup scene was accepted.");

        Write(root / "Scenes/Main.kscene",
            "kairo-scene 2\nentity 1 \"Root\"\nenabled true\nlayer 0\n"
            "transform 0 0 0 0 0 0 1 1 1 1\nend\n");
        std::filesystem::remove(root / "Config/Input.kinput");
        bool missingInputRejected = false;
        try { kairo::player::RuntimeProject invalid(root / "Project.kproject"); }
        catch (const std::invalid_argument&) { missingInputRejected = true; }
        Require(missingInputRejected, "Missing project input map was accepted.");

        bool extensionRejected = false;
        Write(root / "Project.txt", "not a project\n");
        try { (void)kairo::player::ResolveProjectDescriptorPath(root / "Project.txt"); }
        catch (const std::invalid_argument&) { extensionRejected = true; }
        Require(extensionRejected, "Non-kproject descriptor was accepted.");
        TestRuntimePhysics();
        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << "KairoPlayer runtime test: " << error.what() << '\n';
        return 1;
    }
}
