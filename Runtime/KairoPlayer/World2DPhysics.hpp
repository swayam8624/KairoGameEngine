#pragma once
#include "Physics2D.hpp"
#include <istream>
#include <map>
#include <optional>
#include <string>

namespace kairo::player::world2d
{
    namespace p2=kairo::physics2d;
    struct BodyRecord {unsigned Actor=0;p2::Body Body;};
    struct JointRecord {unsigned A=0,B=0;double Length=0;};
    struct PhysicsConfig
    {
        p2::Vec Gravity;
        std::vector<BodyRecord> Bodies;
        std::vector<JointRecord> Joints;
    };
    inline PhysicsConfig ReadPhysics(std::istream& in)
    {
        PhysicsConfig config;std::string tag;int count=0;
        p2::Require(bool(in>>tag>>config.Gravity.X>>config.Gravity.Y)&&tag=="physics"&&
            p2::Finite(config.Gravity)&&p2::Length(config.Gravity)<=2000,"Invalid 2D physics gravity.");
        p2::Require(bool(in>>tag>>count)&&tag=="bodies"&&count>=1&&count<=128,"Invalid 2D physics body count.");
        for(int i=0;i<count;++i)
        {
            BodyRecord record;auto& b=record.Body;
            std::string motion,shape;long long id=0,layer=0,mask=0;int sensor=0;
            p2::Require(bool(in>>id>>motion>>shape>>b.Mass>>b.Restitution>>b.Friction>>layer>>mask>>sensor>>b.Velocity.X>>b.Velocity.Y)&&
                id>0&&id<=0xffffffffll&&layer>0&&layer<=0xffffffffll&&mask>=0&&mask<=0xffffffffll&&(sensor==0||sensor==1)&&
                (motion=="static"||motion=="dynamic"||motion=="kinematic")&&(shape=="box"||shape=="circle"),"Invalid 2D physics body record.");
            record.Actor=static_cast<unsigned>(id);b.Layer=static_cast<std::uint32_t>(layer);b.Mask=static_cast<std::uint32_t>(mask);b.Sensor=sensor!=0;
            b.Type=motion=="dynamic"?p2::Motion::Dynamic:motion=="kinematic"?p2::Motion::Kinematic:p2::Motion::Static;
            b.Geometry=shape=="circle"?p2::Shape::Circle:p2::Shape::Box;
            p2::Validate(b);config.Bodies.push_back(record);
        }
        p2::Require(bool(in>>tag>>count)&&tag=="joints"&&count>=0&&count<=int(p2::MaxJoints),"Invalid 2D physics joint count.");
        for(int i=0;i<count;++i)
        {
            long long a=0,b=0;double length=0;
            p2::Require(bool(in>>a>>b>>length)&&a>0&&a<=0xffffffffll&&b>0&&b<=0xffffffffll,"Invalid 2D physics joint record.");
            config.Joints.push_back({static_cast<unsigned>(a),static_cast<unsigned>(b),length});
        }
        return config;
    }
    struct PhysicsBinding
    {
        p2::Scene Scene;
        std::vector<std::pair<std::size_t,std::size_t>> Moving; // actor index, body index
        std::size_t Player=0;
        template<class Bounds> static p2::Vec Center(Bounds b){return {b.X+b.W*.5,b.Y+b.H*.5};}
        template<class Actors> void Step(Actors& actors,std::size_t player)
        {
            Scene.SetKinematic(Player,Center(actors[player].Bounds));
            Scene.Step();
            for(const auto& [actor,index]:Moving)
            {
                const auto& body=Scene.Bodies()[index];auto& b=actors[actor].Bounds;
                b.X=static_cast<int>(std::lround(body.Position.X-body.Half.X));
                b.Y=static_cast<int>(std::lround(body.Position.Y-body.Half.Y));
            }
        }
    };
    template<class World> PhysicsBinding BuildPhysics(const World& world,const PhysicsConfig& config)
    {
        std::vector<p2::Body> bodies;std::vector<p2::Joint> joints;
        std::vector<std::pair<std::size_t,std::size_t>> moving;
        std::map<unsigned,p2::Body> authored;
        for(const auto& record:config.Bodies)
            p2::Require(authored.emplace(record.Actor,record.Body).second,"Duplicate authored physics actor.");
        std::map<unsigned,std::size_t> ids;std::size_t player=0;
        const auto add=[&](p2::Body b){p2::Require(bodies.size()<p2::MaxBodies,"World physics exceeds body budget.");bodies.push_back(b);return bodies.size()-1;};
        for(std::size_t i=0;i<world.Actors.size();++i)
        {
            const auto& a=world.Actors[i];using Kind=decltype(a.Type);
            const bool isPlayer=a.Type==Kind::Player,solid=a.Type==Kind::Solid;
            auto found=authored.find(a.ID);
            if(!isPlayer&&!solid&&found==authored.end())continue;
            p2::Body b;
            if(found!=authored.end())
            {
                p2::Require(solid||a.Type==Kind::Decor,"Physics bodies may only bind solid or decor actors.");
                b=found->second;authored.erase(found);
                // The existing player controller still sees solid actors as AABBs.
                // Keep their shape and collision mask consistent with that rule.
                p2::Require(!solid||(b.Geometry==p2::Shape::Box&&!b.Sensor&&b.Type!=p2::Motion::Kinematic&&(b.Mask&3)==3),
                    "Solid physics actors require boxes, map/player collision and no sensors or kinematic motion.");
                if(b.Geometry==p2::Shape::Circle)p2::Require(a.Bounds.W==a.Bounds.H,"Circle actor bounds must be square.");
            }
            b.Position=PhysicsBinding::Center(a.Bounds);b.Half={a.Bounds.W*.5,a.Bounds.H*.5};
            if(isPlayer){b.Type=p2::Motion::Kinematic;b.Layer=2;}
            const auto index=add(b);ids.emplace(a.ID,index);
            if(isPlayer)player=index;
            else if(b.Type!=p2::Motion::Static)moving.emplace_back(i,index);
        }
        p2::Require(authored.empty(),"Physics references missing actor.");
        const auto wall=[&](double x,double y,double w,double h)
        {
            p2::Body b;b.Position={x+w*.5,y+h*.5};b.Half={w*.5,h*.5};b.Restitution=1;b.Friction=.8;add(b);
        };
        const int width=world.Width*16,height=world.Height*16;
        wall(-8,-8,8,height+16);wall(width,-8,8,height+16);wall(0,-8,width,8);wall(0,height,width,8);
        for(int y=0;y<world.Height;++y)for(int x=0;x<world.Width;)
        {
            if(!world.Tiles.at(world.Map[y][x]).Solid){++x;continue;}
            int end=x+1;while(end<world.Width&&world.Tiles.at(world.Map[y][end]).Solid)++end;
            wall(x*16,y*16,(end-x)*16,16);x=end;
        }
        for(const auto& j:config.Joints)
        {
            p2::Require(ids.contains(j.A)&&ids.contains(j.B),"Joint references missing physics actor.");
            joints.push_back({ids.at(j.A),ids.at(j.B),j.Length});
        }
        return {p2::Scene(std::move(bodies),std::move(joints),config.Gravity),std::move(moving),player};
    }
}
