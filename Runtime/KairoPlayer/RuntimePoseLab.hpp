#pragma once
#include "RuntimeGridCanvas.hpp"
#include "RuntimeSkeletonPose.hpp"

namespace kairo::player::detail
{
    struct PoseLabFrame
    {
        std::array<std::vector<RetargetTransform>,3> Chains;
        RetargetVector Target;
        bool Reached;
    };
    // Sample a short source rig and retarget its local rotations onto a longer
    // rig, then solve a moving target on the long rig. All three are evaluated
    // by the same FK code; the renderer never fabricates joint locations.
    [[nodiscard]] inline PoseLabFrame SamplePoseLab(double phase)
    {
        if(!std::isfinite(phase))throw std::invalid_argument("Invalid pose lab phase.");
        const auto rotation=[](double angle){return RetargetQuaternion{0,0,std::sin(angle*.5),std::cos(angle*.5)};};
        const std::vector<RetargetJoint> source{{"root",-1,{}},{"elbow",0,{{1,0,0},{0,0,0,1}}},{"tip",1,{{.7,0,0},{0,0,0,1}}}};
        const std::vector<RetargetJoint> target{{"root",-1,{}},{"elbow",0,{{1.5,0,0},{0,0,0,1}}},{"tip",1,{{1.1,0,0},{0,0,0,1}}}};
        const std::vector<RetargetTransform> clip{{{},rotation(.7*std::sin(phase))},{{1,0,0},rotation(.9*std::cos(phase))},{{.7,0,0},{0,0,0,1}}};
        const RuntimeAnimationRetarget retarget(source,target);
        const auto mapped=retarget.Apply(clip);
        const RetargetVector goal{1.6+1.4*std::cos(phase),1.1*std::sin(phase),0};
        const auto solved=SolveLocalTwoBonePose({mapped[0],mapped[1],mapped[2]},goal,{0,1,0});
        return {{EvaluateSkeletonPose(source,clip),EvaluateSkeletonPose(target,mapped),
            EvaluateSkeletonPose(target,{solved.Locals.begin(),solved.Locals.end()})},goal,solved.Reached};
    }
    struct PoseLabRect{GridPixelRect Box;std::array<float,3> Color;};
    // 2D orthographic diagnostic of the actual 3D transforms. Draw each
    // bone with integer line sampling, then joint markers and the IK target.
    // Canvas coordinates stay in a bounded panel even on tiny viewports.
    [[nodiscard]] inline std::vector<PoseLabRect> PoseLabView(double phase,unsigned width,unsigned height)
    {
        const auto frame=SamplePoseLab(phase);RuntimeGridCanvas canvas(width,height,216,90);
        std::vector<PoseLabRect> out;
        const auto add=[&](int x,int y,int w,int h,std::array<float,3> color)
        {
            const auto box=canvas.Rect(x,y,w,h);if(box.W&&box.H)out.push_back({box,color});
        };
        add(0,0,216,90,{.035f,.05f,.08f});
        for(int panel=0;panel<3;++panel)
        {
            add(panel*72+1,14,70,62,{.08f,.12f,.17f});
            const auto project=[&](RetargetVector p)
            {return std::array<int,2>{panel*72+35+static_cast<int>(std::lround(p[0]*9)),45-static_cast<int>(std::lround(p[1]*9))};};
            const auto color=panel==0?std::array<float,3>{.4f,.7f,1}:panel==1?std::array<float,3>{.5f,.85f,.8f}:std::array<float,3>{1,.65f,.2f};
            const auto& joints=frame.Chains[static_cast<std::size_t>(panel)];
            for(std::size_t i=1;i<joints.size();++i)
            {
                const auto a=project(joints[i-1].Translation),b=project(joints[i].Translation);
                const int dx=b[0]-a[0],dy=b[1]-a[1],steps=std::max(std::abs(dx),std::abs(dy));
                for(int j=0;j<=steps;++j)
                {
                    const double t=steps?double(j)/steps:0.;
                    add(a[0]+static_cast<int>(std::lround(dx*t)),a[1]+static_cast<int>(std::lround(dy*t)),1,1,color);
                }
            }
            for(const auto& joint:joints){const auto p=project(joint.Translation);add(p[0]-1,p[1]-1,3,3,color);}
            if(panel==2)
            {
                const auto p=project(frame.Target);const auto marker=frame.Reached?std::array<float,3>{.4f,1,.4f}:std::array<float,3>{1,.2f,.2f};
                add(p[0]-3,p[1],7,1,marker);add(p[0],p[1]-3,1,7,marker);
            }
        }
        return out;
    }
}
