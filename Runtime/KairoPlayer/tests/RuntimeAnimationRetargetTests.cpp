#include "../RuntimeAnimationRetarget.hpp"
#include <iostream>
#include <random>
#include <limits>
using namespace kairo::player::detail;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static RetargetQuaternion AxisZ(double angle){return {0,0,std::sin(angle*.5),std::cos(angle*.5)};}
static bool SameRotation(RetargetQuaternion a,RetargetQuaternion b)
{
    double dot=0;for(unsigned i=0;i<4;++i)dot+=a[i]*b[i];return std::abs(std::abs(dot)-1)<1e-12;
}
int main()
{
    // A short source arm and longer target arm, with different rest twists.
    // A correct retarget preserves the target's limb length and applies
    // the same relative angular motion, not the source absolute orientation.
    const std::vector<RetargetJoint> source{{"root",-1,{{1,0,0},AxisZ(.1)}},
        {"arm",0,{{1,0,0},AxisZ(.2)}}};
    const std::vector<RetargetJoint> target{{"root",-1,{{0,0,0},AxisZ(.3)}},
        {"arm",0,{{2,0,0},AxisZ(.4)}},{"extra",1,{{.5,0,0},AxisZ(.5)}}};
    RuntimeAnimationRetarget retarget(source,target,2);
    std::mt19937 rng(2019);std::uniform_real_distribution<double> angle(-3.14,3.14);
    for(unsigned i=0;i<5000;++i)
    {
        const double a=angle(rng),b=angle(rng);
        const auto pose=retarget.Apply({{{2,1,0},AxisZ(.1+a)},{{9,0,0},AxisZ(.2+b)}});
        Check(pose.size()==3&&pose[0].Translation==RetargetVector{2,2,0},"Root motion scale failed");
        Check(pose[1].Translation==target[1].Rest.Translation,"Target limb length changed");
        Check(SameRotation(pose[0].Rotation,AxisZ(.3+a)),"Root rotation error");
        Check(SameRotation(pose[1].Rotation,AxisZ(.4+b)),"Arm rotation error");
        Check(SameRotation(pose[2].Rotation,target[2].Rest.Rotation),"Extra target joint changed");
    }
    // Noncommuting X/Y rotations detect an incorrectly ordered quaternion product.
    const double q=std::sqrt(.5);
    RuntimeAnimationRetarget cross({{"root",-1,{{},{0,0,0,1}}}},{{"root",-1,{{},{q,0,0,q}}}});
    const auto p=cross.Apply({{{},{0,q,0,q}}});
    Check(SameRotation(p[0].Rotation,{.5,.5,.5,.5}),"Quaternion product order incorrect");
    auto bad=target;bad[2].Name="arm";bool rejected=false;
    try{RuntimeAnimationRetarget invalid(source,bad);}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Duplicate name accepted");
    rejected=false;try{(void)retarget.Apply({});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Wrong pose size accepted");
    rejected=false;try{(void)retarget.Apply({{{},{0,0,0,0}},{{},{0,0,0,1}}});}catch(const std::invalid_argument&){rejected=true;}
    Check(rejected,"Zero quaternion accepted");
    std::cout<<"5000 seeded retarget poses, noncommuting rotations and rejection tests passed\n";
}
