#include "../led_web/matrix_logic.h"
#include <cassert>
#include <cstdio>
#include <random>
int main(){
 using namespace matrix;
 Settings s; s.on=true; s.brightness=255;
 uint32_t in[N],out[N];
 for(int i=0;i<N;i++) in[i]=0xffffff;
 auto a=limitFrame(in,out,s); assert(a.estimatedMA<=s.limitMA); assert(a.limited); assert(a.lit==256);
 s.on=false; a=limitFrame(in,out,s); assert(a.lit==0);
 std::mt19937 rng(21);
 for(int test=0;test<10000;test++) {
  s.on=true; s.brightness=rng()%256; s.limitMA=256+rng()%2745;
  for(int i=0;i<N;i++) in[i]=rng()&0xffffff;
  a=limitFrame(in,out,s); assert(a.estimatedMA<=s.limitMA);
  for(int i=0;i<N;i++) for(int c=0;c<3;c++) assert(((out[i]>>(8*c))&255)<=((in[i]>>(8*c))&255));
 }
 for(int r=0;r<4;r++)for(int z=0;z<2;z++)for(int m=0;m<2;m++) {
  s.rotation=r;s.serpentine=z;s.mirror=m;bool seen[N]={};
  for(int y=0;y<16;y++)for(int x=0;x<16;x++) {int i=physicalIndex(x,y,s);assert(i>=0&&i<N&&!seen[i]);seen[i]=true;}
 }
 struct {uint32_t before;uint32_t f[N];uint32_t after;} guarded={0x12345678,{},0xabcdef01};
 for(int i=0;i<500;i++){memset(guarded.f,0,sizeof(guarded.f));textFrame(guarded.f,"AHOJ 0123456789 !?-.:+/",i,0xffffff);}
 for(int h=0;h<24;h++)for(int m=0;m<60;m++)clockFrame(guarded.f,h,m,true,0xffaa00);
 assert(guarded.before==0x12345678&&guarded.after==0xabcdef01);
 puts("PASS: 10000 power-limit cases, all 16 wiring mappings, text clipping and all clock times");
}
