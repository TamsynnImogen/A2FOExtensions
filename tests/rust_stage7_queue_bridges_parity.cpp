#include "refit_queue_bridge_api.hpp"
#include "refit_queue_bridge_client.hpp"
#include "hybrid_bridge_api.hpp"
#include "hybrid_bridge_client.hpp"
#include <cstdint>
#include <iostream>
extern "C" bool A2FO_RegisterRefitQueueBridge(const A2FO_RefitQueueBridge*);
extern "C" bool A2FO_RegisterHybridBridge(const A2FO_HybridBridge*);
namespace {
bool answer{};
void trace(const char* tag, void* a, void* b=nullptr, std::uint32_t q=0, std::uint32_t v=0, void* c=nullptr) noexcept {
 std::cout<<tag<<':'<<reinterpret_cast<std::uintptr_t>(a)<<','<<reinterpret_cast<std::uintptr_t>(b)<<','<<q<<','<<v<<','<<reinterpret_cast<std::uintptr_t>(c)<<';';
}
bool two(void* a,void* b) noexcept {trace("two",a,b);return answer;}
bool one(void* a) noexcept {trace("one",a);return answer;}
void done(void* a,std::uint32_t q,void* b,void* c) noexcept {trace("done",a,b,q,0,c);}
void removed(void* a,std::uint32_t q,void* b,std::uint32_t v) noexcept {trace("removed",a,b,q,v);}
void finalize(void* a,void* b,bool admitted) noexcept {trace("finalize",a,b,0,admitted);}
void discard(void* a,std::uint32_t q) noexcept {trace("discard",a,nullptr,q);}
void clear(void* a) noexcept {trace("clear",a);}
void retain(void* a,void* b) noexcept {trace("retain",a,b);}
std::size_t publish(void* a) noexcept {trace("publish",a);return answer?0xabcdefu:0;}
void cocoon(void* a) noexcept {trace("cocoon",a);}
void construction(void* a) noexcept {trace("construction",a);}
void invoke(unsigned i) {
 void* a=reinterpret_cast<void*>(std::uintptr_t(i%5));void* b=reinterpret_cast<void*>(std::uintptr_t(i%7));void* c=reinterpret_cast<void*>(std::uintptr_t(i%11));
 using namespace a2fo;
 std::cout<<"dispatch "<<i<<' '<<consume_refit_synchronized_command(a,b)<<' '<<cancel_refit_synchronized_command(a)<<' '<<refit_has_waiting_job(a)<<' ';
 notify_refit_job_finished(a,i,b,c);notify_refit_job_removed(a,i,b,i%6);
 std::cout<<hybrid_production_has_queued_research_conflict(a,b)<<' '<<hybrid_production_is_evolve_target(a,b)<<' '<<hybrid_production_has_evolution_barrier(a)<<' '<<hybrid_production_should_defer_construct_order(a,b)<<' ';
 finalize_hybrid_construct_order(a,b,answer);discard_hybrid_construct_placement(a,i);clear_hybrid_construct_placements(a);retain_hybrid_research_menu_after_order(a,b);
 std::cout<<register_research_station_hybrid_lists(a,b)<<' '<<publish_research_station_hybrid_items(a)<<' ';cleanup_hybrid_cocoon(a);cleanup_hybrid_construction(a);std::cout<<'\n';
}
}
int main(){
 A2FO_RefitQueueBridge r{sizeof(r),A2FO_REFIT_QUEUE_BRIDGE_VERSION,two,one,one,done,removed};
 A2FO_HybridBridge h{sizeof(h),A2FO_HYBRID_BRIDGE_VERSION,two,two,one,two,finalize,discard,clear,retain,two,publish,cocoon,construction};
 invoke(0);
 std::cout<<"null "<<A2FO_RegisterRefitQueueBridge(nullptr)<<','<<A2FO_RegisterHybridBridge(nullptr)<<'\n';
 for(unsigned i=0;i<8;++i){auto x=r;auto y=h;x.version=i;y.version=i;x.struct_size=0;y.struct_size=0;std::cout<<"headers "<<i<<' '<<A2FO_RegisterRefitQueueBridge(&x)<<','<<A2FO_RegisterHybridBridge(&y)<<'\n';}
 for(unsigned i=0;i<sizeof(r);++i){auto x=r;x.struct_size=i;std::cout<<"refit_size "<<i<<' '<<A2FO_RegisterRefitQueueBridge(&x)<<'\n';}
 for(unsigned i=0;i<sizeof(h);++i){auto x=h;x.struct_size=i;std::cout<<"hybrid_size "<<i<<' '<<A2FO_RegisterHybridBridge(&x)<<'\n';}
 for(unsigned i=0;i<5;++i){auto x=r;switch(i){case 0:x.consume_synchronized_command=nullptr;break;case 1:x.cancel_synchronized_command=nullptr;break;case 2:x.has_waiting_job=nullptr;break;case 3:x.job_finished=nullptr;break;case 4:x.job_removed=nullptr;break;}std::cout<<"missing_refit "<<i<<' '<<A2FO_RegisterRefitQueueBridge(&x)<<'\n';}
 for(unsigned i=0;i<12;++i){auto x=h;switch(i){case 0:x.has_queued_research_conflict=nullptr;break;case 1:x.is_evolve_target=nullptr;break;case 2:x.has_evolution_barrier=nullptr;break;case 3:x.should_defer_construct_order=nullptr;break;case 4:x.finalize_construct_order=nullptr;break;case 5:x.discard_construct_placement=nullptr;break;case 6:x.clear_construct_placements=nullptr;break;case 7:x.retain_research_menu_after_order=nullptr;break;case 8:x.register_research_station_lists=nullptr;break;case 9:x.publish_research_station_items=nullptr;break;case 10:x.cleanup_cocoon=nullptr;break;case 11:x.cleanup_construction=nullptr;break;}std::cout<<"missing_hybrid "<<i<<' '<<A2FO_RegisterHybridBridge(&x)<<'\n';}
 invoke(1);r.struct_size+=32;h.struct_size+=32;
 std::cout<<"register "<<A2FO_RegisterRefitQueueBridge(&r)<<','<<A2FO_RegisterHybridBridge(&h)<<'\n';
 // Publication copies function addresses; mutation of caller storage cannot change them.
 r.consume_synchronized_command=nullptr;h.has_queued_research_conflict=nullptr;
 for(unsigned i=0;i<200;++i){answer=i%2;invoke(i);}
 r.consume_synchronized_command=two;h.has_queued_research_conflict=two;
 std::cout<<"duplicate "<<A2FO_RegisterRefitQueueBridge(&r)<<','<<A2FO_RegisterHybridBridge(&h)<<'\n';
}
