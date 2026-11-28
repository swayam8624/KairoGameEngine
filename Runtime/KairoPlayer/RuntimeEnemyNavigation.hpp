#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kairo::player::detail
{
    struct EnemyNavRect {float X0,Z0,X1,Z1;};
    struct EnemyNavDocument {float Y=0;std::vector<EnemyNavRect> Rects;};
    // Rectangles are authored center-walkable regions, already inset for
    // capsule clearance. Shared full edges become existing NavMesh portals.
    inline EnemyNavDocument ParseEnemyNav(std::string_view bytes)
    {
        if(bytes.size()>65536)throw std::invalid_argument("Navigation document too large.");
        std::istringstream in{std::string(bytes)};in.imbue(std::locale::classic());
        std::string line;auto read=[&]{if(!std::getline(in,line))return false;
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.find('\r')!=std::string::npos)throw std::invalid_argument("Invalid nav line.");
            return true;};
        if(!read()||line!="kairo-enemy-nav 1"||!read())throw std::invalid_argument("Missing navigation header/height.");
        EnemyNavDocument doc;auto finite=[](float x){return std::isfinite(x)&&std::abs(x)<=10000;};
        {std::istringstream row(line);row.imbue(std::locale::classic());std::string cmd;
         if(!(row>>cmd>>doc.Y)||cmd!="height"||!finite(doc.Y))throw std::invalid_argument("Invalid nav height.");
         row>>std::ws;if(!row.eof())throw std::invalid_argument("Trailing height fields.");}
        while(read())
        {
            if(line.empty())continue;
            std::istringstream row(line);row.imbue(std::locale::classic());std::string cmd;EnemyNavRect r{};
            if(!(row>>cmd>>r.X0>>r.Z0>>r.X1>>r.Z1)||cmd!="rect"||!finite(r.X0)||!finite(r.Z0)||
                !finite(r.X1)||!finite(r.Z1)||r.X1-r.X0<.05f||r.Z1-r.Z0<.05f||doc.Rects.size()>=256)
                throw std::invalid_argument("Invalid navigation rectangle.");
            row>>std::ws;if(!row.eof())throw std::invalid_argument("Trailing rectangle fields.");
            for(const auto& a:doc.Rects)
                if(std::min(a.X1,r.X1)>std::max(a.X0,r.X0)&&std::min(a.Z1,r.Z1)>std::max(a.Z0,r.Z0))
                    throw std::invalid_argument("Overlapping navigation rectangles.");
            doc.Rects.push_back(r);
        }
        if(doc.Rects.empty())throw std::invalid_argument("Empty navigation mesh.");
        return doc;
    }
    // Bounded refresh: changing mode/goal never bypasses the 12-tick budget.
    // A failed or stale route is retried at most once per interval.
    class EnemyReplanPolicy
    {
        std::optional<std::array<double,3>> m_Goal;
        unsigned m_Wait=0,m_Mode=0;
    public:
        void Reset() noexcept{m_Goal.reset();m_Wait=0;}
        bool Step(unsigned mode,std::array<double,3> goal,bool retry)
        {
            if(!std::all_of(goal.begin(),goal.end(),[](double x){return std::isfinite(x)&&std::abs(x)<=100000;}))
                throw std::invalid_argument("Invalid navigation goal.");
            if(m_Wait)--m_Wait;
            if(m_Wait)return false;
            if(m_Goal&&m_Mode==mode&&!retry&&std::hypot(goal[0]-(*m_Goal)[0],goal[2]-(*m_Goal)[2])<.35&&
                std::abs(goal[1]-(*m_Goal)[1])<.35)return false;
            m_Goal=goal;m_Mode=mode;m_Wait=12;return true;
        }
    };
}
