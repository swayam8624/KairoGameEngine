#pragma once
// Include from the native host after the Renderer, Math and TextureArtifact
// imports. All GPU handles are owned by the one RendererRuntime instance.
#include "World2DAtlas.hpp"
#include "World2DAtlasCapture.hpp"
#include <iostream>
#include <cstddef>
#include <memory>

namespace kairo::player::world2d
{
    class NativeAtlas
    {
        using Handle=kairo::renderer::MeshHandle;
        using Texture=kairo::renderer::TextureHandle;
        kairo::renderer::RendererRuntime& m_Renderer;
        Atlas m_Atlas;
        std::vector<TileChunk> m_Chunks;
        std::vector<Handle> m_ChunkMeshes;
        std::map<std::string,std::vector<Handle>> m_FrameMeshes;
        Texture m_Texture=kairo::renderer::InvalidTextureHandle;
        AtlasFrame m_Frame;
        std::vector<Handle> m_Owned;

        Handle Upload(const std::vector<AtlasQuad>& quads,bool palette)
        {
            using namespace kairo::renderer;
            std::vector<MeshVertex> vertices;
            std::vector<std::uint32_t> indices;
            vertices.reserve(quads.size()*4);indices.reserve(quads.size()*6);
            for(const auto& quad:quads)
            {
                const auto r=quad.Region;
                const auto b=quad.Bounds;
                const float l=float(b.X),t=-float(b.Y),right=float(b.X+b.W),bottom=-float(b.Y+b.H);
                const float u0=(float(r.X)+(palette?.5f:0.f))/float(m_Atlas.Width);
                const float v0=(float(r.Y)+(palette?.5f:0.f))/float(m_Atlas.Height);
                const float u1=palette?u0:float(r.X+r.W)/float(m_Atlas.Width);
                const float v1=palette?v0:float(r.Y+r.H)/float(m_Atlas.Height);
                const auto i=static_cast<std::uint32_t>(vertices.size());
                vertices.push_back({{l,t,0},{1,1,1},{0,0,1},{u0,v0}});
                vertices.push_back({{l,bottom,0},{1,1,1},{0,0,1},{u0,v1}});
                vertices.push_back({{right,bottom,0},{1,1,1},{0,0,1},{u1,v1}});
                vertices.push_back({{right,t,0},{1,1,1},{0,0,1},{u1,v0}});
                indices.insert(indices.end(),{i,i+1,i+2,i,i+2,i+3});
            }
            const auto handle=m_Renderer.CreateMesh(Mesh(std::move(vertices),std::move(indices)));
            m_Owned.push_back(handle);
            return handle;
        }

    public:
        explicit NativeAtlas(kairo::renderer::RendererRuntime& renderer):m_Renderer(renderer){}
        NativeAtlas(const NativeAtlas&)=delete;
        NativeAtlas& operator=(const NativeAtlas&)=delete;

        void Load(const World& world)
        {
            using namespace kairo::renderer;
            // Prepare CPU data first. Retire only after the next atlas is valid.
            auto next=BuildAtlas(world);
            auto chunks=BuildTileChunks(world,next);
            m_Renderer.SubmitRenderScene(RenderScene{});
            for(const auto mesh:m_Owned)m_Renderer.DestroyMesh(mesh);
            m_Owned.clear();m_ChunkMeshes.clear();m_FrameMeshes.clear();
            if(m_Texture!=InvalidTextureHandle)m_Renderer.DestroyTexture(m_Texture);
            m_Texture=InvalidTextureHandle;
            m_Atlas=std::move(next);m_Chunks=std::move(chunks);
            m_Owned.reserve(m_Chunks.size()+MaxSprites*16);
            m_ChunkMeshes.reserve(m_Chunks.size());
            kairo::assets::TextureArtifactData texture;
            texture.Format=kairo::assets::TexturePixelFormat::RGBA8;
            // Indexed colors were already interpreted as linear RGB by the
            // rectangle renderer. Preserve that authored capture convention.
            texture.ColorSpace=kairo::assets::TextureColorSpace::Linear;
            texture.Semantic=kairo::assets::TextureSemantic::Color;
            kairo::assets::TextureMipLevel mip{m_Atlas.Width,m_Atlas.Height,{}};
            mip.Pixels.reserve(m_Atlas.RGBA.size());
            for(const auto byte:m_Atlas.RGBA)mip.Pixels.push_back(static_cast<std::byte>(byte));
            texture.Mips.push_back(std::move(mip));
            m_Texture=m_Renderer.CreateTexture(texture,{TextureAddressMode::ClampToEdge,TextureAddressMode::ClampToEdge,true});
            // Chunks are static indexed batches. Frame quads are allocated
            // lazily, at most one handle per distinct authored animation frame.
            for(const auto& chunk:m_Chunks)m_ChunkMeshes.push_back(Upload(chunk.Quads,true));
            for(const auto& [name,frames]:m_Atlas.Frames)
                m_FrameMeshes.emplace(name,std::vector<Handle>(frames.size(),InvalidMeshHandle));
            m_Renderer.SetViewportShadingMode(ViewportShadingMode::Unlit);
        }

        void Publish(const Game& game,unsigned width,unsigned height)
        {
            using namespace kairo::renderer;
            using namespace kairo::foundation::math;
            BuildAtlasFrame(game,m_Chunks,m_Frame);
            const auto c=m_Frame.Camera;
            CameraPose camera;
            camera.Position={float(c.X)+float(c.W)*.5f,-float(c.Y)-float(c.H)*.5f,64};
            camera.Target={camera.Position.x,camera.Position.y,0};
            camera.Projection=CameraProjectionMode::Orthographic;
            camera.OrthographicSize=std::max(float(c.H),float(c.W)*float(height)/float(width));
            camera.FarPlane=256;
            m_Renderer.SetCameraPose(camera);
            RenderScene scene;
            const auto rgb=game.Definition().Colors[0];
            RenderEnvironment environment;
            environment.BackgroundColor={float((rgb>>16)&255)/255.f,float((rgb>>8)&255)/255.f,float(rgb&255)/255.f};
            scene.SetEnvironment(environment);
            PBRMaterial material;
            material.BaseColorTexture=m_Texture;
            material.DoubleSided=true;
            const auto add=[&](Handle mesh,const Mat4f& model,bool masked)
            {
                MeshDraw draw;
                draw.Mesh=mesh;draw.Model=model;draw.Material=material;
                draw.Material.AlphaMode=masked?MaterialAlphaMode::Mask:MaterialAlphaMode::Opaque;
                draw.CastShadows=draw.ReceiveShadows=false;
                scene.Add(draw);
            };
            for(const auto index:m_Frame.Chunks)
            {
                const auto& chunk=m_Chunks[index];
                const float dx=chunk.Background?float(c.X-m_Frame.BackgroundCamera.X):0.f;
                const float dy=chunk.Background?-float(c.Y-m_Frame.BackgroundCamera.Y):0.f;
                add(m_ChunkMeshes[index],MakeTranslation(Vec3f{dx,dy,chunk.Background?-1.f:0.f}),false);
            }
            for(std::size_t order=0;order<m_Frame.Sprites.size();++order)
            {
                const auto& sprite=m_Frame.Sprites[order];
                const auto& name=game.Definition().Actors[sprite.ActorIndex].SpriteName;
                auto& handle=m_FrameMeshes.at(name).at(sprite.Frame);
                if(handle==InvalidMeshHandle)
                    handle=Upload({{{0,0,1,1},m_Atlas.Frames.at(name).at(sprite.Frame)}},false);
                const auto b=sprite.Bounds;
                const auto model=MakeTranslation(Vec3f{float(b.X),-float(b.Y),1.f+float(order)/32.f})*
                    MakeScale(Vec3f{float(b.W),float(b.H),1.f});
                add(handle,model,true);
            }
            m_Renderer.SubmitRenderScene(scene);
        }
    };

    inline void VerifyAtlasCapture(const Game& game,const kairo::renderer::ViewportCapture& capture,kairo::renderer::GraphicsBackend backend)
    {
        const auto evidence=CheckAtlasCapture(game,capture.Width,capture.Height,capture.RGBA,
            backend==kairo::renderer::GraphicsBackend::Vulkan?CaptureTransfer::Gamma22:CaptureTransfer::Linear);
        if(evidence.Probes<8||evidence.Matches*100u<95u*evidence.Probes)
            throw std::runtime_error("Native atlas capture differs from rectangle reference: "+
                std::to_string(evidence.Matches)+"/"+std::to_string(evidence.Probes));
        std::cout<<"Native atlas interior probes: "<<evidence.Matches<<"/"<<evidence.Probes<<'\n';
    }

    // Preserve the old letterbox and objective HUD without rasterizing the
    // atlas sprites back into CPU rectangles. The bars mask geometry outside
    // the authored camera, which the aspect-preserving projection may reveal.
    inline void BuildAtlasHud(const Game& game,unsigned width,unsigned height,std::vector<DrawRect>& out)
    {
        out.clear();
        if(!width||!height)return;
        const Box c=game.Camera();
        const double scale=std::min(double(width)/c.W,double(height)/c.H);
        const int sw=int(c.W*scale),sh=int(c.H*scale),w=int(width),h=int(height);
        const int x=(w-sw)/2,y=(h-sh)/2;
        const auto draw=[&](Box b,unsigned rgb)
        {if(b.W>0&&b.H>0)out.push_back({b,rgb});};
        constexpr unsigned border=0x090e13;
        draw({0,0,w,y},border);draw({0,y+sh,w,h-y-sh},border);
        draw({0,y,x,sh},border);draw({x+sw,y,w-x-sw,sh},border);
        const auto hud=[&](Box b,unsigned rgb)
        {
            const int l=x+b.X*sw/c.W,t=y+b.Y*sh/c.H;
            const int r=x+std::min(b.X+b.W,c.W)*sw/c.W,d=y+std::min(b.Y+b.H,c.H)*sh/c.H;
            draw({l,t,r-l,d-t},rgb);
        };
        const auto& colors=game.Definition().Colors;
        hud({4,4,48,6},colors[8]);
        const int filled=game.Total()?int(48u*game.Collected()/game.Total()):48;
        if(filled)hud({4,4,filled,6},colors[4]);
    }
}
