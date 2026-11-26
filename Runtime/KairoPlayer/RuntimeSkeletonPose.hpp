#pragma once
#include "RuntimeAnimationRetarget.hpp"
#include "RuntimeTwoBoneIK.hpp"

namespace kairo::player::detail
{
    namespace pose_detail
    {
        // Quaternion sandwich rotates a vector without applying translation.
        inline RetargetVector Rotate(RetargetQuaternion q,RetargetVector v)
        {
            using namespace retarget_detail;
            q=Unit(q);const auto p=Multiply(Multiply(q,{v[0],v[1],v[2],0}),Inverse(q));
            return {p[0],p[1],p[2]};
        }
        inline RetargetVector Cross(RetargetVector a,RetargetVector b)
        {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
        // Use atan2 of cross magnitude and dot to preserve near-antipodal
        // precision. Exactly opposite vectors have no unique swing axis;
        // the same deterministic perpendicular policy as the IK solver wins.
        inline RetargetQuaternion Swing(RetargetVector from,RetargetVector to)
        {
            using namespace ik_detail;
            const auto a=Length(from),b=Length(to);
            if(a<1e-12||b<1e-12)throw std::invalid_argument("Cannot align zero bone.");
            from=Scale(from,1./a);to=Scale(to,1./b);
            const auto cross=Cross(from,to);const double sine=Length(cross),cosine=std::clamp(Dot(from,to),-1.,1.);
            if(sine<1e-14)
            {
                if(cosine>0)return {0,0,0,1};
                const auto axis=Perpendicular(from);return {axis[0],axis[1],axis[2],0};
            }
            const double half=std::atan2(sine,cosine)*.5;
            const auto axis=Scale(cross,std::sin(half)/sine);
            return {axis[0],axis[1],axis[2],std::cos(half)};
        }
    }
    // Parent-first forward kinematics: parent translation + rotated local
    // offset; parent rotation * local rotation. Rest and pose translations
    // are local, output transforms all share the skeleton's parent space.
    // Scale/skinning/inverse-bind matrices are intentionally outside this API.
    [[nodiscard]] inline std::vector<RetargetTransform> EvaluateSkeletonPose(
        std::vector<RetargetJoint> skeleton,const std::vector<RetargetTransform>& locals)
    {
        using namespace retarget_detail;
        (void)Index(skeleton);
        if(locals.size()!=skeleton.size())throw std::invalid_argument("Skeleton pose size mismatch.");
        std::vector<RetargetTransform> world;world.reserve(locals.size());
        for(std::size_t i=0;i<locals.size();++i)
        {
            auto joint=locals[i];ValidateTranslation(joint.Translation);joint.Rotation=Unit(joint.Rotation);
            if(i)
            {
                const auto& parent=world[static_cast<std::size_t>(skeleton[i].Parent)];
                joint.Translation=ik_detail::Add(parent.Translation,pose_detail::Rotate(parent.Rotation,joint.Translation));
                joint.Rotation=Unit(Multiply(parent.Rotation,joint.Rotation));ValidateTranslation(joint.Translation);
            }
            world.push_back(joint);
        }
        return world;
    }
    struct LocalTwoBonePose
    {
        std::array<RetargetTransform,3> Locals;
        bool Reached;
    };
    // Solve a root->joint->tip chain in the root's parent coordinate space.
    // Bone lengths come from authored local offsets, which remain unchanged.
    // Swing the original rotations toward the solved segment directions;
    // convert the second world-space swing back through the new root. Twist
    // is not independently constrained, and the tip local rotation is kept.
    [[nodiscard]] inline LocalTwoBonePose SolveLocalTwoBonePose(std::array<RetargetTransform,3> locals,
        RetargetVector target,RetargetVector pole)
    {
        using namespace retarget_detail;using namespace ik_detail;using namespace pose_detail;
        for(auto& joint:locals){ValidateTranslation(joint.Translation);joint.Rotation=Unit(joint.Rotation);}
        const auto root=locals[0].Translation;
        const auto solved=SolveTwoBoneIK(root,target,pole,Length(locals[1].Translation),Length(locals[2].Translation));
        const auto upperSwing=Swing(Rotate(locals[0].Rotation,locals[1].Translation),Sub(solved.Joint,root));
        locals[0].Rotation=Unit(Multiply(upperSwing,locals[0].Rotation));
        const auto lowerWorld=Unit(Multiply(locals[0].Rotation,locals[1].Rotation));
        const auto lowerSwing=Swing(Rotate(lowerWorld,locals[2].Translation),Sub(solved.Tip,solved.Joint));
        locals[1].Rotation=Unit(Multiply(Inverse(locals[0].Rotation),Multiply(lowerSwing,lowerWorld)));
        return {locals,solved.Reached};
    }
}
