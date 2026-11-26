#include "RuntimeSkeletonPose.hpp"
#include <iostream>
#include <random>
using namespace kairo::player::detail;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static double Error(RetargetVector a,RetargetVector b){return ik_detail::Length(ik_detail::Sub(a,b));}
int main()
{
    const double q=std::sqrt(.5);
    const std::vector<RetargetJoint> rig{{"root",-1,{}},{"elbow",0,{}},{"tip",1,{}}};
    const auto world=EvaluateSkeletonPose(rig,{{{2,3,0},{0,0,q,q}},{{1,0,0},{q,0,0,q}},{{0,1,0},{0,0,0,1}}});
    Check(Error(world[1].Translation,{2,4,0})<1e-12&&Error(world[2].Translation,{2,4,1})<1e-12,"noncommuting FK");
    std::mt19937 rng(92);std::uniform_real_distribution<double> u(-3.,3.);
    for(int i=0;i<20000;++i)
    {
        const RetargetVector root{u(rng),u(rng),u(rng)},target{u(rng),u(rng),u(rng)},pole{u(rng),u(rng),u(rng)};
        std::array<RetargetTransform,3> input{{{root,{q,0,0,q}},{{1,.3,.2},{0,q,0,q}},{{.1,1.5,.2},{0,0,q,q}}}};
        const auto expected=SolveTwoBoneIK(root,target,pole,ik_detail::Length(input[1].Translation),ik_detail::Length(input[2].Translation));
        const auto solved=SolveLocalTwoBonePose(input,target,pole);
        const auto fk=EvaluateSkeletonPose(rig,{solved.Locals.begin(),solved.Locals.end()});
        Check(Error(fk[1].Translation,expected.Joint)<1e-9&&Error(fk[2].Translation,expected.Tip)<1e-9,"local rotations do not reconstruct solved points");
        Check(solved.Reached==expected.Reached&&solved.Locals[1].Translation==input[1].Translation&&solved.Locals[2].Translation==input[2].Translation,"bone length/reach changed");
        if(expected.Reached)Check(Error(fk[2].Translation,target)<1e-9,"reachable target missed");
    }
    const std::array<RetargetTransform,3> straight{{{},{{1,0,0},{}},{{1,0,0},{}}}};
    auto valid=straight;valid[1].Rotation={0,0,0,1};valid[2].Rotation={0,0,0,1};
    for(const auto target:{RetargetVector{-2,0,0},RetargetVector{0,0,0},RetargetVector{3,0,0}})
    {
        const auto solved=SolveLocalTwoBonePose(valid,target,{0,1,0});
        const auto fk=EvaluateSkeletonPose(rig,{solved.Locals.begin(),solved.Locals.end()});
        const auto oracle=SolveTwoBoneIK({},target,{0,1,0},1,1);
        Check(Error(fk[2].Translation,oracle.Tip)<1e-9,"folded/antipodal target");
    }
    bool rejected=false;try{(void)SolveLocalTwoBonePose(straight,{1,0,0},{0,1,0});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"zero quaternion accepted");
    std::cout<<"20000 3D local IK reconstructions, FK composition, folded and antipodal cases passed\n";
}
