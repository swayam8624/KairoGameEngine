#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kairo::physics2d
{
    // Pixel-space coordinates, seconds, positive Y down. Translation-only
    // shapes deliberately exclude angular dynamics and arbitrary polygons.
    struct Vec
    {
        double X=0,Y=0;
        friend Vec operator+(Vec a,Vec b){return {a.X+b.X,a.Y+b.Y};}
        friend Vec operator-(Vec a,Vec b){return {a.X-b.X,a.Y-b.Y};}
        friend Vec operator*(Vec a,double s){return {a.X*s,a.Y*s};}
        Vec& operator+=(Vec b){return *this=*this+b;}
        Vec& operator-=(Vec b){return *this=*this-b;}
    };
    inline double Dot(Vec a,Vec b){return a.X*b.X+a.Y*b.Y;}
    inline double Length(Vec a){return std::hypot(a.X,a.Y);}
    inline bool Finite(Vec a){return std::isfinite(a.X)&&std::isfinite(a.Y);}
    inline void Require(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
    enum class Motion {Static,Kinematic,Dynamic};
    enum class Shape {Box,Circle};
    struct Body
    {
        Vec Position,Velocity,Half{2,2};
        Motion Type=Motion::Static;
        Shape Geometry=Shape::Box;
        double Mass=1,Restitution=0,Friction=.5;
        std::uint32_t Layer=1,Mask=0xffffffffu;
        bool Sensor=false;
        double InvMass() const{return Type==Motion::Dynamic?1/Mass:0;}
    };
    struct Joint {std::size_t A=0,B=0;double Length=1;};
    struct Contact {Vec Normal;double Depth=0;}; // normal from A toward B
    struct Event {std::size_t A=0,B=0;bool Begin=false;};
    inline constexpr std::size_t MaxBodies=1024,MaxJoints=64,MaxEvents=8192;
    inline constexpr unsigned Substeps=8,Iterations=4;
    inline constexpr double MaxSpeed=240,StepTime=1./60.;
    inline Vec Limit(Vec v){const double n=Length(v);return n>MaxSpeed?v*(MaxSpeed/n):v;}
    inline void Validate(const Body& b)
    {
        Require(Finite(b.Position)&&Finite(b.Velocity)&&Finite(b.Half)&&
            std::abs(b.Position.X)<=100000&&std::abs(b.Position.Y)<=100000&&Length(b.Velocity)<=MaxSpeed&&
            (b.Type!=Motion::Static||Length(b.Velocity)==0)&&b.Half.X>=1&&b.Half.Y>=1&&b.Half.X<=4096&&b.Half.Y<=4096&&
            (b.Type==Motion::Static||b.Type==Motion::Kinematic||b.Type==Motion::Dynamic)&&
            (b.Geometry==Shape::Box||(b.Geometry==Shape::Circle&&b.Half.X==b.Half.Y))&&
            std::isfinite(b.Mass)&&b.Mass>=.01&&b.Mass<=10000&&
            std::isfinite(b.Restitution)&&b.Restitution>=0&&b.Restitution<=1&&
            std::isfinite(b.Friction)&&b.Friction>=0&&b.Friction<=1&&b.Layer!=0,
            "Invalid 2D physics body.");
    }
    inline bool Filter(const Body& a,const Body& b){return (a.Layer&b.Mask)&&(b.Layer&a.Mask);}
    inline Contact CircleBox(const Body& circle,const Body& box)
    {
        const auto d=circle.Position-box.Position;
        const Vec closest{std::clamp(d.X,-box.Half.X,box.Half.X),std::clamp(d.Y,-box.Half.Y,box.Half.Y)};
        const Vec toward=closest-d;
        const double distance=Length(toward),radius=circle.Half.X;
        if(distance>0)return distance<radius?Contact{toward*(1/distance),radius-distance}:Contact{};
        // Center inside box: normal points inward so moving A opposite it
        // separates the circle through the nearest face.
        const double x=box.Half.X-std::abs(d.X),y=box.Half.Y-std::abs(d.Y);
        if(x<=y)return {{d.X>=0?-1.:1.,0},radius+x};
        return {{0,d.Y>=0?-1.:1.},radius+y};
    }
    inline Contact Collide(const Body& a,const Body& b)
    {
        const auto d=b.Position-a.Position;
        if(a.Geometry==Shape::Circle&&b.Geometry==Shape::Circle)
        {
            const auto n=Length(d),depth=a.Half.X+b.Half.X-n;
            return depth>0?Contact{n>1e-12?d*(1/n):Vec{1,0},depth}:Contact{};
        }
        if(a.Geometry==Shape::Circle)return CircleBox(a,b);
        if(b.Geometry==Shape::Circle){auto c=CircleBox(b,a);c.Normal=c.Normal*(-1);return c;}
        const double x=a.Half.X+b.Half.X-std::abs(d.X),y=a.Half.Y+b.Half.Y-std::abs(d.Y);
        if(x<=0||y<=0)return {};
        return x<=y?Contact{{d.X>=0?1.:-1.,0},x}:Contact{{0,d.Y>=0?1.:-1.},y};
    }
    class Scene
    {
        using Pair=std::pair<std::size_t,std::size_t>;
        std::vector<Body> m_Bodies;
        std::vector<Joint> m_Joints;
        std::vector<std::size_t> m_Order;
        std::vector<Pair> m_Pairs;
        std::set<Pair> m_Sensors;
        std::vector<Event> m_Events;
        Vec m_Gravity;
        void Pairs()
        {
            m_Pairs.clear();
            std::sort(m_Order.begin(),m_Order.end(),[&](auto i,auto j)
            {
                const auto& a=m_Bodies[i];const auto& b=m_Bodies[j];
                const auto x=a.Position.X-a.Half.X,y=b.Position.X-b.Half.X;
                return x==y?i<j:x<y;
            });
            for(std::size_t i=0;i<m_Order.size();++i)
            {
                const auto a=m_Order[i];const auto& x=m_Bodies[a];
                for(std::size_t j=i+1;j<m_Order.size();++j)
                {
                    const auto b=m_Order[j];const auto& y=m_Bodies[b];
                    if(y.Position.X-y.Half.X>=x.Position.X+x.Half.X)break;
                    if(std::abs(x.Position.Y-y.Position.Y)>=x.Half.Y+y.Half.Y||!Filter(x,y))continue;
                    if(x.InvMass()+y.InvMass()==0&&!x.Sensor&&!y.Sensor)continue;
                    m_Pairs.emplace_back(std::min(a,b),std::max(a,b));
                }
            }
            std::sort(m_Pairs.begin(),m_Pairs.end());
        }
        void Solve(Body& a,Body& b,Contact c)
        {
            const double ia=a.InvMass(),ib=b.InvMass(),sum=ia+ib;
            if(c.Depth<=0||sum==0)return;
            const Vec relative=b.Velocity-a.Velocity;
            const double vn=Dot(relative,c.Normal);
            if(vn<0)
            {
                const double restitution=vn<-1?std::min(a.Restitution,b.Restitution):0;
                const double magnitude=-(1+restitution)*vn/sum;
                const Vec normalImpulse=c.Normal*magnitude;
                a.Velocity-=normalImpulse*ia;b.Velocity+=normalImpulse*ib;
                const Vec tangent{-c.Normal.Y,c.Normal.X};
                const double friction=std::sqrt(a.Friction*b.Friction)*magnitude;
                const Vec impulse=tangent*std::clamp(-Dot(relative,tangent)/sum,-friction,friction);
                a.Velocity-=impulse*ia;b.Velocity+=impulse*ib;
            }
            const Vec correction=c.Normal*(std::max(c.Depth-.001,0.)*.8/sum);
            a.Position-=correction*ia;b.Position+=correction*ib;
        }
        void SolveJoint(const Joint& j)
        {
            auto& a=m_Bodies[j.A];auto& b=m_Bodies[j.B];
            const auto d=b.Position-a.Position;const double length=Length(d),sum=a.InvMass()+b.InvMass();
            if(sum==0)return;
            const auto n=length>1e-12?d*(1/length):Vec{1,0};
            const auto correction=n*(std::clamp(length-j.Length,-4.,4.)*.5/sum);
            a.Position+=correction*a.InvMass();b.Position-=correction*b.InvMass();
            const auto impulse=n*(Dot(b.Velocity-a.Velocity,n)/sum);
            a.Velocity+=impulse*a.InvMass();b.Velocity-=impulse*b.InvMass();
        }
        void Sensors()
        {
            std::set<Pair> current;
            for(const auto& p:m_Pairs)
            {
                const auto& a=m_Bodies[p.first];const auto& b=m_Bodies[p.second];
                if((a.Sensor||b.Sensor)&&Collide(a,b).Depth>0)current.insert(p);
            }
            const auto emit=[&](Pair p,bool begin)
            {
                if(m_Events.size()>=MaxEvents)throw std::length_error("2D sensor event budget exceeded.");
                m_Events.push_back({p.first,p.second,begin});
            };
            for(const auto& p:m_Sensors)if(!current.contains(p))emit(p,false);
            for(const auto& p:current)if(!m_Sensors.contains(p))emit(p,true);
            m_Sensors.swap(current);
        }
    public:
        Scene(std::vector<Body> bodies,std::vector<Joint> joints,Vec gravity)
            :m_Bodies(std::move(bodies)),m_Joints(std::move(joints)),m_Gravity(gravity)
        {
            Require(m_Bodies.size()<=MaxBodies&&m_Joints.size()<=MaxJoints&&Finite(gravity)&&Length(gravity)<=2000,"Invalid physics scene budget.");
            for(std::size_t i=0;i<m_Bodies.size();++i){Validate(m_Bodies[i]);m_Order.push_back(i);}
            std::set<Pair> unique;
            for(const auto& j:m_Joints)
                Require(j.A<m_Bodies.size()&&j.B<m_Bodies.size()&&j.A!=j.B&&std::isfinite(j.Length)&&j.Length>=2&&j.Length<=4096&&
                    m_Bodies[j.A].InvMass()+m_Bodies[j.B].InvMass()>0&&unique.emplace(std::min(j.A,j.B),std::max(j.A,j.B)).second,
                    "Invalid or duplicate distance joint.");
        }
        const std::vector<Body>& Bodies() const noexcept{return m_Bodies;}
        const std::vector<Event>& Events() const noexcept{return m_Events;}
        void SetKinematic(std::size_t i,Vec position)
        {
            auto& b=m_Bodies.at(i);Require(b.Type==Motion::Kinematic&&Finite(position)&&std::abs(position.X)<=100000&&std::abs(position.Y)<=100000,"Invalid kinematic position.");
            b.Position=position;
        }
        void Impulse(std::size_t i,Vec impulse)
        {
            auto& b=m_Bodies.at(i);Require(b.Type==Motion::Dynamic&&Finite(impulse)&&Length(impulse)<=1e6,"Invalid physics impulse.");
            b.Velocity=Limit(b.Velocity+impulse*b.InvMass());
        }
        void Step()
        {
            m_Events.clear();constexpr double dt=StepTime/Substeps;
            for(unsigned step=0;step<Substeps;++step)
            {
                for(auto& b:m_Bodies)
                {
                    if(b.Type==Motion::Dynamic)b.Velocity=Limit(b.Velocity+m_Gravity*dt);
                    if(b.Type!=Motion::Static)b.Position+=b.Velocity*dt;
                }
                Pairs();Sensors();
                for(unsigned iteration=0;iteration<Iterations;++iteration)
                {
                    for(const auto& joint:m_Joints)SolveJoint(joint);
                    Pairs();
                    for(const auto& [i,j]:m_Pairs)
                    {
                        auto& a=m_Bodies[i];auto& b=m_Bodies[j];
                        if(!a.Sensor&&!b.Sensor)Solve(a,b,Collide(a,b));
                    }
                }
                for(auto& b:m_Bodies)
                {
                    Require(Finite(b.Position)&&Finite(b.Velocity)&&std::abs(b.Position.X)<=100000&&std::abs(b.Position.Y)<=100000,"2D physics state escaped numeric bounds.");
                    b.Velocity=Limit(b.Velocity);
                }
            }
        }
    };
}
