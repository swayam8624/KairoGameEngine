#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

import Kairo.EngineCore.Entity;
import Kairo.Foundation.Math.Vector;
import Kairo.Sample.Phase1GameState;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    template<class Function>
    void RequireThrows(Function&& function, const char* message)
    {
        try { function(); }
        catch (const std::exception&) { return; }
        throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        using kairo::engine::Entity;
        using kairo::sample::Phase1GameState;
        using kairo::sample::Phase1Mode;

        Phase1GameState game({ Entity{ 3u }, Entity{ 4u }, Entity{ 5u } });
        Require(game.Mode() == Phase1Mode::Menu, "Game must begin in menu mode.");
        game.Start();
        Require(game.IsRunning(), "Start must enter running mode.");
        Require(!game.ReachGoal(), "Goal must remain locked before collection.");
        Require(game.Collect({ 4u }), "Known collectible must be accepted.");
        Require(!game.Collect({ 4u }), "Duplicate collectible must be ignored.");
        Require(!game.Collect({ 99u }), "Unknown collectible must be ignored.");
        Require(game.Collect({ 3u }) && game.Collect({ 5u }),
            "Every remaining collectible must be accepted.");
        Require(game.ReachGoal(), "Goal must win after all collectibles.");
        Require(game.Mode() == Phase1Mode::Won, "Winning must update game mode.");

        const auto snapshot = game.Capture({ 1.25f, 2.5f, -3.75f });
        const auto encoded = kairo::sample::SerializePhase1Snapshot(snapshot);
        const auto decoded = kairo::sample::ParsePhase1Snapshot(encoded);
        Require(decoded.Mode == snapshot.Mode, "Save mode must round-trip.");
        Require(decoded.PlayerPosition == snapshot.PlayerPosition,
            "Player position must round-trip.");
        Require(decoded.Collected == snapshot.Collected,
            "Collected entities must round-trip.");

        Phase1GameState restored({ Entity{ 3u }, Entity{ 4u }, Entity{ 5u } });
        restored.Restore(decoded);
        Require(restored.Mode() == Phase1Mode::Won && restored.CollectedCount() == 3u,
            "Restore must reproduce completed state.");

        restored.Reset();
        restored.Start();
        restored.TogglePause();
        Require(restored.Mode() == Phase1Mode::Paused, "Pause must stop running state.");
        restored.TogglePause();
        Require(restored.IsRunning(), "Pause toggle must resume.");
        restored.Lose();
        Require(restored.Mode() == Phase1Mode::Lost, "Lose must enter lost state.");

        auto invalid = decoded;
        invalid.Collected = { Entity{ 99u } };
        invalid.Mode = Phase1Mode::Running;
        RequireThrows([&] { restored.Restore(invalid); },
            "Restore must reject unknown collectible IDs.");
        RequireThrows([&] { (void)kairo::sample::ParsePhase1Snapshot("broken"); },
            "Parser must reject malformed save data.");

        std::cout << "Kairo Phase 1 game-state tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Kairo Phase 1 game-state tests: " << error.what() << '\n';
        return 1;
    }
}
