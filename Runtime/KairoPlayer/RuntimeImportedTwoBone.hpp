#pragma once
#include "RuntimeSkeletonPose.hpp"
#include <cstdint>

namespace kairo::player::detail
{
    // Pure adapter shared by scene extraction and the Player. Target and
    // pole are in the root node's parent space, not world/mesh space. Node
    // indices are source glTF indices, which need not be parent-first.
    inline void ValidateImportedTwoBoneRequest(const std::array<std::uint32_t,3>& nodes,
        RetargetVector target,RetargetVector pole)
    {
        if(nodes[0]==nodes[1]||nodes[0]==nodes[2]||nodes[1]==nodes[2])
            throw std::invalid_argument("Two-bone IK requires three distinct nodes.");
        ik_detail::Validate(target);ik_detail::Validate(pole);
    }
    template<class Source,class Pose,class MatrixFromTRS>
    [[nodiscard]] auto ApplyImportedTwoBone(const Source& source,const Pose& pose,
        const std::array<std::uint32_t,3>& nodes,RetargetVector target,RetargetVector pole,
        MatrixFromTRS toMatrix)
    {
        ValidateImportedTwoBoneRequest(nodes,target,pole);
        if(pose.Nodes.size()!=source.Nodes.size())throw std::invalid_argument("IK pose/source size mismatch.");
        std::array<RetargetTransform,3> locals;
        for(unsigned i=0;i<3;++i)
        {
            const auto id=nodes[i];
            if(id>=source.Nodes.size())throw std::out_of_range("IK source node out of range.");
            const auto& authored=source.Nodes[id];const auto& node=pose.Nodes[id];
            if(i&&(authored.Parent<0||static_cast<std::uint32_t>(authored.Parent)!=nodes[i-1]))
                throw std::invalid_argument("IK nodes must form a direct parent-child chain.");
            const auto& t=node.LocalTRS;
            // Nonuniform/animated scale alters bone lengths and axes. The
            // rigid solver explicitly rejects all non-unit chain scale.
            if(!authored.HasRestTRS||!node.UsesTRS||t.Scale.x!=1.f||t.Scale.y!=1.f||t.Scale.z!=1.f)
                throw std::invalid_argument("IK requires unit-scale TRS chain nodes.");
            locals[i]={{t.Translation.x,t.Translation.y,t.Translation.z},{t.Rotation.x,t.Rotation.y,t.Rotation.z,t.Rotation.w}};
        }
        bool skinFound=false;
        for(const auto& skin:source.Skins)
        {
            const auto contains=[&](auto id){return std::find(skin.Joints.begin(),skin.Joints.end(),id)!=skin.Joints.end();};
            if(contains(nodes[0])&&contains(nodes[1])){skinFound=true;break;}
        }
        if(!skinFound)throw std::invalid_argument("IK root and middle must belong to the same skin.");
        const auto solved=SolveLocalTwoBonePose(locals,target,pole);
        // Copy-on-success keeps the shared rest/clip pose immutable. Refresh
        // LocalMatrix from changed TRS: the engine hierarchy reads matrices,
        // not the quaternion fields directly. Tip and unrelated nodes stay exact.
        auto result=pose;
        for(unsigned i=0;i<2;++i)
        {
            auto& node=result.Nodes[nodes[i]];const auto& q=solved.Locals[i].Rotation;
            node.LocalTRS.Rotation={static_cast<float>(q[0]),static_cast<float>(q[1]),static_cast<float>(q[2]),static_cast<float>(q[3])};
            node.LocalMatrix=toMatrix(node.LocalTRS);
        }
        return std::pair{std::move(result),solved.Reached};
    }
}
