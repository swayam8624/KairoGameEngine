module;

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module Kairo.Sample.Phase1GameState;

import Kairo.EngineCore.Entity;
import Kairo.Foundation.Math.Vector;

export namespace kairo::sample
{
    enum class Phase1Mode : std::uint8_t
    {
        Menu,
        Running,
        Paused,
        Won,
        Lost
    };

    struct Phase1Snapshot final
    {
        Phase1Mode Mode = Phase1Mode::Menu;
        kairo::foundation::math::Vec3f PlayerPosition{};
        std::vector<kairo::engine::Entity> Collected;
        friend bool operator==(const Phase1Snapshot&, const Phase1Snapshot&) = default;
    };

    [[nodiscard]] inline std::string_view Phase1ModeName(Phase1Mode mode)
    {
        switch (mode)
        {
            case Phase1Mode::Menu: return "menu";
            case Phase1Mode::Running: return "running";
            case Phase1Mode::Paused: return "paused";
            case Phase1Mode::Won: return "won";
            case Phase1Mode::Lost: return "lost";
        }
        throw std::invalid_argument("Unknown Phase 1 game mode.");
    }

    [[nodiscard]] inline Phase1Mode ParsePhase1Mode(std::string_view value)
    {
        if (value == "menu") return Phase1Mode::Menu;
        if (value == "running") return Phase1Mode::Running;
        if (value == "paused") return Phase1Mode::Paused;
        if (value == "won") return Phase1Mode::Won;
        if (value == "lost") return Phase1Mode::Lost;
        throw std::invalid_argument("Unknown Phase 1 save mode.");
    }

    /// Backend-neutral state machine for the first complete Kairo gameplay slice.
    /// It owns no renderer, window, filesystem, or physics handles, so menu,
    /// pause, collectible, win/loss, and save-state behavior remain deterministic
    /// and directly testable.
    class Phase1GameState final
    {
    public:
        explicit Phase1GameState(std::vector<kairo::engine::Entity> collectibles)
            : m_Collectibles(std::move(collectibles))
        {
            if (m_Collectibles.empty())
                throw std::invalid_argument("Phase 1 game requires at least one collectible.");
            std::ranges::sort(m_Collectibles, {}, &kairo::engine::Entity::Value);
            if (std::ranges::any_of(m_Collectibles, [](kairo::engine::Entity entity) { return !entity; }))
                throw std::invalid_argument("Phase 1 collectible IDs cannot be zero.");
            if (std::adjacent_find(m_Collectibles.begin(), m_Collectibles.end()) != m_Collectibles.end())
                throw std::invalid_argument("Phase 1 collectible IDs must be unique.");
        }

        [[nodiscard]] Phase1Mode Mode() const noexcept { return m_Mode; }
        [[nodiscard]] bool IsRunning() const noexcept { return m_Mode == Phase1Mode::Running; }
        [[nodiscard]] std::size_t CollectedCount() const noexcept { return m_Collected.size(); }
        [[nodiscard]] std::size_t CollectibleCount() const noexcept { return m_Collectibles.size(); }
        [[nodiscard]] const std::vector<kairo::engine::Entity>& CollectedEntities() const noexcept
        {
            return m_Collected;
        }

        void Start()
        {
            if (m_Mode == Phase1Mode::Menu || m_Mode == Phase1Mode::Paused)
                m_Mode = Phase1Mode::Running;
        }

        void TogglePause()
        {
            if (m_Mode == Phase1Mode::Running) m_Mode = Phase1Mode::Paused;
            else if (m_Mode == Phase1Mode::Paused) m_Mode = Phase1Mode::Running;
        }

        void Reset() noexcept
        {
            m_Mode = Phase1Mode::Menu;
            m_Collected.clear();
        }

        [[nodiscard]] bool Collect(kairo::engine::Entity entity)
        {
            if (m_Mode != Phase1Mode::Running || !IsCollectible(entity)) return false;
            const auto insertion = std::ranges::lower_bound(m_Collected, entity.Value, {},
                &kairo::engine::Entity::Value);
            if (insertion != m_Collected.end() && *insertion == entity) return false;
            m_Collected.insert(insertion, entity);
            return true;
        }

        [[nodiscard]] bool ReachGoal()
        {
            if (m_Mode != Phase1Mode::Running || m_Collected.size() != m_Collectibles.size())
                return false;
            m_Mode = Phase1Mode::Won;
            return true;
        }

        void Lose() noexcept
        {
            if (m_Mode == Phase1Mode::Running) m_Mode = Phase1Mode::Lost;
        }

        [[nodiscard]] bool IsCollected(kairo::engine::Entity entity) const noexcept
        {
            return std::ranges::binary_search(m_Collected, entity.Value, {},
                &kairo::engine::Entity::Value);
        }

        [[nodiscard]] Phase1Snapshot Capture(
            const kairo::foundation::math::Vec3f& playerPosition) const
        {
            ValidatePosition(playerPosition);
            return { m_Mode, playerPosition, m_Collected };
        }

        void Restore(const Phase1Snapshot& snapshot)
        {
            ValidatePosition(snapshot.PlayerPosition);
            std::vector<kairo::engine::Entity> collected = snapshot.Collected;
            std::ranges::sort(collected, {}, &kairo::engine::Entity::Value);
            if (std::adjacent_find(collected.begin(), collected.end()) != collected.end())
                throw std::invalid_argument("Phase 1 save contains duplicate collectibles.");
            for (const auto entity : collected)
                if (!IsCollectible(entity))
                    throw std::invalid_argument("Phase 1 save references an unknown collectible.");
            if (snapshot.Mode == Phase1Mode::Won && collected.size() != m_Collectibles.size())
                throw std::invalid_argument("A won Phase 1 save must contain every collectible.");
            m_Mode = snapshot.Mode;
            m_Collected = std::move(collected);
        }

        [[nodiscard]] std::string StatusLine() const
        {
            const std::string score = std::to_string(m_Collected.size()) + "/" +
                std::to_string(m_Collectibles.size());
            switch (m_Mode)
            {
                case Phase1Mode::Menu: return "Press Enter to start | Collect " + score;
                case Phase1Mode::Running: return "Collect " + score + " | Reach the goal after collecting all";
                case Phase1Mode::Paused: return "Paused | P to resume | Score " + score;
                case Phase1Mode::Won: return "You won | R to reset | Score " + score;
                case Phase1Mode::Lost: return "You lost | R to reset | Score " + score;
            }
            throw std::logic_error("Unknown Phase 1 game mode.");
        }

    private:
        Phase1Mode m_Mode = Phase1Mode::Menu;
        std::vector<kairo::engine::Entity> m_Collectibles;
        std::vector<kairo::engine::Entity> m_Collected;

        [[nodiscard]] bool IsCollectible(kairo::engine::Entity entity) const noexcept
        {
            return std::ranges::binary_search(m_Collectibles, entity.Value, {},
                &kairo::engine::Entity::Value);
        }

        static void ValidatePosition(const kairo::foundation::math::Vec3f& position)
        {
            if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
                !std::isfinite(position.z))
                throw std::invalid_argument("Phase 1 player position must be finite.");
        }
    };

    [[nodiscard]] inline std::string SerializePhase1Snapshot(const Phase1Snapshot& snapshot)
    {
        if (!std::isfinite(snapshot.PlayerPosition.x) || !std::isfinite(snapshot.PlayerPosition.y) ||
            !std::isfinite(snapshot.PlayerPosition.z))
            throw std::invalid_argument("Cannot serialize a non-finite Phase 1 player position.");
        std::vector<kairo::engine::Entity> collected = snapshot.Collected;
        std::ranges::sort(collected, {}, &kairo::engine::Entity::Value);
        if (std::adjacent_find(collected.begin(), collected.end()) != collected.end())
            throw std::invalid_argument("Cannot serialize duplicate Phase 1 collectibles.");
        std::ostringstream output;
        output.precision(std::numeric_limits<float>::max_digits10);
        output << "kairo-phase1-save 1\n";
        output << "mode " << Phase1ModeName(snapshot.Mode) << '\n';
        output << "player " << snapshot.PlayerPosition.x << ' ' << snapshot.PlayerPosition.y
            << ' ' << snapshot.PlayerPosition.z << '\n';
        output << "collected " << collected.size() << '\n';
        for (const auto entity : collected) output << "entity " << entity.Value << '\n';
        return output.str();
    }

    [[nodiscard]] inline Phase1Snapshot ParsePhase1Snapshot(std::string_view source)
    {
        std::istringstream input{ std::string(source) };
        std::string key;
        std::uint32_t version = 0u;
        if (!(input >> key >> version) || key != "kairo-phase1-save" || version != 1u)
            throw std::invalid_argument("Phase 1 save has an invalid header.");
        std::string mode;
        if (!(input >> key >> mode) || key != "mode")
            throw std::invalid_argument("Phase 1 save is missing its mode.");
        Phase1Snapshot result;
        result.Mode = ParsePhase1Mode(mode);
        if (!(input >> key >> result.PlayerPosition.x >> result.PlayerPosition.y >> result.PlayerPosition.z)
            || key != "player")
            throw std::invalid_argument("Phase 1 save is missing its player position.");
        if (!std::isfinite(result.PlayerPosition.x) || !std::isfinite(result.PlayerPosition.y) ||
            !std::isfinite(result.PlayerPosition.z))
            throw std::invalid_argument("Phase 1 save player position must be finite.");
        std::size_t count = 0u;
        if (!(input >> key >> count) || key != "collected" || count > 1'000'000u)
            throw std::invalid_argument("Phase 1 save has an invalid collectible count.");
        result.Collected.reserve(count);
        for (std::size_t index = 0u; index < count; ++index)
        {
            std::uint32_t value = 0u;
            if (!(input >> key >> value) || key != "entity" || value == 0u)
                throw std::invalid_argument("Phase 1 save has an invalid collectible record.");
            result.Collected.push_back({ value });
        }
        if (input >> key)
            throw std::invalid_argument("Phase 1 save contains trailing data.");
        return result;
    }
}
