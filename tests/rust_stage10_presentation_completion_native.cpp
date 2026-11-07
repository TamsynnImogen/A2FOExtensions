// Native std algorithms are independent oracles for the production iterator
// adapters. The same callbacks expose mutations and throwing move assignment.
#include "../rust/stage10_presentation_completion_ffi.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool okay,const char* label){if(!okay)throw std::runtime_error(label);}
std::vector<int>* active_trace{};
int throw_move=-1;
struct Item {
    int value{};
    Item()=default;
    explicit Item(int v):value(v){}
    Item(const Item&)=default;
    Item(Item&&)=default;
    Item& operator=(const Item&)=default;
    Item& operator=(Item&& other){
        if(active_trace)active_trace->push_back(100+other.value);
        if(other.value==throw_move)throw std::runtime_error("move");
        value=other.value;other.value=-99;return *this;
    }
};
struct Result {std::vector<int> trace,values;std::size_t size{};std::string error;
    bool operator==(const Result& b)const{return trace==b.trace&&values==b.values&&size==b.size&&error==b.error;}};
Result remove_case(bool rust,unsigned mask,int throwing){
    std::vector<Item> values;for(int i=0;i<8;++i)values.emplace_back(i);
    Result out;active_trace=&out.trace;throw_move=throwing;
    auto predicate=[&](const Item& item){
        out.trace.push_back(item.value);
        if(item.value==1)values[7].value=9;
        return item.value>=0&&(mask&(1u<<(unsigned(item.value)%8)))!=0;
    };
    try{
        auto end=rust?a2fo_stage10_presentation_remove(values.begin(),values.end(),predicate)
                     :std::remove_if(values.begin(),values.end(),predicate);
        out.size=static_cast<std::size_t>(end-values.begin());
    }catch(const std::runtime_error& e){out.error=e.what();}
    for(const auto& item:values)out.values.push_back(item.value);
    active_trace=nullptr;throw_move=-1;return out;
}
Result find_count_case(bool rust,bool count,int throwing){
    std::array<int,6> values{1,2,3,4,5,6};Result out;
    auto predicate=[&](int value){out.trace.push_back(value);if(value==2)values[4]=4;
        if(value==throwing){throw std::runtime_error("predicate");}
        return value==4;};
    try{
        if(count)out.size=rust?a2fo_stage10_presentation_count(values.begin(),values.end(),predicate)
                             :static_cast<std::size_t>(std::count_if(values.begin(),values.end(),predicate));
        else{auto i=rust?a2fo_stage10_presentation_find(values.begin(),values.end(),predicate)
                        :std::find_if(values.begin(),values.end(),predicate);out.size=std::size_t(i-values.begin());}
    }catch(const std::runtime_error& e){out.error=e.what();}
    out.values.assign(values.begin(),values.end());return out;
}
Result each_case(bool rust,int throwing){
    std::array<int,4> values{1,2,3,4};Result out;
    auto action=[&](int& item){out.trace.push_back(item);if(item==1)values[3]=8;
        if(item==throwing){throw std::runtime_error("action");}
        ++item;};
    try{
        if(rust)a2fo_stage10_presentation_each(values.begin(),values.end(),action);
        else for(auto& item:values)action(item);
    }catch(const std::runtime_error& e){out.error=e.what();}
    out.values.assign(values.begin(),values.end());return out;
}
}
int main(){
    for(unsigned mask=0;mask<256;++mask)for(int throwing:{-1,3,6,9})
        require(remove_case(false,mask,throwing)==remove_case(true,mask,throwing),"stable remove mutation/move failure");
    for(bool count:{false,true})for(int throwing:{-1,2,3,4,6})
        require(find_count_case(false,count,throwing)==find_count_case(true,count,throwing),"first match/count mutation/failure");
    for(int throwing:{-1,1,2,3,8})require(each_case(false,throwing)==each_case(true,throwing),"visit mutation/failure");
    std::uint32_t pixels[]{0x80000000u,0x7f123456u,0u,0xffffffffu};
    a2fo_rs_stage10_presentation_layer_alpha(pixels,4);
    require(pixels[0]==0&&pixels[1]==0xff123456u&&pixels[2]==0&&pixels[3]==0xffffffffu,"layer pixel rule");
    std::puts("presentation native helpers: 1039 mutation/failure cases and alpha PASS");
}
