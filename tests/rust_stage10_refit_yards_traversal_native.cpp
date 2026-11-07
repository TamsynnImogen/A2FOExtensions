// Included after the whole-source RefitYards native fixture. Mutates only the
// fixture's synthetic list/object fields from a synchronous visitor.
void stage10_refit_yards_traversal_tests(){
    auto reset=[](){
        g_armada=reinterpret_cast<HMODULE>(image);
        put_pointer(image+kGameObjectListPointerRva,0,object_list.data());
        put_pointer(object_list.data(),4,list_head.data());put_u32(object_list.data(),8,2);
        put_pointer(list_head.data(),0,list_first.data());
        put_pointer(list_first.data(),0,list_second.data());put_pointer(list_first.data(),8,yard_one.data());
        put_pointer(list_second.data(),0,list_head.data());put_pointer(list_second.data(),8,yard_two.data());
        yard_one[kObjectExpiredOffset]=yard_two[kObjectExpiredOffset]=source[kObjectExpiredOffset]=0;
    };
    for(unsigned mode=0;mode<8;++mode){
        reset();std::vector<void*> seen;
        if(mode==4)put_pointer(list_first.data(),0,list_first.data());
        if(mode==5)put_u32(object_list.data(),8,1);
        if(mode==6)put_u32(object_list.data(),8,kMaximumObjects+1);
        if(mode==7)put_pointer(object_list.data(),4,nullptr);
        for_each_live_object([&](void* object) noexcept {
            seen.push_back(object);
            if(object==yard_one.data()){
                if(mode==1)put_pointer(list_first.data(),0,list_head.data());
                if(mode==2)put_pointer(list_second.data(),8,source.data());
                if(mode==3)yard_two[kObjectExpiredOffset]=1;
            }
        });
        const std::vector<void*> wanted=mode>=6?std::vector<void*>{}:
            mode>=3?std::vector<void*>{yard_one.data()}:
            std::vector<void*>{yard_one.data(),mode==2?source.data():yard_two.data()};
        require(seen==wanted,"stage10 list successor/mutation/count guard");
    }
    std::puts("stage10 refit yards: eight live-list mutation/guard cases PASS");
}
