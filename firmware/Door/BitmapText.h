#pragma once
#include <Arduino.h>
#include "glyphs.h"
inline uint32_t utf8Next(const char*&s){
 unsigned char c=*s++;if(c<128)return c;uint32_t v;int n;
 if((c&0xE0)==0xC0){v=c&31;n=1;}else if((c&0xF0)==0xE0){v=c&15;n=2;}else if((c&0xF8)==0xF0){v=c&7;n=3;}else return '?';
 for(int i=0;i<n;i++){unsigned char a=*s;if(!a||(a&0xC0)!=0x80)return '?';++s;v=(v<<6)|(a&63);}return v;
}
inline int glyphIndex(uint32_t c){int lo=0,hi=GLYPH_COUNT-1;while(lo<=hi){int mid=(lo+hi)/2;uint32_t m=GLYPHS[mid].code;if(m==c)return mid;if(m<c)lo=mid+1;else hi=mid-1;}return -1;}
inline int textWidth(const String&s,int scale=1){const char*p=s.c_str();int w=0;while(*p){uint32_t c=utf8Next(p);int n=glyphIndex(c);w+=(n<0?GLYPH_SIZE:GLYPHS[n].advance)*scale;}return w;}
template<class Pixel> void bitmapText(const String&s,int x,int y,uint16_t color,Pixel pixel,int scale=1,int maxWidth=32767){
 const char*p=s.c_str();int start=x;
 while(*p){uint32_t c=utf8Next(p);int n=glyphIndex(c);int a=(n<0?GLYPH_SIZE:GLYPHS[n].advance)*scale;if(x+a-start>maxWidth)break;
  if(n<0){for(int i=0;i<GLYPH_SIZE*scale;i++){pixel(x+i,y,color);pixel(x+i,y+GLYPH_SIZE*scale-1,color);pixel(x,y+i,color);pixel(x+GLYPH_SIZE*scale-1,y+i,color);}}
  else {for(int yy=0;yy<GLYPH_SIZE;yy++)for(int xx=0;xx<GLYPH_SIZE;xx++){int bit=yy*GLYPH_SIZE+xx;if(GLYPHS[n].bits[bit/8]&(0x80>>(bit%8)))for(int sy=0;sy<scale;sy++)for(int sx=0;sx<scale;sx++)pixel(x+xx*scale+sx,y+yy*scale+sy,color);}}
  x+=a;
 }
}
