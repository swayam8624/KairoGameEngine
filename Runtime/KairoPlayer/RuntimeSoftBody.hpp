#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    // Position-based tetrahedral deformable body. Positions are meters,
    // velocities meters/second, inverse mass zero means a pinned vertex.
    // A fixed step and deterministic constraint order make replay possible.
    // This is not a continuum FEM solver or a self-collision implementation.
    using SoftPoint=std::array<double,3>;
    namespace soft_detail
    {
        // Elementary Cartesian operations keep the solver independent of GPU APIs.
        inline SoftPoint Add(SoftPoint a,SoftPoint b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
        inline SoftPoint Sub(SoftPoint a,SoftPoint b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
        inline SoftPoint Scale(SoftPoint a,double s){return {a[0]*s,a[1]*s,a[2]*s};}
        inline double Dot(SoftPoint a,SoftPoint b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
        inline SoftPoint Cross(SoftPoint a,SoftPoint b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
        inline double Length(SoftPoint a){return std::sqrt(Dot(a,a));}
        inline void Validate(SoftPoint a)
        {for(double x:a)if(!std::isfinite(x)||std::abs(x)>1e6)throw std::invalid_argument("Invalid soft body vector.");}
        // Signed tetra volume; orientation is preserved in the rest constraint.
        inline double Volume(SoftPoint a,SoftPoint b,SoftPoint c,SoftPoint d)
        {return Dot(Sub(b,a),Cross(Sub(c,a),Sub(d,a)))/6.;}
    }
    struct SoftVertex{SoftPoint Position{},Velocity{};double InverseMass=1.;};
    class RuntimeSoftBody
    {
    public:
        static constexpr double FixedStep=1./120.;
        // Add vertices before stepping. Index is stable; no topology removal.
        std::size_t AddVertex(SoftPoint p,double inverseMass=1.)
        {
            soft_detail::Validate(p);
            if(!std::isfinite(inverseMass)||inverseMass<0||inverseMass>100||m_Vertices.size()>=4096)
                throw std::invalid_argument("Invalid soft vertex.");
            m_Vertices.push_back({p,{},inverseMass});return m_Vertices.size()-1;
        }
        // Distance constraint stores the initial length as its rest value.
        void AddEdge(std::size_t a,std::size_t b,double compliance=1e-7)
        {
            using namespace soft_detail;
            ValidateCompliance(compliance);Require(a);Require(b);
            const auto rest=Length(Sub(m_Vertices[b].Position,m_Vertices[a].Position));
            if(a==b||rest<1e-6||m_Edges.size()>=16384)throw std::invalid_argument("Invalid soft edge.");
            m_Edges.push_back({a,b,rest,compliance});
        }
        // Volume constraint resists collapse of a nondegenerate tetrahedron.
        void AddTetra(std::array<std::size_t,4> ids,double compliance=1e-8)
        {
            ValidateCompliance(compliance);for(auto id:ids)Require(id);
            const auto rest=soft_detail::Volume(m_Vertices[ids[0]].Position,m_Vertices[ids[1]].Position,
                m_Vertices[ids[2]].Position,m_Vertices[ids[3]].Position);
            if(std::abs(rest)<1e-9||m_Tetra.size()>=16384)throw std::invalid_argument("Invalid soft tetrahedron.");
            m_Tetra.push_back({ids,rest,compliance});
        }
        [[nodiscard]] const std::vector<SoftVertex>& Vertices() const noexcept{return m_Vertices;}
        // Signed volume of the current mesh element, useful as a diagnostic.
        [[nodiscard]] double TetraVolume(std::size_t index) const
        {
            const auto& ids=m_Tetra.at(index).IDs;
            return soft_detail::Volume(m_Vertices[ids[0]].Position,m_Vertices[ids[1]].Position,
                m_Vertices[ids[2]].Position,m_Vertices[ids[3]].Position);
        }
        // XPBD: predict positions, project compliant distance and signed
        // volume constraints, then project against an infinite ground plane.
        // Lambdas persist within one step, reset across steps. Reconstruct
        // velocity from corrected positions; publish only finite results.
        // Caller owns the fixed-step accumulator and mesh presentation.
        void Step(SoftPoint acceleration={0,-9.81,0},double floor=0,unsigned iterations=8)
        {
            using namespace soft_detail;
            Validate(acceleration);
            if(!std::isfinite(floor)||std::abs(floor)>1e6||!iterations||iterations>32)
                throw std::invalid_argument("Invalid soft step.");
            auto next=m_Vertices;
            for(auto& v:next)if(v.InverseMass>0)
            {
                v.Velocity=Add(Scale(v.Velocity,.995),Scale(acceleration,FixedStep));
                v.Position=Add(v.Position,Scale(v.Velocity,FixedStep));
            }
            std::vector<double> edgeLambda(m_Edges.size()),tetraLambda(m_Tetra.size());
            for(unsigned iteration=0;iteration<iterations;++iteration)
            {
                for(std::size_t i=0;i<m_Edges.size();++i)
                {
                    const auto& e=m_Edges[i];auto& a=next[e.A];auto& b=next[e.B];
                    const auto delta=Sub(b.Position,a.Position);const double length=Length(delta);
                    if(length<1e-12)continue;
                    const auto g=Scale(delta,1./length);
                    Project(next,std::array{e.A,e.B},std::array{Scale(g,-1.),g},length-e.Rest,e.Compliance,edgeLambda[i]);
                }
                for(std::size_t i=0;i<m_Tetra.size();++i)
                {
                    const auto& t=m_Tetra[i];const auto& id=t.IDs;
                    const auto a=next[id[0]].Position,b=next[id[1]].Position,c=next[id[2]].Position,d=next[id[3]].Position;
                    const auto ab=Sub(b,a),ac=Sub(c,a),ad=Sub(d,a);
                    // Analytic derivatives of signed volume with respect to each vertex.
                    const auto gb=Scale(Cross(ac,ad),1./6.),gc=Scale(Cross(ad,ab),1./6.),gd=Scale(Cross(ab,ac),1./6.);
                    const auto ga=Scale(Add(Add(gb,gc),gd),-1.);
                    Project(next,id,std::array{ga,gb,gc,gd},Volume(a,b,c,d)-t.Rest,t.Compliance,tetraLambda[i]);
                }
                for(auto& v:next)if(v.InverseMass>0)v.Position[1]=std::max(v.Position[1],floor);
            }
            for(std::size_t i=0;i<next.size();++i)
            {
                auto& v=next[i];
                if(v.InverseMass>0)v.Velocity=Scale(Sub(v.Position,m_Vertices[i].Position),1./FixedStep);
                Validate(v.Position);Validate(v.Velocity);
            }
            m_Vertices.swap(next);
        }
    private:
        struct Edge{std::size_t A,B;double Rest,Compliance;};
        struct Tetra{std::array<std::size_t,4> IDs;double Rest,Compliance;};
        std::vector<SoftVertex> m_Vertices;
        std::vector<Edge> m_Edges;
        std::vector<Tetra> m_Tetra;
        void Require(std::size_t i) const{if(i>=m_Vertices.size())throw std::out_of_range("Soft vertex index.");}
        static void ValidateCompliance(double c)
        {if(!std::isfinite(c)||c<0||c>1)throw std::invalid_argument("Invalid compliance.");}
        // Generic XPBD scalar constraint update. The denominator contains
        // weighted gradient norms plus compliance/dt^2. Pinned vertices have
        // zero inverse mass, hence cannot move. A degenerate gradient is skipped.
        template<std::size_t N>
        static void Project(std::vector<SoftVertex>& v,const std::array<std::size_t,N>& ids,
            const std::array<SoftPoint,N>& gradients,double error,double compliance,double& lambda)
        {
            using namespace soft_detail;
            const double alpha=compliance/(FixedStep*FixedStep);double denominator=alpha;
            for(std::size_t i=0;i<N;++i)denominator+=v[ids[i]].InverseMass*Dot(gradients[i],gradients[i]);
            if(denominator<=1e-20)return;
            const double change=(-error-alpha*lambda)/denominator;lambda+=change;
            for(std::size_t i=0;i<N;++i)
                v[ids[i]].Position=Add(v[ids[i]].Position,Scale(gradients[i],v[ids[i]].InverseMass*change));
        }
    };
}
