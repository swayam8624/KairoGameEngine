module;

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module Kairo.Runtime.RenderBridge.EditorOfflineService;

import Kairo.Editor.OfflineRenderAuthoring;
import Kairo.EngineCore;
import Kairo.Foundation.RayTracer;
import Kairo.Runtime.RenderBridge.SceneSnapshot;
import Kairo.Runtime.RenderBridge.OfflineSession;

export namespace kairo::runtime::renderbridge
{
    class EditorOfflineRenderService final : public kairo::editor::OfflineRenderService
    {
    public:
        using SceneProvider = std::function<const kairo::engine::Scene&()>;

        EditorOfflineRenderService(SceneProvider sceneProvider,
            OfflineSceneAssetResolver assetResolver)
            : m_SceneProvider(std::move(sceneProvider)),
              m_AssetResolver(std::move(assetResolver))
        {
            if (!m_SceneProvider)
                throw std::invalid_argument("Editor offline render service requires a scene provider.");
        }

        void Submit(kairo::editor::OfflineRenderRequest request) override
        {
            request.Validate();
            if (m_Jobs.contains(request.JobID))
                throw std::invalid_argument("Offline render job ID is already in use.");

            OfflineSceneSettings settings;
            settings.Width = request.Width;
            settings.Height = request.Height;
            Job job;
            job.Request = std::move(request);
            job.Session = std::make_unique<OfflineRenderSession>(
                m_SceneProvider(), m_AssetResolver, settings);
            for (const auto& diagnostic : job.Session->Conversion().Diagnostics)
            {
                std::string message = diagnostic.Code + ": " + diagnostic.Message;
                job.Diagnostics.push_back(std::move(message));
            }
            if (!job.Session->Supported())
            {
                job.PreflightFailed = true;
                m_Jobs.emplace(job.Request.JobID, std::move(job));
                return;
            }
            job.Session->Start(job.Request.Passes);
            m_Jobs.emplace(job.Request.JobID, std::move(job));
        }

        void Cancel(std::uint64_t jobID) override
        {
            Job& job = Require(jobID);
            if (job.Session) job.Session->Cancel();
            job.CancelRequested = true;
        }

        [[nodiscard]] kairo::editor::OfflineRenderServiceProgress Poll(
            std::uint64_t jobID) override
        {
            Job& job = Require(jobID);
            kairo::editor::OfflineRenderServiceProgress result;
            result.JobID = jobID;
            result.TotalPasses = job.Request.Passes;
            result.Diagnostics = job.Diagnostics;

            if (job.PreflightFailed)
            {
                result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Failed;
                return result;
            }
            if (!job.Session)
            {
                result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Failed;
                result.Diagnostics.push_back("offline-render-session-missing: internal render session is unavailable");
                return result;
            }

            const auto progress = job.Session->Progress();
            result.CompletedPasses = progress.CompletedPasses;
            using RayState = kairo::foundation::raytracer::OfflineRenderJobState;
            switch (progress.State)
            {
                case RayState::Idle:
                    result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Queued;
                    break;
                case RayState::Running:
                    result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Running;
                    break;
                case RayState::Completed:
                    result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Completed;
                    break;
                case RayState::Cancelled:
                    result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Cancelled;
                    break;
                case RayState::Failed:
                    result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Failed;
                    if (!progress.Error.empty()) result.Diagnostics.push_back(progress.Error);
                    break;
            }

            if (job.CancelRequested && result.Status == kairo::editor::OfflineRenderWorkspaceStatus::Running)
                result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Cancelled;

            if (result.Status == kairo::editor::OfflineRenderWorkspaceStatus::Completed)
            {
                if (!job.Output.has_value() && !job.SaveFailed)
                {
                    try
                    {
                        job.Output = job.Session->SaveResult(
                            job.Request.ProjectRoot,
                            job.Request.RelativeOutput).ImagePath;
                    }
                    catch (const std::exception& error)
                    {
                        job.SaveFailed = true;
                        job.Diagnostics.push_back(std::string("offline-render-save-failed: ") + error.what());
                    }
                }
                if (job.SaveFailed)
                {
                    result.Status = kairo::editor::OfflineRenderWorkspaceStatus::Failed;
                    result.Diagnostics = job.Diagnostics;
                }
                else result.Output = job.Output;
            }
            return result;
        }

        void Forget(std::uint64_t jobID)
        {
            Job& job = Require(jobID);
            const auto progress = Poll(jobID);
            if (progress.Status == kairo::editor::OfflineRenderWorkspaceStatus::Running ||
                progress.Status == kairo::editor::OfflineRenderWorkspaceStatus::Queued)
                throw std::logic_error("Cannot forget an active offline render job.");
            (void)job;
            m_Jobs.erase(jobID);
        }

    private:
        struct Job final
        {
            kairo::editor::OfflineRenderRequest Request;
            std::unique_ptr<OfflineRenderSession> Session;
            std::vector<std::string> Diagnostics;
            std::optional<std::filesystem::path> Output;
            bool PreflightFailed = false;
            bool CancelRequested = false;
            bool SaveFailed = false;
        };

        SceneProvider m_SceneProvider;
        OfflineSceneAssetResolver m_AssetResolver;
        std::map<std::uint64_t, Job> m_Jobs;

        [[nodiscard]] Job& Require(std::uint64_t jobID)
        {
            const auto found = m_Jobs.find(jobID);
            if (found == m_Jobs.end()) throw std::out_of_range("Offline render job ID was not found.");
            return found->second;
        }
    };
}
