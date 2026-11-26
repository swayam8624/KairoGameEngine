// End-to-end acceptance: original GLB on disk -> real asset importer ->
// shared scene/clip/IK extraction -> native skinned draw -> GPU capture/pick.
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

import Kairo.Assets;
import Kairo.EngineCore;
import Kairo.Foundation.Math;
import Kairo.Renderer;
import Kairo.Runtime.RealtimeSceneBridge;

namespace
{
    using namespace kairo::renderer;
    using namespace kairo::runtime::renderbridge;

    ViewportCapture Capture(RendererRuntime& renderer)
    {
        renderer.RequestViewportCapture();
        for(unsigned i=0;i<16;++i)
        {
            renderer.DrawFrame();
            if(auto image=renderer.TakeViewportCapture())
            {
                if(image->Width!=160||image->Height!=160||!image->IsVisuallyNonUniform())
                    throw std::runtime_error("Imported skin capture unavailable or uniform.");
                return *image;
            }
        }
        throw std::runtime_error("Imported skin capture deadline.");
    }

    std::uint32_t Pick(RendererRuntime& renderer,unsigned x,unsigned y)
    {
        renderer.RequestViewportPick(x,y);
        for(unsigned i=0;i<16;++i)
        {
            renderer.DrawFrame();
            if(auto id=renderer.TakeViewportPickResult())return *id;
        }
        throw std::runtime_error("Imported skin pick unavailable.");
    }

    // A skin matrix maps bind-space vertices; check the tip's original
    // x=2 against the requested asset-space target, not matrix translation alone.
    void CheckTip(const RenderScene& scene,float x,float y)
    {
        if(scene.Draws().size()!=1||scene.Draws()[0].Skinning.Size()!=3)
            throw std::runtime_error("Imported skin palette size mismatch.");
        const auto& m=scene.Draws()[0].Skinning.JointMatrices[2];
        if(std::abs(2.f*m(0u,0u)+m(0u,3u)-x)>1e-5f||
           std::abs(2.f*m(1u,0u)+m(1u,3u)-y)>1e-5f||
           std::abs(2.f*m(2u,0u)+m(2u,3u))>1e-5f)
            throw std::runtime_error("Imported skin tip mismatch.");
    }
}

int main(int argc,char** argv)
{
    try
    {
        if(argc!=3)throw std::invalid_argument("Use KairoImportedIKSmoke FIXTURE_ROOT metal|vulkan.");
        const auto root=std::filesystem::canonical(argv[1]);
        const std::string_view name=argv[2];
        if(name!="metal"&&name!="vulkan")throw std::invalid_argument("Expected native backend.");
        const auto backend=name=="metal"?GraphicsBackend::Metal:GraphicsBackend::Vulkan;
        const auto asset=kairo::assets::AssetID::Parse("00000000-0000-4000-8000-000000000601");
        kairo::assets::AssetRegistry registry;
        registry.Insert({asset,kairo::assets::AssetType::Scene,kairo::assets::AssetOrigin::SourceFile,
            "Character.glb","kairo.gltf.scene",1u,{}});
        kairo::assets::ImportDatabase imports;
        kairo::assets::DerivedDataCache cache(root/".kairo"/"derived-data");
        auto imported=ImportRenderGltfSceneWithSource(root,{asset},registry,imports,cache);
        if(imported.Source.Nodes.size()!=4||imported.Source.Skins.size()!=1||imported.Source.Animations.size()!=1||
            imported.Source.Skins[0].Joints!=std::vector<std::uint32_t>{0u,1u,2u}||
            imported.Source.Animations[0].Name!="RootMove"||imported.RenderAsset.Primitives.size()!=1)
            throw std::runtime_error("Actual GLB import lost fixture metadata.");
        const auto& primitive=imported.Source.Primitives.at(0);
        if(primitive.Skinning.size()!=6||primitive.Skinning[4].Joints[0]!=2u)
            throw std::runtime_error("Actual GLB import lost vertex influences.");
        RendererRuntime renderer({"Imported IK acceptance",160u,160u,false,backend});
        if(renderer.Backend()!=backend)throw std::runtime_error("Unexpected graphics backend.");
        renderer.ResizeViewport(160,160);
        CameraPose camera;camera.Position={0.f,0.f,5.f};camera.Target={};
        camera.Projection=CameraProjectionMode::Orthographic;camera.OrthographicSize=4.f;
        renderer.SetCameraPose(camera);renderer.SetViewportShadingMode(ViewportShadingMode::Unlit);
        const auto mesh=renderer.CreateMesh(imported.RenderAsset.Primitives[0].Geometry);
        RenderAssetBindings bindings(registry);
        const std::array<MeshHandle,1> handles{mesh};
        bindings.BindGltfScene({asset},std::move(imported.Source),imported.RenderAsset,handles);
        kairo::engine::Scene world;
        const auto entity=world.CreateEntity("Imported ribbon");
        world.SetSceneInstance(entity,{{asset},true,false,false,0x4u});
        world.Transform(entity).Local.Translation={-1.25f,0.f,0.f};
        SceneAnimationOverrides animation;
        animation.Set(entity,{0u,.5f,kairo::engine::AnimationTimeMode::Clamp});
        const auto baseline=BuildRenderScene(world,bindings,animation,0x4u);
        CheckTip(baseline,2.25f,0.f);
        renderer.SubmitRenderScene(baseline);
        const auto horizontal=Capture(renderer);
        if(Pick(renderer,80,80)!=entity.Value)throw std::runtime_error("Horizontal skin not pickable at expected interior.");

        // Same imported asset, same animation time and same mesh handle.
        // Only the post-clip IK override changes the submitted palette.
        animation.SetTwoBoneIK(entity,asset,{{0u,1u,2u},{.25,2.,0.},{0.,0.,1.}});
        const auto corrected=BuildRenderScene(world,bindings,animation,0x4u);
        CheckTip(corrected,.25f,2.f);
        if(corrected.Draws()[0].Mesh!=mesh||corrected.Draws()[0].Model!=baseline.Draws()[0].Model)
            throw std::runtime_error("IK changed geometry ownership or outer transform.");
        renderer.SubmitRenderScene(corrected);
        const auto vertical=Capture(renderer);
        if(horizontal.RGBA==vertical.RGBA||Pick(renderer,80,80)==entity.Value)
            throw std::runtime_error("Native skin did not leave original horizontal interior.");
        // The middle of the vertical strip is at x=-1,y=+1. Accept either
        // viewport Y convention, while requiring the correct object ID.
        if(Pick(renderer,40,40)!=entity.Value&&Pick(renderer,40,120)!=entity.Value)
            throw std::runtime_error("Native corrected skin not pickable at target interior.");

        animation.ClearTwoBoneIK(entity);
        renderer.SubmitRenderScene(BuildRenderScene(world,bindings,animation,0x4u));
        if(Capture(renderer).RGBA!=horizontal.RGBA||Pick(renderer,80,80)!=entity.Value)
            throw std::runtime_error("Removing IK failed to restore the imported clip.");
        renderer.SubmitRenderScene(RenderScene{});renderer.DestroyMesh(mesh);
        std::cout<<"Actual GLB import, clip sampling, IK palette, native skinned pixels and picking passed\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
