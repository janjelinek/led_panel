#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace matrix {
constexpr int N = 256;
// Conservative WS2812 model: 1 mA idle per pixel + 20 mA per full channel.
// This is an estimate, not a physical measurement or a hardware current limiter.
constexpr uint16_t IDLE_MA = N;
constexpr uint16_t DEFAULT_LIMIT_MA = 1000;
struct Settings {
  uint8_t mode = 0; // 0 solid, 1 drawing, 2 text, 3 clock
  uint8_t brightness = 16;
  uint8_t rotation = 0;
  bool serpentine = true;
  bool mirror = false;
  bool on = false;
  uint32_t color = 0x4DFFAA;
  uint16_t speed = 100;
  uint16_t limitMA = DEFAULT_LIMIT_MA;
};
struct Stats { uint16_t estimatedMA; uint16_t lit; uint8_t effectiveBrightness; bool limited; };
inline int physicalIndex(int x, int y, const Settings &s) {
  for (int r = 0; r < s.rotation; ++r) { int oldX = x; x = 15-y; y = oldX; }
  if (s.mirror) x = 15-x;
  return y*16 + (s.serpentine && (y&1) ? 15-x : x);
}
inline void pixel(uint32_t *f, int x, int y, uint32_t c) {
  if (x>=0 && x<16 && y>=0 && y<16) f[y*16+x]=c;
}
// Original compact 5x7 column glyphs: 0-9, A-Z, space and punctuation.
constexpr uint8_t glyphs[][5] = {
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},
 {0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
 {0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
 {0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},
 {0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},
 {0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,2,0x0c,2,0x7f},
 {0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},
 {0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
 {1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
 {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},
 {0,0,0,0,0},{0,0,0x5f,0,0},{2,1,0x51,9,6},{8,8,8,8,8},{0,0x60,0x60,0,0},
 {0,0x36,0x36,0,0},{8,8,0x3e,8,8},{0x20,0x10,8,4,2}
};
inline int glyphIndex(char c) {
  if(c>='0'&&c<='9') return c-'0';
  if(c>='a'&&c<='z') c-=32;
  if(c>='A'&&c<='Z') return c-'A'+10;
  const char *p=" !?-.:+/";
  for(int i=0;p[i];++i) if(c==p[i]) return 36+i;
  return 38;
}
inline void character(uint32_t *f, char c, int x, int y, uint32_t color) {
  const uint8_t *g=glyphs[glyphIndex(c)];
  for(int a=0;a<5;++a) for(int b=0;b<7;++b)
    if(g[a]&(1<<b)) pixel(f,x+a,y+b,color);
}
inline void textFrame(uint32_t *f,const char *text,uint32_t step,uint32_t color) {
  int width = strlen(text)*6;
  int x=16-int(step%uint32_t(width+16));
  for(size_t i=0;text[i];++i) character(f,text[i],x+int(i)*6,4,color);
}
inline void clockFrame(uint32_t *f,int hour,int minute,bool synced,uint32_t color) {
  if(!synced) { character(f,'-',2,4,color); character(f,'-',9,4,color); return; }
  character(f,'0'+hour/10,2,0,color); character(f,'0'+hour%10,8,0,color);
  character(f,'0'+minute/10,2,9,color); character(f,'0'+minute%10,8,9,color);
}
inline Stats limitFrame(const uint32_t *input,uint32_t *output,const Settings &s) {
  uint32_t sum=0;
  for(int i=0;i<N;++i) sum+=((input[i]>>16)&255)+((input[i]>>8)&255)+(input[i]&255);
  uint32_t desired = s.on ? uint32_t(s.brightness)*257 : 0;
  uint32_t allowedSum = s.limitMA>IDLE_MA ? uint32_t(s.limitMA-IDLE_MA)*255/20 : 0;
  uint32_t scale=desired;
  if(sum && uint64_t(sum)*scale > uint64_t(allowedSum)*65535)
    scale=uint64_t(allowedSum)*65535/sum;
  uint32_t actual=0; uint16_t lit=0;
  for(int i=0;i<N;++i) {
    uint8_t r=(((input[i]>>16)&255)*scale)/65535;
    uint8_t g=(((input[i]>>8)&255)*scale)/65535;
    uint8_t b=((input[i]&255)*scale)/65535;
    output[i]=(uint32_t(r)<<16)|(uint32_t(g)<<8)|b;
    actual+=r+g+b; if(output[i]) ++lit;
  }
  return {uint16_t(IDLE_MA+(actual*20+254)/255),lit,uint8_t(scale/257),scale<desired};
}
}
