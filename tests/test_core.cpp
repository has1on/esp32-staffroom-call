#include "../firmware/CommonCore.h"
#include <cassert>
#include <iostream>
struct Memory{std::string bytes;uint32_t pos=0;int read(){return pos<bytes.size()?(unsigned char)bytes[pos++]:-1;}bool seek(uint32_t p){if(p>bytes.size())return false;pos=p;return true;}uint32_t size(){return bytes.size();}};
void u16(std::string&s,uint16_t n){s+=char(n);s+=char(n>>8);}void u32(std::string&s,uint32_t n){for(int i=0;i<4;i++)s+=char(n>>(8*i));}
Memory makeWav(int rate=16000,int channels=1,bool metadata=false){std::string body="WAVE";if(metadata){body+="JUNK";u32(body,3);body+="abc";body+=char(0);}body+="fmt ";u32(body,16);u16(body,1);u16(body,channels);u32(body,rate);u32(body,rate*channels*2);u16(body,channels*2);u16(body,16);body+="data";u32(body,8);body+=std::string(8,'\0');std::string s="RIFF";u32(s,body.size());s+=body;return {s};}
int main(){using namespace staffroom;int n=0;
 assert(identifier("000001"));++n;assert(!identifier("../x"));++n;
 Memory f{"\xEF\xBB\xBFstudentId,class,maskedName\r\n000001,\"2,A\",\"陳\"\"大\"\r\n"};std::vector<std::string>r;std::string e;
 assert(csvRecord(f,r,e));stripBom(r);assert(r[0]=="studentId");++n;assert(csvRecord(f,r,e));assert(r[0]=="000001"&&r[1]=="2,A"&&r[2]=="陳\"大");++n;
 Memory q{"\"one\nline\",x\n"};assert(csvRecord(q,r,e)&&r[0]=="one\nline");++n;
 Memory bad{"\"unfinished"};e.clear();assert(!csvRecord(bad,r,e)&&!e.empty());++n;
 Memory bad2{"abc\"x\n"};e.clear();assert(!csvRecord(bad2,r,e));++n;
 WavInfo w;auto a=makeWav();e.clear();assert(wavInfo(a,w,e)&&w.channels==1&&w.sampleRate==16000&&w.dataBytes==8);++n;
 auto b=makeWav(22050,2,true);assert(wavInfo(b,w,e)&&w.channels==2);++n;
 auto c=makeWav(44100);assert(!wavInfo(c,w,e));++n;
 auto d=makeWav();d.bytes.pop_back();assert(!wavInfo(d,w,e));++n;
 Memory fake{"ID3thisismp3"};assert(!wavInfo(fake,w,e));++n;
 auto g=makeWav();g.bytes[4]=(char)0xFF;assert(!wavInfo(g,w,e));++n;
 std::cout<<"Portable firmware parser checks: "<<n<<" PASS\n";
}
