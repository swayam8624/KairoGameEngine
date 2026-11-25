#include "../../Runtime/KairoPlayer/RuntimeAnimationRetarget.hpp"
#include <iostream>
using namespace kairo::player::detail;
// Sample a tiny authored source clip at five times, retarget each local pose,
// and report the target limb translation and quaternion. Not a GPU demo.
int main()
{
    const std::vector<RetargetJoint> source{{"root",-1,{}},{"arm",0,{{1,0,0},{0,0,0,1}}}};
    const std::vector<RetargetJoint> target{{"root",-1,{}},{"arm",0,{{2,0,0},{0,0,0,1}}}};
    RuntimeAnimationRetarget retarget(source,target,2);
    for(unsigned frame=0;frame<=4;++frame)
    {
        const double t=frame*.25,angle=t*1.5707963267948966;
        const auto pose=retarget.Apply({{{t,0,0},{0,0,0,1}},{{1,0,0},{0,0,std::sin(angle*.5),std::cos(angle*.5)}}});
        std::cout<<"t="<<t<<" root x="<<pose[0].Translation[0]<<" arm length="<<pose[1].Translation[0]
            <<" arm z,w="<<pose[1].Rotation[2]<<','<<pose[1].Rotation[3]<<'\n';
    }
}
