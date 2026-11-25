#include "../RuntimeTwoBoneIK.hpp"
#include <iostream>
#include <random>
#include <limits>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static double Distance(IKPoint a,IKPoint b){return ik_detail::Length(ik_detail::Sub(a,b));}
int main()
{
    // Independent invariant: the two segments retain their original length,
    // and the solved tip is on the closest reachable point to the target.
    std::mt19937 rng(2718);
    std::uniform_real_distribution<double> coordinate(-10,10),length(.1,5);
    for(unsigned i=0;i<10000;++i)
    {
        IKPoint root{coordinate(rng),coordinate(rng),coordinate(rng)};
        IKPoint target{coordinate(rng),coordinate(rng),coordinate(rng)};
        IKPoint pole{coordinate(rng),coordinate(rng),coordinate(rng)};
        const double a=length(rng),b=length(rng),d=Distance(root,target);
        const auto result=SolveTwoBoneIK(root,target,pole,a,b);
        Check(std::abs(Distance(root,result.Joint)-a)<1e-9,"Upper length changed");
        Check(std::abs(Distance(result.Joint,result.Tip)-b)<1e-9,"Lower length changed");
        Check(std::abs(Distance(root,result.Tip)-std::clamp(d,std::abs(a-b),a+b))<1e-9,"Shell error");
        if(result.Reached)Check(Distance(result.Tip,target)<1e-9,"Target missed");
    }
    const auto folded=SolveTwoBoneIK({0,0,0},{0,0,0},{0,0,1},2,2);
    Check(folded.Reached&&Distance(folded.Joint,{0,0,2})<1e-12,"Fold failed");
    const auto inside=SolveTwoBoneIK({0,0,0},{0,0,0},{0,0,0},3,1);
    Check(!inside.Reached&&Distance(inside.Tip,{2,0,0})<1e-12,"Inner shell failed");
    const auto collinear=SolveTwoBoneIK({0,0,0},{1,0,0},{2,0,0},1,1);
    Check(std::abs(Distance(collinear.Joint,{0,0,0})-1)<1e-12,"Pole fallback failed");
    bool rejected=false;try{(void)SolveTwoBoneIK({0,0,0},{0,0,0},{0,0,0},0,1);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Zero length accepted");
    rejected=false;try{(void)SolveTwoBoneIK({0,0,0},{std::numeric_limits<double>::infinity(),0,0},{0,0,0},1,1);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Infinite target accepted");
    std::cout<<"10000 seeded IK cases and singularity/validation fixtures passed\n";
}
