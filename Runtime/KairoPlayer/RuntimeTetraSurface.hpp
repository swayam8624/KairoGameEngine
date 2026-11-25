#pragma once
#include "RuntimeSoftBody.hpp"
#include <array>
#include <stdexcept>

namespace kairo::player::detail
{
    // CPU geometry for one tetrahedral surface. Duplicate each face's three
    // vertices so flat normals do not get averaged across sharp edges. The
    // renderer adapter converts these doubles to its own vertex ABI.
    struct TetraSurfaceVertex{SoftPoint Position,Normal;};
    using TetraSurface=std::array<TetraSurfaceVertex,12>;
    [[nodiscard]] inline TetraSurface BuildTetraSurface(const std::array<SoftPoint,4>& points)
    {
        using namespace soft_detail;
        for(const auto& p:points)Validate(p);
        if(std::abs(Volume(points[0],points[1],points[2],points[3]))<1e-12)
            throw std::invalid_argument("Degenerate visual tetrahedron.");
        constexpr std::array<std::array<unsigned,4>,4> faces{{{{0,1,2,3}},{{0,3,1,2}},{{0,2,3,1}},{{1,3,2,0}}}};
        TetraSurface surface{};std::size_t next=0;
        for(const auto& face:faces)
        {
            auto a=points[face[0]],b=points[face[1]],c=points[face[2]];
            const auto opposite=points[face[3]];
            auto normal=Cross(Sub(b,a),Sub(c,a));
            // Orient the face away from the fourth point, even when the
            // tetrahedron's original signed volume is negative. Swapping
            // winding and negating the normal must occur together.
            if(Dot(normal,Sub(opposite,a))>0){std::swap(b,c);normal=Scale(normal,-1.);}
            const auto length=Length(normal);
            if(!std::isfinite(length)||length<1e-10)throw std::invalid_argument("Degenerate visual tetra face.");
            normal=Scale(normal,1./length);
            for(const auto& p:{a,b,c})surface[next++]={p,normal};
        }
        return surface;
    }
    // This visual adapter only accepts the narrow four-vertex fixture.
    // A general tetrahedral mesh needs boundary-face extraction and deduplication.
    [[nodiscard]] inline TetraSurface BuildTetraSurface(const RuntimeSoftBody& body)
    {
        if(body.Vertices().size()!=4)throw std::invalid_argument("Expected four visual tetra vertices.");
        return BuildTetraSurface(std::array{body.Vertices()[0].Position,body.Vertices()[1].Position,
            body.Vertices()[2].Position,body.Vertices()[3].Position});
    }
}
