#include "geometry_parity.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace a2fo::refit {
namespace {
bool fail(std::string& error,const char* why) noexcept {
    try {error=why;} catch(...) {error.clear();}
    return false;
}
template<std::size_t N> bool finite(const std::array<float,N>& a) noexcept {
    for(float v:a)if(!std::isfinite(v))return false;
    return true;
}
bool finite(const AuthoredVertex2& v) noexcept {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
        std::isfinite(v.u)&&std::isfinite(v.v);
}
bool finite(const ProjectedVertex2& v) noexcept {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&
        std::isfinite(v.u)&&std::isfinite(v.v)&&std::isfinite(v.rhw)&&v.rhw>0;
}
double determinant3(const std::array<float,12>& m) noexcept {
    return double(m[0])*(double(m[4])*m[8]-double(m[5])*m[7])-
           double(m[1])*(double(m[3])*m[8]-double(m[5])*m[6])+
           double(m[2])*(double(m[3])*m[7]-double(m[4])*m[6]);
}
bool invertible4(const std::array<float,16>& m) noexcept {
    double rows[4][4]{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)rows[r][c]=m[r*4+c];
    for(unsigned col=0;col<4;++col){
        unsigned pivot=col;
        for(unsigned r=col+1;r<4;++r)
            if(std::fabs(rows[r][col])>std::fabs(rows[pivot][col]))pivot=r;
        if(!std::isfinite(rows[pivot][col])||std::fabs(rows[pivot][col])<=1e-12)
            return false;
        if(pivot!=col)for(unsigned c=0;c<4;++c)std::swap(rows[pivot][c],rows[col][c]);
        const double scale=rows[col][col];
        for(unsigned r=col+1;r<4;++r){
            const double factor=rows[r][col]/scale;
            for(unsigned c=col;c<4;++c)rows[r][c]-=factor*rows[col][c];
        }
    }
    return true;
}
std::array<double,4> transform(const std::array<double,4>& p,
                               const std::array<float,16>& m) noexcept {
    std::array<double,4> out{};
    for(unsigned col=0;col<4;++col)
        for(unsigned row=0;row<4;++row)out[col]+=p[row]*m[row*4+col];
    return out;
}
double area(const ProjectedTriangle2& t) noexcept {
    return (double(t[1].x)-t[0].x)*(double(t[2].y)-t[0].y)-
           (double(t[1].y)-t[0].y)*(double(t[2].x)-t[0].x);
}
bool less_vertex(const ProjectedVertex2& a,const ProjectedVertex2& b) noexcept {
    if(a.x!=b.x)return a.x<b.x;
    if(a.y!=b.y)return a.y<b.y;
    if(a.z!=b.z)return a.z<b.z;
    if(a.u!=b.u)return a.u<b.u;
    return a.v<b.v;
}
void canonicalize(ProjectedTriangle2& t) noexcept {
    unsigned first=0;
    for(unsigned i=1;i<3;++i)if(less_vertex(t[i],t[first]))first=i;
    std::rotate(t.begin(),t.begin()+first,t.end());
}
bool near(const ProjectedTriangle2& a,const ProjectedTriangle2& b) noexcept {
    for(unsigned i=0;i<3;++i)
        if(std::fabs(double(a[i].x)-b[i].x)>geometry_xy_tolerance2||
           std::fabs(double(a[i].y)-b[i].y)>geometry_xy_tolerance2||
           std::fabs(double(a[i].z)-b[i].z)>geometry_z_tolerance2||
           std::fabs(double(a[i].u)-b[i].u)>geometry_uv_tolerance2||
           std::fabs(double(a[i].v)-b[i].v)>geometry_uv_tolerance2)return false;
    return true;
}
bool scale_interval(const ProjectedTriangle2& a,const ProjectedTriangle2& b,
                    double& lower,double& upper) noexcept {
    for(unsigned i=0;i<3;++i){
        const double ratio=double(b[i].rhw)/a[i].rhw;
        if(!std::isfinite(ratio)||ratio<=0)return false;
        lower=std::max(lower,ratio/(1+geometry_rhw_relative_tolerance2));
        upper=std::min(upper,ratio/(1-geometry_rhw_relative_tolerance2));
    }
    return lower<=upper;
}
bool match_triangles(const std::vector<ProjectedTriangle2>& expected,
                     const std::vector<ProjectedTriangle2>& observed,std::string& error) {
    if(expected.empty())return true;
    // Every native corner is indexed: a tiny projection/quantization difference
    // may change the lexicographic first corner, but not the cyclic winding.
    struct Corner {float x;std::uint32_t triangle,corner;};
    std::vector<Corner> index;index.reserve(observed.size()*3);
    double expected_sum=0,observed_sum=0;
    for(std::size_t t=0;t<observed.size();++t)for(unsigned c=0;c<3;++c){
        index.push_back({observed[t][c].x,static_cast<std::uint32_t>(t),c});
        observed_sum+=observed[t][c].rhw;expected_sum+=expected[t][c].rhw;
    }
    std::sort(index.begin(),index.end(),[](const Corner& a,const Corner& b){
        if(a.x!=b.x)return a.x<b.x;
        if(a.triangle!=b.triangle)return a.triangle<b.triangle;
        return a.corner<b.corner;
    });
    const double mean_scale=observed_sum/expected_sum;
    if(!std::isfinite(mean_scale)||mean_scale<=0)return fail(error,"invalid reciprocal W scale");
    // The weighted mean need not itself fit every corner. Use its feasible
    // interval, then intersect per-corner intervals for one common positive
    // scale. This preserves valid cases that straddle the mean/first corner.
    const double global_lower=mean_scale/(1+geometry_rhw_relative_tolerance2);
    const double global_upper=mean_scale/(1-geometry_rhw_relative_tolerance2);
    struct Edge {std::uint32_t target;double lower,upper;};
    std::vector<std::vector<Edge>> edges(expected.size());
    constexpr std::size_t work_limit=262144;
    std::size_t work=0;
    for(std::size_t t=0;t<expected.size();++t){
        const auto& a=expected[t];
        const double minimum=double(a[0].x)-geometry_xy_tolerance2;
        const double maximum=double(a[0].x)+geometry_xy_tolerance2;
        auto first=std::lower_bound(index.begin(),index.end(),minimum,
            [](const Corner& corner,double x){return double(corner.x)<x;});
        for(auto it=first;it!=index.end()&&it->x<=maximum;++it){
            if(++work>work_limit)return fail(error,"geometry matching work limit");
            const auto& b=observed[it->triangle];
            if((area(a)>0)!=(area(b)>0))continue;
            ProjectedTriangle2 rotated{};
            for(unsigned c=0;c<3;++c)rotated[c]=b[(it->corner+c)%3];
            if(!near(a,rotated))continue;
            double lower=global_lower,upper=global_upper;
            if(scale_interval(a,rotated,lower,upper))edges[t].push_back({it->triangle,lower,upper});
        }
        if(edges[t].empty())return fail(error,"projected geometry mismatch");
    }
    // First try an ordinary one-to-one match. Most meshes have an unambiguous
    // assignment, so they do not need the constrained scale search below.
    std::vector<int> selected(expected.size(),-1),owner(observed.size(),-1);
    std::vector<int> parent(observed.size(),-1),parent_edge(observed.size(),-1);
    std::vector<std::uint32_t> visited(observed.size(),0);
    std::uint32_t visit_generation=0;
    std::vector<std::uint32_t> queue;queue.reserve(expected.size());
    enum class Match {Complete,Impossible,WorkLimit};
    const auto assign=[&](bool constrained,double scale) {
        // Charge retry initialization, too; generation tags avoid clearing a
        // full parent array for each root of each candidate-scale search.
        if(expected.size()>(work_limit-work)/2)return Match::WorkLimit;
        work+=expected.size()*2;
        std::fill(selected.begin(),selected.end(),-1);
        std::fill(owner.begin(),owner.end(),-1);
        for(std::size_t root=0;root<expected.size();++root){
            if(visit_generation==UINT32_MAX)return Match::WorkLimit;
            ++visit_generation;
            queue.clear();queue.push_back(static_cast<std::uint32_t>(root));
            int free_target=-1;
            for(std::size_t head=0;head<queue.size()&&free_target<0;++head){
                const auto from=queue[head];
                for(std::size_t e=0;e<edges[from].size();++e){
                    if(++work>work_limit)return Match::WorkLimit;
                    const auto& edge=edges[from][e];
                    if(constrained&&(scale<edge.lower||scale>edge.upper))continue;
                    const auto target=edge.target;
                    if(visited[target]==visit_generation)continue;
                    visited[target]=visit_generation;
                    parent[target]=static_cast<int>(from);parent_edge[target]=static_cast<int>(e);
                    if(owner[target]<0){free_target=static_cast<int>(target);break;}
                    queue.push_back(static_cast<std::uint32_t>(owner[target]));
                }
            }
            if(free_target<0)return Match::Impossible;
            while(free_target>=0){
                const auto from=parent[free_target];
                const int previous=selected[from]<0?-1:static_cast<int>(edges[from][selected[from]].target);
                selected[from]=parent_edge[free_target];owner[free_target]=from;free_target=previous;
            }
        }
        return Match::Complete;
    };
    const auto initial=assign(false,0);
    if(initial==Match::WorkLimit)return fail(error,"geometry matching work limit");
    if(initial==Match::Impossible)return fail(error,"projected geometry multiset mismatch");
    double lower=global_lower,upper=global_upper;
    for(std::size_t t=0;t<expected.size();++t){
        const auto& edge=edges[t][selected[t]];
        lower=std::max(lower,edge.lower);upper=std::min(upper,edge.upper);
    }
    if(lower<=upper)return true;
    // A valid common-scale assignment may differ from that first matching.
    // For any such assignment, the maximum of its selected lower endpoints
    // belongs to every selected closed interval. Trying all distinct lower
    // endpoints therefore covers every feasible common scale, within the same
    // shared work budget. Never accept independently compatible edge scales.
    std::vector<double> scales{global_lower};
    for(const auto& candidates:edges)for(const auto& edge:candidates){
        if(++work>work_limit)return fail(error,"geometry matching work limit");
        scales.push_back(edge.lower);
    }
    std::sort(scales.begin(),scales.end());
    scales.erase(std::unique(scales.begin(),scales.end()),scales.end());
    for(const double scale:scales){
        if(++work>work_limit)return fail(error,"geometry matching work limit");
        const auto matched=assign(true,scale);
        if(matched==Match::Complete)return true;
        if(matched==Match::WorkLimit)return fail(error,"geometry matching work limit");
    }
    return fail(error,"reciprocal W scale mismatch");
}
}

bool check_geometry_parity2(const std::vector<AuthoredTriangle2>& authored,
    const std::array<float,12>& world,const std::array<float,16>& view,
    const std::array<float,16>& projection,const std::array<float,4>& viewport,
    GeometryCull2 cull,const std::vector<ProjectedTriangle2>& native,
    std::uint32_t& visible_triangles,std::string& error) noexcept {
    try {
        if(authored.size()>2048||native.size()>2048)
            return fail(error,"geometry triangle limit");
        if(!finite(world)||!finite(view)||!finite(projection)||!finite(viewport)||
           std::fabs(determinant3(world))<=1e-12||
           !invertible4(view)||!invertible4(projection)||
           viewport[0]<0||viewport[1]<0||viewport[2]<1||viewport[3]<1||
           viewport[2]>16384||viewport[3]>16384||
           double(viewport[0])+viewport[2]>UINT32_MAX||
           double(viewport[1])+viewport[3]>UINT32_MAX||
           viewport[0]!=std::floor(viewport[0])||
           viewport[1]!=std::floor(viewport[1])||
           viewport[2]!=std::floor(viewport[2])||
           viewport[3]!=std::floor(viewport[3]))
            return fail(error,"invalid geometry matrices or viewport");
        if(cull!=GeometryCull2::None&&cull!=GeometryCull2::Clockwise&&
           cull!=GeometryCull2::CounterClockwise)
            return fail(error,"invalid geometry cull mode");
        std::vector<ProjectedTriangle2> expected;
        expected.reserve(authored.size());
        std::vector<ProjectedTriangle2> culled_slivers;
        for(const auto& triangle:authored){
            ProjectedTriangle2 projected{};
            for(unsigned i=0;i<3;++i){
                const auto& v=triangle[i];
                if(!finite(v))return fail(error,"nonfinite authored vertex");
                const std::array<double,4> p={
                    double(v.x)*world[0]+double(v.y)*world[3]+double(v.z)*world[6]+world[9],
                    double(v.x)*world[1]+double(v.y)*world[4]+double(v.z)*world[7]+world[10],
                    double(v.x)*world[2]+double(v.y)*world[5]+double(v.z)*world[8]+world[11],1};
                const auto clip=transform(transform(p,view),projection);
                const double w=clip[3];
                if(!std::isfinite(w)||w<=0||!std::isfinite(clip[0])||
                   !std::isfinite(clip[1])||!std::isfinite(clip[2])||
                   clip[0]<-w||clip[0]>w||clip[1]<-w||clip[1]>w||
                   clip[2]<0||clip[2]>w)
                    return fail(error,"geometry clipping or invalid homogeneous W");
                const double x=(clip[0]/w+1)*viewport[2]/2+viewport[0];
                const double y=(1-clip[1]/w)*viewport[3]/2+viewport[1];
                const double z=clip[2]/w;
                if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
                   std::fabs(x)>std::numeric_limits<float>::max()||
                   std::fabs(y)>std::numeric_limits<float>::max())
                    return fail(error,"nonfinite projected vertex");
                const double rhw=1/w;
                if(!std::isfinite(rhw)||rhw<=0||
                   rhw<std::numeric_limits<float>::denorm_min()||
                   rhw>std::numeric_limits<float>::max())
                    return fail(error,"invalid projected reciprocal W");
                projected[i]={float(x),float(y),float(z),v.u,v.v,float(rhw)};
                if(!finite(projected[i]))
                    return fail(error,"invalid projected reciprocal W");
            }
            const double signed_area=area(projected);
            if(!std::isfinite(signed_area)||std::fabs(signed_area)<1e-8)
                return fail(error,"degenerate authored triangle");
            if((cull==GeometryCull2::Clockwise&&signed_area>0)||
               (cull==GeometryCull2::CounterClockwise&&signed_area<0)){
                // A culled sliver may still be drawn natively (facing flip).
                if(std::fabs(signed_area)/2<geometry_sliver_area2)culled_slivers.push_back(projected);
                continue;
            }
            expected.push_back(projected);
        }
        std::vector<ProjectedTriangle2> observed=native;
        for(auto& triangle:observed){
            for(const auto& v:triangle)if(!finite(v))
                return fail(error,"nonfinite native projected vertex");
            const double signed_area=area(triangle);
            if(!std::isfinite(signed_area)||std::fabs(signed_area)<1e-8)
                return fail(error,"degenerate native triangle");
        }
        // Strict path first: every currently admitted mesh keeps its exact
        // behaviour. The sliver path below runs only after a strict failure.
        std::string strict_error;
        if(expected.size()!=native.size())strict_error="geometry triangle count mismatch";
        else if(match_triangles(expected,observed,strict_error)){
            visible_triangles=static_cast<std::uint32_t>(expected.size());
            error.clear();
            return true;
        }
        if(strict_error=="geometry matching work limit")return fail(error,"geometry matching work limit");
        // Sliver-tolerant path. Triangles below geometry_sliver_area2 have
        // indeterminate facing and cover no sample, so an expected sliver may
        // be absent natively and a native sliver may come from a triangle this
        // projection culled. Every non-sliver must still match one-to-one with
        // the strict corner tolerances and one common reciprocal-W scale, and
        // every native sliver must match a distinct authored sliver.
        const auto sliver=[](const ProjectedTriangle2& t){return std::fabs(area(t))/2<geometry_sliver_area2;};
        std::vector<ProjectedTriangle2> expected_solid,expected_slivers=culled_slivers,native_solid,native_slivers;
        for(const auto& t:expected)(sliver(t)?expected_slivers:expected_solid).push_back(t);
        for(const auto& t:observed)(sliver(t)?native_slivers:native_solid).push_back(t);
        if(expected_slivers.empty()&&native_slivers.empty())return fail(error,strict_error.c_str());
        if(expected_solid.size()!=native_solid.size())return fail(error,strict_error.c_str());
        std::string solid_error;
        if(!match_triangles(expected_solid,native_solid,solid_error))
            return fail(error,solid_error=="geometry matching work limit"?solid_error.c_str():strict_error.c_str());
        if(native_slivers.size()>expected_slivers.size()||
           native_slivers.size()*expected_slivers.size()>262144)return fail(error,strict_error.c_str());
        std::vector<bool> used(expected_slivers.size(),false);
        for(const auto& b:native_slivers){
            bool found=false;
            for(std::size_t i=0;i<expected_slivers.size()&&!found;++i){
                if(used[i])continue;
                for(unsigned r=0;r<3&&!found;++r){
                    ProjectedTriangle2 rotated{};
                    for(unsigned c=0;c<3;++c)rotated[c]=b[(r+c)%3];
                    if(near(expected_slivers[i],rotated))found=used[i]=true;
                }
            }
            if(!found)return fail(error,strict_error.c_str());
        }
        visible_triangles=static_cast<std::uint32_t>(native.size());
        error.clear();
        return true;
    } catch(...) {return fail(error,"geometry parity allocation failed");}
}

std::string explain_geometry_parity2(const std::vector<AuthoredTriangle2>& authored,
    const std::array<float,12>& world,const std::array<float,16>& view,
    const std::array<float,16>& projection,const std::array<float,4>& viewport,
    GeometryCull2 cull,const std::vector<ProjectedTriangle2>& native) noexcept {
    try {
        if(authored.size()>2048||native.size()>2048)return "limit";
        std::vector<ProjectedTriangle2> all;unsigned clipped=0,degenerate=0,cw=0,ccw=0;
        for(const auto& triangle:authored){
            ProjectedTriangle2 p{};bool ok=true;
            for(unsigned i=0;i<3&&ok;++i){
                const auto& v=triangle[i];
                const std::array<double,4> q={
                    double(v.x)*world[0]+double(v.y)*world[3]+double(v.z)*world[6]+world[9],
                    double(v.x)*world[1]+double(v.y)*world[4]+double(v.z)*world[7]+world[10],
                    double(v.x)*world[2]+double(v.y)*world[5]+double(v.z)*world[8]+world[11],1};
                const auto c=transform(transform(q,view),projection);const double w=c[3];
                if(!(w>0)||c[0]<-w||c[0]>w||c[1]<-w||c[1]>w||c[2]<0||c[2]>w){ok=false;break;}
                p[i]={float((c[0]/w+1)*viewport[2]/2+viewport[0]),float((1-c[1]/w)*viewport[3]/2+viewport[1]),
                      float(c[2]/w),v.u,v.v,float(1/w)};
            }
            if(!ok){++clipped;continue;}
            const double a=area(p);
            if(!std::isfinite(a)||std::fabs(a)<1e-8){++degenerate;continue;}
            (a>0?ccw:cw)++;
            if((cull==GeometryCull2::Clockwise&&a>0)||(cull==GeometryCull2::CounterClockwise&&a<0))continue;
            canonicalize(p);all.push_back(p);
        }
        unsigned native_cw=0,native_ccw=0;
        std::vector<ProjectedTriangle2> observed=native;
        for(auto& t:observed){(area(t)>0?native_ccw:native_cw)++;canonicalize(t);}
        char text[400]{};
        int n=std::snprintf(text,sizeof(text),"authored=%zu clipped=%u degenerate=%u cw=%u ccw=%u expected=%zu native=%zu native_cw=%u native_ccw=%u cull=%d",
            authored.size(),clipped,degenerate,cw,ccw,all.size(),observed.size(),native_cw,native_ccw,int(cull));
        // Greedy rotation-tolerant matching with the check's own corner
        // tolerances (diagnostic only; the admission matcher is match_triangles).
        if(!all.empty()&&all.size()*observed.size()<=2048u*2048u){
            std::vector<bool> used(observed.size(),false);std::vector<double> unmatched;
            std::vector<double> areas;areas.reserve(all.size());
            for(const auto& a:all){
                areas.push_back(std::fabs(area(a)));bool found=false;
                for(std::size_t j=0;j<observed.size()&&!found;++j){
                    if(used[j])continue;
                    for(unsigned r=0;r<3&&!found;++r){
                        ProjectedTriangle2 b{};for(unsigned c=0;c<3;++c)b[c]=observed[j][(r+c)%3];
                        if(near(a,b)){used[j]=true;found=true;}
                    }
                }
                if(!found)unmatched.push_back(std::fabs(area(a))/2);
            }
            std::sort(areas.begin(),areas.end());std::sort(unmatched.begin(),unmatched.end());
            const auto native_unmatched=static_cast<std::size_t>(std::count(used.begin(),used.end(),false));
            std::string list;
            for(std::size_t i=0;i<unmatched.size()&&i<6;++i){char s[24]{};std::snprintf(s,sizeof(s),"%s%.3g",i?",":"",unmatched[i]);list+=s;}
            if(n>0&&std::size_t(n)<sizeof(text))
                std::snprintf(text+n,sizeof(text)-n," matched=%zu expected_unmatched=%zu native_unmatched=%zu unmatched_px2=[%s] median_px2=%.3g",
                    all.size()-unmatched.size(),unmatched.size(),native_unmatched,list.c_str(),areas[areas.size()/2]/2);
        }
        return text;
    } catch(...) {return "explain failed";}
}
} // namespace a2fo::refit
