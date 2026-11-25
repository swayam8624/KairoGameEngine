#pragma once
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

namespace kairo::player::detail
{
    // Pose contract: local translations and XYZW unit quaternions. Joint
    // names are stable authored IDs. Parent indices precede children (-1 root).
    // This first retarget path assumes compatible local joint axes and no
    // animated scale. It does not perform automatic humanoid correspondence.
    using RetargetVector=std::array<double,3>;
    using RetargetQuaternion=std::array<double,4>;
    struct RetargetTransform{RetargetVector Translation{};RetargetQuaternion Rotation{0,0,0,1};};
    struct RetargetJoint{std::string Name;int Parent=-1;RetargetTransform Rest;};
    namespace retarget_detail
    {
        // Normalize at the API boundary; reject zero/nonfinite quaternions.
        inline RetargetQuaternion Unit(RetargetQuaternion q)
        {
            const double n=std::hypot(std::hypot(q[0],q[1]),std::hypot(q[2],q[3]));
            if(!std::isfinite(n)||n<1e-12||n>1e12)throw std::invalid_argument("Invalid retarget rotation.");
            for(auto& x:q)x/=n;
            return q;
        }
        // Hamilton product: a*b applies b first, then a to a column vector.
        inline RetargetQuaternion Multiply(RetargetQuaternion a,RetargetQuaternion b)
        {
            return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
                a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
                a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
                a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
        }
        // Conjugation equals inverse for a normalized quaternion.
        inline RetargetQuaternion Inverse(RetargetQuaternion q){return {-q[0],-q[1],-q[2],q[3]};}
        inline void ValidateTranslation(RetargetVector v)
        {
            for(auto x:v)if(!std::isfinite(x)||std::abs(x)>1e9)throw std::invalid_argument("Invalid retarget translation.");
        }
        // Validate bounded, parent-first, single-root hierarchy and unique names.
        inline std::map<std::string,std::size_t> Index(std::vector<RetargetJoint>& skeleton)
        {
            if(skeleton.empty()||skeleton.size()>4096)throw std::invalid_argument("Invalid skeleton size.");
            std::map<std::string,std::size_t> index;
            for(std::size_t i=0;i<skeleton.size();++i)
            {
                auto& joint=skeleton[i];
                if(joint.Name.empty()||joint.Name.size()>128||!index.emplace(joint.Name,i).second||
                    (i==0?joint.Parent!=-1:joint.Parent<0||static_cast<std::size_t>(joint.Parent)>=i))
                    throw std::invalid_argument("Invalid skeleton hierarchy or name.");
                ValidateTranslation(joint.Rest.Translation);joint.Rest.Rotation=Unit(joint.Rest.Rotation);
            }
            return index;
        }
    }
    class RuntimeAnimationRetarget
    {
    public:
        // Own rest poses and a precomputed target->source mapping. Matching
        // names must have matching parent names; extra target joints remain at
        // rest. Root name must match so root motion cannot map accidentally.
        RuntimeAnimationRetarget(std::vector<RetargetJoint> source,std::vector<RetargetJoint> target,
            double rootMotionScale=1.) : m_Source(std::move(source)),m_Target(std::move(target)),m_RootScale(rootMotionScale)
        {
            using namespace retarget_detail;
            if(!std::isfinite(m_RootScale)||m_RootScale<=0||m_RootScale>1000)throw std::invalid_argument("Invalid root motion scale.");
            const auto sourceIndex=Index(m_Source);(void)Index(m_Target);
            if(m_Source[0].Name!=m_Target[0].Name)throw std::invalid_argument("Root name mismatch.");
            m_Map.assign(m_Target.size(),-1);
            for(std::size_t i=0;i<m_Target.size();++i)
            {
                const auto found=sourceIndex.find(m_Target[i].Name);
                if(found==sourceIndex.end())continue;
                const auto j=found->second;
                if(i&&(!j||m_Source[static_cast<std::size_t>(m_Source[j].Parent)].Name!=
                    m_Target[static_cast<std::size_t>(m_Target[i].Parent)].Name))
                    throw std::invalid_argument("Matched joint parent mismatch.");
                m_Map[i]=static_cast<int>(j);
            }
        }
        // Input is one sampled source local pose. Output is a new target pose;
        // no source/rest mutation. Apply source rest-relative rotation delta
        // after target rest rotation. Preserve target bone translations, and
        // scale only the root's translation displacement. This preserves limb
        // proportions for compatible rigs, not differently oriented joint axes.
        [[nodiscard]] std::vector<RetargetTransform> Apply(const std::vector<RetargetTransform>& pose) const
        {
            using namespace retarget_detail;
            if(pose.size()!=m_Source.size())throw std::invalid_argument("Source pose size mismatch.");
            for(const auto& joint:pose){ValidateTranslation(joint.Translation);(void)Unit(joint.Rotation);}
            std::vector<RetargetTransform> result;result.reserve(m_Target.size());
            for(std::size_t i=0;i<m_Target.size();++i)
            {
                auto joint=m_Target[i].Rest;
                if(m_Map[i]>=0)
                {
                    const auto j=static_cast<std::size_t>(m_Map[i]);
                    const auto delta=Multiply(Inverse(m_Source[j].Rest.Rotation),Unit(pose[j].Rotation));
                    joint.Rotation=Unit(Multiply(joint.Rotation,delta));
                    if(i==0)for(unsigned k=0;k<3;++k)
                        joint.Translation[k]+=(pose[j].Translation[k]-m_Source[j].Rest.Translation[k])*m_RootScale;
                    ValidateTranslation(joint.Translation);
                }
                result.push_back(joint);
            }
            return result;
        }
    private:
        std::vector<RetargetJoint> m_Source,m_Target;
        std::vector<int> m_Map;
        double m_RootScale;
    };
}
