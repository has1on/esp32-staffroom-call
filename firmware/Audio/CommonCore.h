#pragma once
// Portable parsers shared by firmware and desktop C++ tests. No Arduino dependency.
#include <cstdint>
#include <string>
#include <vector>
#include <limits>
namespace staffroom {
inline bool identifier(const std::string &s, size_t max=24) {
 if(s.empty() || s.size()>max) return false;
 for(unsigned char c:s) if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'||c=='-'))return false;
 return true;
}
// RFC 4180-style records, including quoted commas, escaped quotes and quoted newlines.
// Reader contract: int read(), bool seek(uint32_t), uint32_t size(). EOF is -1.
template<class Reader> bool csvRecord(Reader &f,std::vector<std::string> &row,std::string &error) {
 row.clear(); std::string field; bool quoted=false,closed=false,started=false,seen=false;
 size_t bytes=0;
 while(true) {
  int v=f.read();
  if(v<0) { if(quoted){error="unterminated CSV quote";return false;} if(!seen)return false;row.push_back(field);return true; }
  seen=true; if(++bytes>2048){error="CSV record too long";return false;} char c=char(v);
  if(quoted) {
   if(c=='"'){quoted=false;closed=true;} else field+=c;
  } else if(closed) {
   if(c=='"'){field+='"';quoted=true;closed=false;}
   else if(c==','){row.push_back(field);field.clear();started=false;closed=false;}
   else if(c=='\n'){row.push_back(field);return true;}
   else if(c!='\r'){error="characters after CSV closing quote";return false;}
  } else {
   if(c=='"'){if(started){error="quote inside unquoted CSV field";return false;}quoted=true;started=true;}
   else if(c==','){row.push_back(field);field.clear();started=false;}
   else if(c=='\n'){row.push_back(field);return true;}
   else if(c!='\r'){field+=c;started=true;}
  }
 }
}
inline void stripBom(std::vector<std::string>&h){if(!h.empty()&&h[0].compare(0,3,"\xEF\xBB\xBF")==0)h[0].erase(0,3);}
inline int column(const std::vector<std::string>&h,const char*n){for(size_t i=0;i<h.size();++i)if(h[i]==n)return int(i);return -1;}
struct WavInfo { uint32_t sampleRate=0,dataOffset=0,dataBytes=0;uint16_t channels=0; };
inline uint16_t le16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t le32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
template<class R> bool exact(R&f,uint8_t*b,size_t n){for(size_t i=0;i<n;++i){int c=f.read();if(c<0)return false;b[i]=uint8_t(c);}return true;}
template<class R> bool wavInfo(R&f,WavInfo&w,std::string&error){
 uint8_t b[16]; bool fmt=false,data=false;w=WavInfo{};
 if(!f.seek(0)||!exact(f,b,12)||std::string((char*)b,4)!="RIFF"||std::string((char*)b+8,4)!="WAVE"){error="not RIFF/WAVE";return false;}
 const uint64_t end=uint64_t(le32(b+4))+8;
 if(end>f.size()||end<12){error="truncated RIFF";return false;}
 uint64_t pos=12;
 while(pos+8<=end){
  if(!f.seek(uint32_t(pos))||!exact(f,b,8)){error="truncated chunk";return false;}
  const std::string name((char*)b,4);uint32_t len=le32(b+4);uint64_t next=pos+8+len+(len&1);
  if(next>end){error="invalid chunk length";return false;}
  if(name=="fmt "){
   if(fmt||len<16||!exact(f,b,16)){error="invalid fmt chunk";return false;}
   w.channels=le16(b+2);w.sampleRate=le32(b+4);uint16_t align=le16(b+12);
   if(le16(b)!=1||le16(b+14)!=16||(w.channels!=1&&w.channels!=2)||(w.sampleRate!=16000&&w.sampleRate!=22050)||align!=w.channels*2||le32(b+8)!=w.sampleRate*align){error="requires PCM16 mono/stereo 16000/22050 Hz";return false;}
   fmt=true;
  }else if(name=="data"){
   if(data){error="multiple data chunks unsupported";return false;} w.dataOffset=uint32_t(pos+8);w.dataBytes=len;data=true;
  }
  pos=next;
 }
 if(!fmt||!data||!w.dataBytes||w.dataBytes%(w.channels*2)||w.dataBytes>uint64_t(w.sampleRate)*w.channels*2*30){error="missing/unaligned/overlong WAV data (max 30s)";return false;}
 return f.seek(w.dataOffset);
}
}
