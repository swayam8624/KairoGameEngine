module;

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

export module Kairo.Runtime.RenderBridge.OfflineSession;

import Kairo.EngineCore;
import Kairo.Foundation.RayTracer;
import Kairo.Runtime.RenderBridge.SceneSnapshot;

export namespace kairo::runtime::renderbridge
{
    namespace ray = kairo::foundation::raytracer;

    struct OfflineRenderOutput final
    {
        std::filesystem::path ImagePath;
        std::filesystem::path MetadataPath;
        std::uint32_t Width = 0u;
        std::uint32_t Height = 0u;
        std::uint32_t Passes = 0u;
    };

    class OfflineRenderSession final
    {
    public:
        OfflineRenderSession(const kairo::engine::Scene& scene,
            OfflineSceneAssetResolver resolver,
            OfflineSceneSettings settings = {})
            : m_Settings(settings)
            , m_Conversion(ConvertSceneSnapshot(scene, resolver, settings))
        {
        }

        [[nodiscard]] const SceneConversionResult& Conversion() const noexcept
        {
            return m_Conversion;
        }

        [[nodiscard]] bool Supported() const noexcept { return m_Conversion.Supported(); }

        void Start(std::uint32_t passes)
        {
            if (!Supported())
                throw std::logic_error("Offline render cannot start while scene conversion has error diagnostics.");
            if (passes == 0u)
                throw std::invalid_argument("Offline render passes must be non-zero.");
            if (m_Job)
                throw std::logic_error("Offline render session can only start one job.");
            m_Passes = passes;
            m_Job = std::make_unique<ray::OfflineRenderJob>(std::move(m_Conversion.Snapshot), passes);
            m_Job->Start();
        }

        void Cancel() noexcept
        {
            if (m_Job) m_Job->Cancel();
        }

        void Wait()
        {
            if (m_Job) m_Job->Wait();
        }

        [[nodiscard]] ray::OfflineRenderProgress Progress() const
        {
            if (!m_Job)
            {
                ray::OfflineRenderProgress progress;
                progress.State = ray::OfflineRenderJobState::Idle;
                return progress;
            }
            return m_Job->Progress();
        }

        [[nodiscard]] std::optional<ray::Film> Snapshot() const
        {
            return m_Job ? m_Job->Snapshot() : std::nullopt;
        }

        [[nodiscard]] OfflineRenderOutput SaveResult(const std::filesystem::path& projectRoot,
            const std::filesystem::path& relativeImagePath) const
        {
            if (!m_Job) throw std::logic_error("Offline render has not been started.");
            const auto progress = m_Job->Progress();
            if (progress.State != ray::OfflineRenderJobState::Completed)
                throw std::logic_error("Only a completed offline render can be saved as a project result.");
            const auto image = m_Job->Snapshot();
            if (!image.has_value()) throw std::logic_error("Completed offline render has no film snapshot.");
            if (projectRoot.empty() || relativeImagePath.empty() || relativeImagePath.is_absolute())
                throw std::invalid_argument("Render output must be a project-relative path.");

            const std::filesystem::path normalizedRoot = std::filesystem::weakly_canonical(projectRoot);
            const std::filesystem::path imagePath = normalizedRoot / relativeImagePath.lexically_normal();
            const std::filesystem::path normalizedImageParent = std::filesystem::weakly_canonical(
                imagePath.has_parent_path() ? imagePath.parent_path() : normalizedRoot);
            const auto relativeParent = normalizedImageParent.lexically_relative(normalizedRoot);
            if (relativeParent.empty() && normalizedImageParent != normalizedRoot)
                throw std::invalid_argument("Render output escapes the project root.");
            for (const auto& part : relativeParent)
                if (part == "..") throw std::invalid_argument("Render output escapes the project root.");

            ray::SavePPM(*image, imagePath);
            const std::filesystem::path metadataPath = imagePath.string() + ".kairo-render";
            std::ofstream metadata(metadataPath, std::ios::binary | std::ios::trunc);
            if (!metadata) throw std::runtime_error("Failed to write offline render metadata.");
            metadata << "kairo.render.output.v1\n"
                     << "width=" << image->Width() << "\n"
                     << "height=" << image->Height() << "\n"
                     << "passes=" << m_Passes << "\n"
                     << "mode=" << ray::ToString(m_Settings.Mode) << "\n"
                     << "acceleration=" << ray::ToString(m_Settings.Acceleration) << "\n";
            if (!metadata) throw std::runtime_error("Failed while writing offline render metadata.");
            return { imagePath, metadataPath, image->Width(), image->Height(), m_Passes };
        }

    private:
        OfflineSceneSettings m_Settings;
        SceneConversionResult m_Conversion;
        std::unique_ptr<ray::OfflineRenderJob> m_Job;
        std::uint32_t m_Passes = 0u;
    };
}
