#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../build/rust-migration-stage7/device-work/math_mock.hpp"
static unsigned short control_word() { unsigned short cw; __asm__ volatile("fnstcw %0":"=m"(cw)); return cw; }
static void precision(unsigned bits) {
  auto cw=control_word(); cw=(cw & ~0x0300u) | bits; __asm__ volatile("fldcw %0"::"m"(cw));
}
static std::uint32_t bits(float v) { std::uint32_t n; std::memcpy(&n,&v,4); return n; }
static float from_bits(std::uint32_t n) { float v;std::memcpy(&v,&n,4);return v; }
int main() {
  using namespace a2fo::renderer::game;
  const auto old=control_word();
  std::uint32_t seed=0x1f33b67du;
  for (unsigned pc : {0u,0x200u,0x300u}) {
    precision(pc);
    for (unsigned case_id=0;case_id<512;++case_id) {
      Matrix a,b,c;
      const std::uint32_t edge[16]={0u,0x80000000u,0x3f800000u,0xbf800000u,
          0x7f800000u,0xff800000u,0x7fc12345u,0x00000001u,
          0x00800000u,0x7f7fffffu,0x3f000000u,0x40000000u,
          0x00000000u,0x7fc00000u,0x00400000u,0x80800000u};
      for(unsigned i=0;i<16;++i) {
        seed=seed*1664525u+1013904223u;
        a.m[i]=from_bits(case_id<16 ? edge[(i+case_id)%16] : seed);
        seed=seed*1664525u+1013904223u;
        b.m[i]=from_bits(seed);
      }
      identity(c);
      multiply(c,a,b);
      std::printf("m:%u:%u",pc,case_id);
      for(float v:c.m)std::printf(":%08x",bits(v));
      std::printf("\n");
      multiply(a,a,b);
      std::printf("alias:%u:%u",pc,case_id);
      for(float v:a.m)std::printf(":%08x",bits(v));
      std::printf("\n");
    }
  }
  __asm__ volatile("fldcw %0"::"m"(old));
}
