#include <cmath>
#include <iostream>
#include <stdexcept>

import Kairo.Player.RuntimeLocomotionAnimationBridge;

namespace player = kairo::player;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void RequireNear(float actual, float expected, float tolerance,
        const char* message)
    {
        if (std::abs(actual - expected) > tolerance)
            throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        player::RuntimeLocomotionAnimationSettings settings;
        settings.MovingSpeedThreshold = 0.10f;
        settings.RunSpeedThreshold = 3.0f;
        settings.WalkReferenceSpeed = 2.0f;
        settings.RunReferenceSpeed = 4.0f;
        settings.MinimumPlaybackRate = 0.5f;
        settings.MaximumPlaybackRate = 1.5f;

        const auto idle = player::SelectLocomotionAnimation(
            settings, 0.05f, true);
        Require(idle.State == player::RuntimeLocomotionAnimationState::Idle,
            "Sub-threshold physical speed did not select idle animation.");
        RequireNear(idle.PlaybackRate, 1.0f, 1.0e-6f,
            "Idle animation playback rate should remain neutral.");

        const auto walk = player::SelectLocomotionAnimation(
            settings, 1.0f, true);
        Require(walk.State == player::RuntimeLocomotionAnimationState::Walk,
            "Walking physical speed did not select walk animation.");
        RequireNear(walk.PlaybackRate, 0.5f, 1.0e-6f,
            "Walk playback rate did not track physical speed/reference speed.");

        const auto fastWalk = player::SelectLocomotionAnimation(
            settings, 2.8f, true);
        Require(fastWalk.State == player::RuntimeLocomotionAnimationState::Walk,
            "Speed below run threshold incorrectly selected run.");
        RequireNear(fastWalk.PlaybackRate, 1.4f, 1.0e-6f,
            "Fast-walk playback scaling is incorrect.");

        const auto run = player::SelectLocomotionAnimation(
            settings, 4.0f, true);
        Require(run.State == player::RuntimeLocomotionAnimationState::Run,
            "Running physical speed did not select run animation.");
        RequireNear(run.PlaybackRate, 1.0f, 1.0e-6f,
            "Run reference speed should produce neutral playback rate.");

        const auto capped = player::SelectLocomotionAnimation(
            settings, 20.0f, true);
        Require(capped.State == player::RuntimeLocomotionAnimationState::Run,
            "High physical speed did not remain in run state.");
        RequireNear(capped.PlaybackRate, settings.MaximumPlaybackRate, 1.0e-6f,
            "Run playback rate exceeded authored upper bound.");

        const auto airborne = player::SelectLocomotionAnimation(
            settings, 0.0f, false);
        Require(airborne.State == player::RuntimeLocomotionAnimationState::Airborne,
            "Ungrounded character did not select airborne animation.");

        bool invalidSettingsRejected = false;
        try
        {
            auto invalid = settings;
            invalid.RunSpeedThreshold = invalid.MovingSpeedThreshold;
            (void)player::SelectLocomotionAnimation(invalid, 1.0f, true);
        }
        catch (const std::invalid_argument&)
        {
            invalidSettingsRejected = true;
        }
        Require(invalidSettingsRejected,
            "Invalid locomotion speed threshold ordering was accepted.");

        bool invalidSpeedRejected = false;
        try { (void)player::SelectLocomotionAnimation(settings, -1.0f, true); }
        catch (const std::invalid_argument&) { invalidSpeedRejected = true; }
        Require(invalidSpeedRejected,
            "Negative physical locomotion speed was accepted.");

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "KairoPlayer locomotion animation test: "
                  << error.what() << '\n';
        return 1;
    }
}
