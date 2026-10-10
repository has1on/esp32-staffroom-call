#pragma once
#include <string>
namespace staffroom {
inline bool studentId5(const std::string &s) {
 if(s.size()!=5)return false;
 for(char c:s)if(c<'0'||c>'9')return false;
 return true;
}
inline std::string studentUiId(const std::string &s) {
 if(studentId5(s))return s;
 if(s.size()==6&&s[0]=='E'&&studentId5(s.substr(1)))return s.substr(1);
 return "";
}
inline bool unicastIPv4(const std::string &s) {
 unsigned oct[4]={};size_t start=0;
 for(int i=0;i<4;i++) {
  size_t end=s.find('.',start);if(i==3)end=s.size();
  if(end==std::string::npos||end==start||end-start>3)return false;
  if(end-start>1&&s[start]=='0')return false;
  for(size_t n=start;n<end;n++){if(s[n]<'0'||s[n]>'9')return false;oct[i]=oct[i]*10+s[n]-'0';}
  if(oct[i]>255)return false;
  start=end+1;
 }
 if(start!=s.size()+1)return false;
 return oct[0]>0&&oct[0]<224&&oct[0]!=127&&!(oct[0]==169&&oct[1]==254);
}
inline bool validNetworkFields(const std::string &ssid,const std::string &pass,
 unsigned mode,const std::string &user,const std::string &identity,
 bool haveCA,bool allowInsecure) {
 if(ssid.empty()||ssid=="CHANGE_ME"||ssid.size()>32||pass.size()>128||user.size()>128||identity.size()>128||mode>2)return false;
 if(mode==0){if(pass.empty())return true;if(pass.size()>=8&&pass.size()<=63)return true;
  if(pass.size()!=64)return false;
  for(char c:pass)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
  return true;}
 return !user.empty()&&!pass.empty()&&(haveCA||allowInsecure);
}
}
