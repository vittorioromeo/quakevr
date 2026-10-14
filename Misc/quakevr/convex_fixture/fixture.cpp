#include "vr_convex.hpp"
#include "vr_jobs.hpp"
#include <box3d/box3d.h>
#include <chrono>
#include <cstdarg>
#include <atomic>
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
namespace qvr::convex { bool baselineDecompose(const za::Vector<glm::vec3>&, const Settings&, za::Vector<Piece>&, Report*); }
extern "C" double Sys_DoubleTime() {return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
extern "C" void Con_Printf(const char*, ...) {}
static bool fixtureParallel=false;
static int faultMode=0, side=0;
static std::atomic<size_t> budgets[2][45]{};
static std::atomic<size_t> visits[2][2]{};
static size_t peakRetained=0;
b3HullData* fixtureHull(const b3Vec3* points,int count,int budget) {
 ++budgets[side][budget];
 if(faultMode==3||(faultMode==1&&budget==24)||(faultMode==2&&budget>24))return nullptr;
 return b3CreateHull(points,count,budget);
}
void fixtureVisit(int stage){++visits[side][stage];}
void fixtureRetained(size_t bytes){peakRetained=std::max(peakRetained,bytes);}
namespace qvr::jobs {
Site::Site(const char* n) noexcept:name(n),next(nullptr) {}
void parallelFor(Site&,za::SizeT count,za::SizeT chunk,FunctionRef<void(za::SizeT,za::SizeT)> body,bool) noexcept {
 if(!fixtureParallel){for(za::SizeT c=0;c<count;c+=chunk)body(c,std::min(count,c+chunk));return;}
 std::atomic<size_t> at{0}; std::vector<std::thread> ts;
 for(int i=0;i<4;++i)ts.emplace_back([&]{for(;;){auto c=at.fetch_add(chunk);if(c>=count)break;body(c,std::min(count,c+chunk));}});
 for(auto& t:ts)t.join();
}
}
static bool equal(const za::Vector<qvr::convex::Piece>& a,const za::Vector<qvr::convex::Piece>& b) {
 if(a.size()!=b.size())return false;
 for(size_t i=0;i<a.size();++i)if(a[i].points.size()!=b[i].points.size()||memcmp(a[i].points.data(),b[i].points.data(),a[i].points.size()*sizeof(glm::vec3)))return false;
 return true;
}
static int cases=0; static double before=0,after=0;
static bool check(const char* label,const za::Vector<glm::vec3>& pts,const qvr::convex::Settings& s) {
 const auto bytes=b3GetByteCount(); qvr::convex::Report a,b; za::Vector<qvr::convex::Piece> pa,pb;
 side=0; bool oka=qvr::convex::baselineDecompose(pts,s,pa,&a); side=1; bool okb=qvr::convex::decompose(pts,s,pb,&b);
 before+=a.ms;after+=b.ms; a.ms=b.ms=0;
 // These seven contiguous non-timing fields have no padding (six floats, then cells).
 bool report=memcmp(&a.wholeGap,&b.wholeGap,7*sizeof(float))==0;
 if(oka!=okb||!equal(pa,pb)||!report||bytes!=b3GetByteCount()) {
  printf("FAIL %s parallel=%d most=%d cell=%g tolerance=%g pieces=%zu/%zu bytes=%lld/%lld\n",label,fixtureParallel,s.maxPieces,s.cell,s.tolerance,pa.size(),pb.size(),(long long)bytes,(long long)b3GetByteCount());return false;
 }
 ++cases; return true;
}
static void box(za::Vector<glm::vec3>& p,glm::vec3 lo,glm::vec3 hi) {
 glm::vec3 v[8];for(int i=0;i<8;++i)v[i]={i&1?hi.x:lo.x,i&2?hi.y:lo.y,i&4?hi.z:lo.z};
 constexpr int t[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};for(int i:t)p.pushBack(v[i]);
}
int main(int argc,char**argv) {
 if(argc!=2)return 2;
 std::vector<std::filesystem::path> paths;for(const auto& e:std::filesystem::directory_iterator(argv[1]))if(e.path().extension()==".input")paths.push_back(e.path());std::sort(paths.begin(),paths.end());
 for(int threads=0;threads<2;++threads){fixtureParallel=threads;
  for(const auto& path:paths){std::ifstream in(path,std::ios::binary);qvr::convex::Settings s; unsigned count;
   in.read((char*)&s.cell,4);in.read((char*)&s.tolerance,4);in.read((char*)&s.maxPieces,4);in.read((char*)&count,4);
   za::Vector<glm::vec3> pts;pts.resize(count);in.read((char*)pts.data(),count*sizeof(glm::vec3));if(!in)return 3;
   for(int most:{1,2,6,12,32}){s.maxPieces=most;if(!check(path.filename().string().c_str(),pts,s))return 1;}
   s.maxPieces=12;for(float tolerance:{0.f,100.f}){s.tolerance=tolerance;if(!check("tolerance",pts,s))return 1;}
   s.tolerance=.3f;for(float cell:{.08f,.25f}){s.cell=cell;if(!check("cell",pts,s))return 1;}
   s.cell=.12f;for(int fault:{1,2,3}){faultMode=fault;if(!check("hull-failure",pts,s))return 1;}faultMode=0;
  }
  qvr::convex::Settings s;za::Vector<glm::vec3> pts;
  if(!check("empty",pts,s))return 1;
  for(int i=0;i<12;++i)pts.pushBack({1,1,1});if(!check("point",pts,s))return 1;
  pts.clear();for(int i=0;i<4;++i){pts.pushBack({0,0,0});pts.pushBack({1,0,0});pts.pushBack({0,1,0});}if(!check("flat",pts,s))return 1;
  pts.clear();box(pts,{0,0,0},{3,3,3});if(!check("box-done",pts,s))return 1;
  box(pts,{2,0,0},{5,1,1});if(!check("L-shape",pts,s))return 1;
  pts.clear();box(pts,{0,0,0},{3,3,3});s.maxPieces=0;if(!check("zero-most",pts,s))return 1;
  s.maxPieces=12;s.cell=.0001f;box(pts,{0,0,0},{100,100,100});if(!check("grid-reject",pts,s))return 1;
 }
 printf("budget24 calls=%zu/%zu; budget36=%zu/%zu; budget44=%zu/%zu; regionPoints=%zu/%zu; gap=%zu/%zu; peak retained point capacity+hulls=%zu bytes\n",budgets[0][24].load(),budgets[1][24].load(),budgets[0][36].load(),budgets[1][36].load(),budgets[0][44].load(),budgets[1][44].load(),visits[0][0].load(),visits[1][0].load(),visits[0][1].load(),visits[1][1].load(),peakRetained);
 if(budgets[0][36]!=budgets[1][36]||budgets[0][44]!=budgets[1][44]||budgets[0][36]==0||budgets[0][44]==0){puts("FAIL refinement coverage");return 1;}
 printf("PASS corpus=%zu comparisons=%d point/piece/report bytes identical; hull allocation balance=0; sequential+4-thread; aggregate ms before=%.3f after=%.3f\n",paths.size(),cases,before,after);return 0;
}
