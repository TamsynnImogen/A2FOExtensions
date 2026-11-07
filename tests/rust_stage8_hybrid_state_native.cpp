#define main a2fo_hybrid_runtime_unused_main
#include "rust_stage8_hybrid_runtime_native.cpp"
#undef main
template<class T,std::size_t N>static void put(std::array<std::uint32_t,N>& object,std::size_t offset,T value){
    std::memcpy(reinterpret_cast<std::uint8_t*>(object.data())+offset,&value,sizeof(value));
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    InitializeCriticalSection(&a2fo::g_registry_lock);a2fo::g_registry_lock_ready=true;
    void* denied=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);if(!denied)return 2;
    std::array<std::uint32_t,0x500/4> producer{},producer_class{};
    std::array<std::array<std::uint32_t,0x500/4>,4> targets{};
    std::array<std::uint32_t,4> projects{{1,2,3,4}};
    for(unsigned i=0;i<4;++i)put(targets[i],a2fo::kClassProjectIdOffset,&projects[i]);
    put(producer,a2fo::kObjectClassOffset,producer_class.data());
    auto explicit_lists=std::make_unique<a2fo::HybridBuildLists>();
    explicit_lists->add(a2fo::ProductionMethod::yard,0,1);explicit_lists->add(a2fo::ProductionMethod::research,0,2);
    explicit_lists->add(a2fo::ProductionMethod::evolve,0,3);a2fo::g_class_lists[producer_class.data()]=std::move(explicit_lists);
    auto lists=std::make_unique<a2fo::RuntimeClassLists>();auto* runtime=lists.get();
    std::array<void*,a2fo::kRuntimeBuildListCapacity> legacy{};legacy[0]=targets[3].data();
    runtime->legacy_research=legacy.data();runtime->yard[0]=targets[0].data();runtime->research[0]=targets[1].data();
    runtime->has_yard=true;a2fo::g_runtime_class_lists[producer_class.data()]=std::move(lists);
    std::array<std::array<std::uintptr_t,4>,2> items{};
    for(unsigned target=0;target<6;++target)for(unsigned queue=0;queue<5;++queue){
        void* active=target<4?static_cast<void*>(targets[target].data()):target==4?nullptr:denied;
        put(producer,a2fo::kCurrentBuildClassOffset,active);
        items={};items[0][0]=reinterpret_cast<std::uintptr_t>(targets[queue%4].data());
        items[0][a2fo::kQueueItemNextOffset/sizeof(std::uintptr_t)]=queue==3?reinterpret_cast<std::uintptr_t>(items[0].data()):reinterpret_cast<std::uintptr_t>(items[1].data());
        items[1][0]=reinterpret_cast<std::uintptr_t>(targets[1].data());
        void* head=queue==0?nullptr:queue==4?denied:static_cast<void*>(items[0].data());put(producer,a2fo::kQueueHeadOffset,head);
        a2fo::ProductionMethod method=a2fo::ProductionMethod::construct;
        const bool resolved=a2fo::active_explicit_method(producer.data(),method);
        std::printf("active:%u:%u:%u:%u:%u:%u:%u:%u\n",target,queue,resolved,static_cast<unsigned>(method),
            a2fo::active_yard_job(producer.data()),a2fo::active_construct_job(producer.data()),
            a2fo::active_evolve_job(producer.data()),a2fo::has_evolution_barrier(producer.data()));
    }
    for(unsigned config=0;config<5;++config)for(unsigned target=0;target<6;++target){
        runtime->has_yard=config!=0;runtime->legacy_research=config==1?nullptr:legacy.data();
        runtime->research.fill(nullptr);if(config==2)runtime->research[0]=targets[3].data();
        put(producer,a2fo::kCurrentBuildClassOffset-sizeof(void*),config==3?runtime->yard.data():legacy.data());
        void* value=target<4?static_cast<void*>(targets[target].data()):target==4?nullptr:denied;
        std::printf("research:%u:%u:%u:%u:%u\n",config,target,a2fo::hybrid_research_target(producer.data(),value),
            a2fo::raw_list_contains(legacy.data(),value),a2fo::raw_list_contains(nullptr,value));
    }
    for(unsigned existing=0;existing<4;++existing)for(unsigned target=0;target<4;++target)
    for(std::int32_t level:{INT32_MIN,-1,0,1,INT32_MAX}){
        put(targets[existing],a2fo::kResearchPodClassFlagOffset,static_cast<std::uint8_t>(existing!=0));
        put(targets[target],a2fo::kResearchPodClassFlagOffset,static_cast<std::uint8_t>(target!=0));
        put(targets[existing],a2fo::kResearchPodFamilyOffset,static_cast<std::int32_t>(existing%2));
        put(targets[target],a2fo::kResearchPodFamilyOffset,static_cast<std::int32_t>(target%2));
        put(targets[existing],a2fo::kResearchPodLevelOffset,level);
        put(targets[target],a2fo::kResearchPodLevelOffset,static_cast<std::int32_t>(0));
        std::printf("conflict:%u:%u:%d:%u\n",existing,target,level,a2fo::research_classes_conflict(targets[existing].data(),targets[target].data()));
    }
    put(producer,a2fo::kQueueHeadOffset,static_cast<void*>(nullptr));
    for(unsigned yard=0;yard<2;++yard)for(unsigned active=0;active<3;++active)for(std::uint32_t count:{0u,9u,10u,11u,UINT32_MAX}){
        runtime->has_yard=yard!=0;put(producer,a2fo::kCurrentBuildClassOffset,active==0?nullptr:targets[active].data());put(producer,a2fo::kQueueCountOffset,count);
        std::printf("capacity:%u:%u:%u:%u:%u\n",yard,active,count,a2fo::hybrid_station_has_queue_capacity(producer.data()),a2fo::active_hybrid_queue(producer.data()));
    }
    std::array<std::uintptr_t,3> selection{};void* selected=producer.data();selection[0]=reinterpret_cast<std::uintptr_t>(&selected);
    for(unsigned policy=0;policy<4;++policy)for(std::int32_t count:{INT32_MIN,-1,0,1,2}){
        runtime->has_yard=(policy&1)!=0;runtime->has_explicit_research=(policy&2)!=0;runtime->legacy_research=nullptr;
        selection[2]=static_cast<std::uintptr_t>(count);
        std::printf("single:%u:%d:%u\n",policy,count,a2fo::single_hybrid_station(selection.data())==producer.data());
    }
    for(void* invalid:{static_cast<void*>(nullptr),denied}){
        a2fo::ProductionMethod method=a2fo::ProductionMethod::legacy;
        std::printf("invalid:%u:%u:%u:%u:%u:%u:%u\n",a2fo::active_explicit_method(invalid,method),a2fo::has_evolution_barrier(invalid),
            a2fo::hybrid_research_target(invalid,targets[0].data()),a2fo::research_classes_conflict(invalid,targets[0].data()),
            a2fo::hybrid_station_has_queue_capacity(invalid),a2fo::active_hybrid_queue(invalid),a2fo::single_hybrid_station(invalid)!=nullptr);
    }
    a2fo::g_class_lists.clear();a2fo::g_runtime_class_lists.clear();VirtualFree(denied,0,MEM_RELEASE);DeleteCriticalSection(&a2fo::g_registry_lock);
    std::puts("hybrid state fixture: PASS");return 0;
}
