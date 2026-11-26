#include "RuntimeImportedTwoBone.hpp"
#include <iostream>
#include <limits>
#include <random>
using namespace kairo::player::detail;
// Structural fixture matches the imported node/pose fields used by the
// adapter; matrix generation is independent of KairoMath and the IK solver.
struct Vec{float x=0,y=0,z=0;friend bool operator==(Vec,Vec)=default;};
struct Quat{float x=0,y=0,z=0,w=1;friend bool operator==(Quat,Quat)=default;};
struct TRS{Vec Translation;Quat Rotation;Vec Scale{1,1,1};friend bool operator==(TRS,TRS)=default;};
using Mat=std::array<double,16>;
static Mat Matrix(const TRS& t)
{
    const double x=t.Rotation.x,y=t.Rotation.y,z=t.Rotation.z,w=t.Rotation.w;
    return {1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),t.Translation.x,
        2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),t.Translation.y,
        2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),t.Translation.z,0,0,0,1};
}
static Mat Mul(const Mat& a,const Mat& b)
{Mat c{};for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)c[4*i+j]+=a[4*i+k]*b[4*k+j];return c;}
static RetargetVector Position(const Mat& m){return {m[3],m[7],m[11]};}
struct Node{int Parent;bool HasRestTRS=true;};
struct Skin{std::vector<std::uint32_t> Joints;};
struct Source{std::vector<Node> Nodes;std::vector<Skin> Skins;};
struct PoseNode{bool UsesTRS=true;TRS LocalTRS;Mat LocalMatrix;friend bool operator==(const PoseNode&,const PoseNode&)=default;};
struct Pose{std::vector<PoseNode> Nodes;friend bool operator==(const Pose&,const Pose&)=default;};
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main()
{
    const Source source{{{2},{0},{-1},{-1}},{{{2,0,1}}}};
    constexpr std::array<std::uint32_t,3> chain{2,0,1};
    Pose pose;pose.Nodes.resize(4);pose.Nodes[0].LocalTRS.Translation={1,0,0};pose.Nodes[1].LocalTRS.Translation={.8f,.2f,0};
    pose.Nodes[3].UsesTRS=false;pose.Nodes[3].LocalMatrix[0]=27;
    std::mt19937 rng(625);std::uniform_real_distribution<float> u(-2.5f,2.5f);
    for(unsigned trial=0;trial<10000;++trial)
    {
        pose.Nodes[2].LocalTRS.Translation={u(rng),u(rng),u(rng)};
        for(auto id:chain)pose.Nodes[id].LocalMatrix=Matrix(pose.Nodes[id].LocalTRS);
        const auto before=pose;const auto root=Position(pose.Nodes[2].LocalMatrix);
        const RetargetVector target{u(rng),u(rng),u(rng)},pole{u(rng),u(rng),u(rng)};
        const auto expected=SolveTwoBoneIK(root,target,pole,1.,std::hypot(double(.8f),double(.2f)));
        const auto [solved,reached]=ApplyImportedTwoBone(source,pose,chain,target,pole,Matrix);
        const auto middle=Mul(solved.Nodes[2].LocalMatrix,solved.Nodes[0].LocalMatrix);
        const auto tip=Mul(middle,solved.Nodes[1].LocalMatrix);
        Check(ik_detail::Length(ik_detail::Sub(Position(middle),expected.Joint))<1e-6&&
            ik_detail::Length(ik_detail::Sub(Position(tip),expected.Tip))<1e-6&&reached==expected.Reached,"matrix/solver mismatch");
        Check(pose==before&&solved.Nodes[1]==before.Nodes[1]&&solved.Nodes[3]==before.Nodes[3],"source/tip/unrelated mutation");
        Check(solved.Nodes[0].LocalTRS.Translation==before.Nodes[0].LocalTRS.Translation,"bone offset changed");
    }
    const auto original=pose;
    const auto reject=[&](const Source& src,const Pose& p,std::array<std::uint32_t,3> ids,RetargetVector target){
        bool caught=false;try{(void)ApplyImportedTwoBone(src,p,ids,target,{0,1,0},Matrix);}catch(const std::exception&){caught=true;}
        Check(caught,"invalid imported chain accepted");};
    reject(source,pose,{2,2,1},{0,1,0});reject(source,pose,{2,1,0},{0,1,0});reject(source,pose,{2,0,4},{0,1,0});
    reject(source,pose,chain,{std::numeric_limits<double>::quiet_NaN(),0,0});
    auto bad=pose;bad.Nodes[0].LocalTRS.Scale.y=2;reject(source,bad,chain,{0,1,0});
    bad=pose;bad.Nodes[0].UsesTRS=false;reject(source,bad,chain,{0,1,0});
    bad=pose;bad.Nodes.pop_back();reject(source,bad,chain,{0,1,0});
    auto wrong=source;wrong.Skins[0].Joints={2};reject(wrong,pose,chain,{0,1,0});
    bool failed=false;try{(void)ApplyImportedTwoBone(source,pose,chain,{0,1,0},{0,0,1},[](const TRS&)->Mat{throw std::runtime_error("matrix failure");});}catch(const std::runtime_error&){failed=true;}
    Check(failed&&pose==original,"callback failure mutated source");
    std::cout<<"10000 imported-node rigid IK matrix reconstructions and rollback/validation cases passed\n";
}
