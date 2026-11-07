// Differential scalar decisions copied from the stage-seven frozen modules.
// Native patching, Win32 reads and rendering are covered by their own fixtures.
#include "../rust/module_runtime_b_ffi.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

static bool old_api(bool present,unsigned v,unsigned expected,size_t n,size_t min,
                    bool log,bool image,bool patch,bool root) {
    return present && v==expected && n>=min && log && image && patch && root;
}
int main() {
    for (unsigned mask=0;mask<16;++mask)
      for (unsigned version=0;version<2;++version)
        for (unsigned size=0;size<2;++size) {
          const bool log=mask&1,image=mask&2,patch=mask&4,root=mask&8;
          assert(bool(a2fo_rs_radar_api_valid(1,version,1,size,1,log,image,patch,root))==
                 old_api(true,version,1,size,1,log,image,patch,root));
        }
    assert(!a2fo_rs_radar_api_valid(0,1,1,1,1,1,1,1,1));
    std::array<A2foRsRadarSeen,96> seen{};uint32_t count=0;
    const auto* object=reinterpret_cast<const void*>(uintptr_t(0x1234));
    assert(a2fo_rs_radar_mark_seen(seen.data(),&count,object,0)&&count==1);
    assert(!a2fo_rs_radar_mark_seen(seen.data(),&count,object,0)&&count==1);
    assert(a2fo_rs_radar_mark_seen(seen.data(),&count,object,1)&&count==2);
    for (unsigned i=2;i<96;++i)
      assert(a2fo_rs_radar_mark_seen(seen.data(),&count,reinterpret_cast<const void*>(uintptr_t(i+0x2000)),0));
    assert(count==96&&!a2fo_rs_radar_mark_seen(seen.data(),&count,nullptr,0));

    A2foRsRecoveryConfig c{1,1,1,1,1000,1000,16};
    for (const char* s: {"0","1","001","1000000","1000001","-1","+1","1x","4294967296"}) {
      uint32_t value=0xfeedbeef;
      const bool expected=std::strcmp(s,"1")==0||std::strcmp(s,"001")==0||
                          std::strcmp(s,"1000000")==0;
      assert(bool(a2fo_rs_recovery_parse_number(reinterpret_cast<const uint8_t*>(s),
                          std::strlen(s),1,1000000,&value))==expected);
    }
    assert(a2fo_rs_recovery_config_valid(&c));
    c.sample_every=0;assert(!a2fo_rs_recovery_config_valid(&c));
    std::puts("stage7 runtime-b scalar differential: PASS");
}
