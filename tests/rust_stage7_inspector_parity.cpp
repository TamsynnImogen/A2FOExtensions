// Full frozen/current inspector workflow with synthetic Delphi rows. No HWND.
#include <windows.h>
int WINAPI inspector_message(HWND,LPCSTR,LPCSTR,UINT);
#define MessageBoxA inspector_message
#include "../modules/A2FOCraftIdentity/fleetops_object_editor.cpp"
#undef MessageBoxA
#include <iostream>
#include <vector>
#include <string>
namespace n=a2fo::object_editor::fleetops;
namespace ed=a2fo::object_editor;
namespace {
std::uint8_t* image{};
struct Row {std::array<std::uint8_t,128> data{};std::array<void*,24> table{};std::array<std::uint8_t,32> descriptor{};std::vector<std::uint8_t> wide;std::wstring text;bool checked{};};
std::array<Row,12> rows;
std::array<std::uint8_t,128> craft{};
ed::SavedState initial;
unsigned adds{},hooks{},applies{};
int fail_hook=-1,missing_gateway=-1,fail_row=-1,fail_apply=-1;
bool read_ok=true,shields=true;
void* form=reinterpret_cast<void*>(0x1111);void* parent=reinterpret_cast<void*>(0x2222);
template<class T>void put(void* p,std::size_t at,T value){std::memcpy(static_cast<std::uint8_t*>(p)+at,&value,sizeof value);}
void log(const char*,const char* text){std::cout<<"log:"<<text<<'\n';}
bool install(void* target,void*,std::size_t,const std::uint8_t*,A2FO_InlineHook* hook){
 unsigned i=hooks++;std::cout<<"hook:"<<i<<':'<<(static_cast<std::uint8_t*>(target)-image)<<'\n';
 if(int(i)==fail_hook)return false;
 hook->gateway=int(i)==missing_gateway?nullptr:image+0x1000+i;return true;
}
void reset(){
 adds=hooks=applies=0;fail_hook=missing_gateway=fail_row=fail_apply=-1;read_ok=shields=true;
 n::g_ready=false;n::g_build_hook={};n::g_okay_hook={};n::g_destroy_hook={};n::g_session={};
 initial=ed::SavedState{};initial.flags=7;initial.current={20,30,40,50};initial.maximum={100,100,100,100};
 std::strcpy(initial.captain.data(),"Old captain");std::strcpy(initial.registry.data(),"Old registry");
 for(unsigned i=0;i<12;++i){rows[i].data.fill(0);rows[i].table.fill(nullptr);rows[i].descriptor.fill(0);rows[i].wide.clear();rows[i].checked=false;rows[i].text=i==0?L"New captain":i==1?L"Old registry":i%2?L"100":L"25";
  put(rows[i].data.data(),0,rows[i].table.data());rows[i].table[0x48/sizeof(void*)]=image+0x100;rows[i].table[0x38/sizeof(void*)]=image+0x101;put(rows[i].descriptor.data(),8,rows[i].data.data());}
 put(craft.data(),0x28,std::uint32_t(99));put(craft.data(),0x40,image+0x200);put(image,0x2447f8,craft.data());
 std::memcpy(image+0x1b90d8,n::kBuildSignature.data(),n::kBuildSignature.size());std::memcpy(image+0x1baf34,n::kOkaySignature.data(),n::kOkaySignature.size());std::memcpy(image+0x1b88c8,n::kDestroySignature.data(),n::kDestroySignature.size());std::memcpy(image+0x1b8904,n::kPropertySignature.data(),n::kPropertySignature.size());std::memcpy(image+0x1bacc0,n::kApplySignature.data(),n::kApplySignature.size());std::memcpy(image+0x1baf39,n::kOkayApplyCall.data(),n::kOkayApplyCall.size());std::memcpy(image+0x1baf40,n::kOkayCloseCall.data(),n::kOkayCloseCall.size());
}
}
int WINAPI inspector_message(HWND,LPCSTR text,LPCSTR,UINT){std::cout<<"warning:"<<text<<'\n';return IDOK;}
extern "C" std::uintptr_t __cdecl a2fo_identity_call_delphi_2(void* function,std::uintptr_t self,std::uintptr_t argument){
 if(function==image+0x5f38){*reinterpret_cast<wchar_t**>(self)=nullptr;std::cout<<"wide-clear\n";return 0;}
 if(function==image+0x100||function==image+0x101){
  for(unsigned i=0;i<12;++i)if(self==reinterpret_cast<std::uintptr_t>(rows[i].data.data())){
   if(function==image+0x101){std::cout<<"checked:"<<i<<'\n';return rows[i].checked;}
   auto& row=rows[i];row.wide.resize(4+(row.text.size()+1)*sizeof(wchar_t));put(row.wide.data(),0,std::uint32_t(row.text.size()*sizeof(wchar_t)));std::memcpy(row.wide.data()+4,row.text.c_str(),(row.text.size()+1)*sizeof(wchar_t));*reinterpret_cast<wchar_t**>(argument)=reinterpret_cast<wchar_t*>(row.wide.data()+4);std::cout<<"text:"<<i<<'\n';return 0;
  }
 }
 std::cout<<"native:"<<(static_cast<std::uint8_t*>(function)-image)<<'\n';return 0;
}
extern "C" void* __cdecl a2fo_fo_editor_add_property(void*,void*,std::uint32_t type,const char* caption,void*,void*,std::uint32_t writable){unsigned i=adds++;std::cout<<"row:"<<i<<':'<<type<<':'<<caption<<':'<<writable<<'\n';return int(i)==fail_row?nullptr:rows[i].descriptor.data();}
extern "C" void a2fo_fo_editor_build_bridge(){}
extern "C" void a2fo_fo_editor_ok_bridge(){}
extern "C" void a2fo_fo_editor_destroy_bridge(){}
namespace a2fo::object_editor {
bool read_values(void* p,SavedState* state,bool* has_shields) noexcept {std::cout<<"snapshot:"<<(p==craft.data())<<'\n';*state=initial;*has_shields=shields;return read_ok;}
bool apply_values(void*,const SavedState& state,bool has_shields) noexcept {unsigned i=applies++;std::cout<<"apply:"<<i<<':'<<state.flags<<':'<<has_shields<<':'<<state.captain.data()<<':'<<state.registry.data();for(float f:state.current)std::cout<<':'<<f;std::cout<<'\n';return int(i)!=fail_apply;}
}
int main(){
 image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x260000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!image)return 2;
 A2FO_ModuleApi api{};api.log=log;api.install_inline_hook=install;
 for(int scenario=0;scenario<25;++scenario){reset();std::cout<<"case:"<<scenario<<'\n';
  if(scenario<3)fail_hook=scenario;else if(scenario<6)missing_gateway=scenario-3;
  else if(scenario==6)image[0x1baf40]^=1;
  if(scenario==7)std::cout<<"init-null:"<<n::initialize(nullptr,reinterpret_cast<HMODULE>(image))<<'\n';
  else std::cout<<"init:"<<n::initialize(&api,reinterpret_cast<HMODULE>(image))<<'\n';
  if(scenario<8)continue;
  if(scenario==8)shields=false;if(scenario==9)read_ok=false;if(scenario==10)fail_row=2;
  if(scenario==11)fail_row=8;
  n::build(form,parent);
  if(scenario==12)rows[0].text=std::wstring(600,L'x');
  if(scenario==13)rows[4].text=L"nan";
  if(scenario==14)rows[5].text=L"0";
  if(scenario==15)rows[2].checked=true;
  if(scenario==16)fail_apply=0;
  if(scenario==17)fail_apply=1;
  if(scenario==18)put(craft.data(),0x28,std::uint32_t(100));
  if(scenario==19)n::g_ready=false;
  if(scenario==20)rows[2].table[0x38/sizeof(void*)]=nullptr;
  if(scenario==21)rows[1].text=L"Changed registry";
  if(scenario==22)rows[3].checked=true;
  if(scenario==23)std::cout<<"repeat:"<<n::initialize(nullptr,nullptr)<<'\n';
  n::okay(form,parent);n::destroy(form,parent);
  std::cout<<"session-cleared:"<<(n::g_session.form==nullptr)<<'\n';
 }
 VirtualFree(image,0,MEM_RELEASE);
}
