#include <cstdint>
#include <cstdio>
#include <cstring>
#include <float.h>
#include <limits>
#include "../rust/game_draw_ffi.h"

namespace frozen {bool inverse3(const float*,float*);}
static std::uint32_t bits(float v){std::uint32_t u;std::memcpy(&u,&v,4);return u;}
static float value(std::uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
static std::uint32_t seed=0x139ac471;
static std::uint32_t next(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(){
    const unsigned modes[]={_PC_24,_PC_53,_PC_64};
    unsigned mismatches=0;
    for(unsigned mode:modes){
        _controlfp(mode,_MCW_PC);
        for(int case_no=0;case_no<2048;++case_no){
            float matrix[16]{};
            for(float& v:matrix){
                const std::uint32_t raw=next();
                v=float(int(raw%20001)-10000)/1000.f;
            }
            if(case_no==0){matrix[0]=matrix[5]=matrix[10]=1;matrix[1]=matrix[2]=matrix[4]=matrix[6]=matrix[8]=matrix[9]=0;}
            if(case_no==1)matrix[0]=matrix[4]=matrix[8]=0;
            if(case_no==2)matrix[0]=value(0x7f800000);
            if(case_no==3)matrix[0]=value(0x7fc01234);
            if(case_no==4){matrix[0]=1e-6f;matrix[5]=1e-6f;matrix[10]=1;matrix[1]=matrix[2]=matrix[4]=matrix[6]=matrix[8]=matrix[9]=0;}
            if(case_no==5){matrix[0]=1e-7f;matrix[5]=1e-6f;matrix[10]=1;matrix[1]=matrix[2]=matrix[4]=matrix[6]=matrix[8]=matrix[9]=0;}
            float before[9],current[9];for(int i=0;i<9;++i)before[i]=current[i]=value(0x4f123456);
            const bool a=frozen::inverse3(matrix,before);
            const bool b=a2fo_draw_inverse3(matrix,current);
            bool equal=a==b;
            for(int i=0;i<9;++i)equal&=bits(before[i])==bits(current[i]);
            if(!equal){
                std::printf("inverse-mismatch|mode=%u|case=%d|a=%u|b=%u",mode,case_no,unsigned(a),unsigned(b));
                for(int i=0;i<9;++i)std::printf("|%08x:%08x",bits(before[i]),bits(current[i]));
                std::printf("\n");++mismatches;if(mismatches>=8)return 1;
            }
            if(a){
                float ca[3]{},cb[3]{};
                for(int col=0;col<3;++col)ca[col]=-(matrix[12]*before[col]+matrix[13]*before[3+col]+matrix[14]*before[6+col]);
                a2fo_draw_camera(matrix,before,cb);
                for(int i=0;i<3;++i)if(bits(ca[i])!=bits(cb[i])){
                    std::printf("camera-mismatch|mode=%u|case=%d|component=%d|%08x:%08x\n",mode,case_no,i,bits(ca[i]),bits(cb[i]));
                    ++mismatches;if(mismatches>=8)return 1;
                }
            }
        }
        std::printf("math-mode|%u|2048|mismatches=%u\n",mode,mismatches);
    }
    return mismatches?1:0;
}
