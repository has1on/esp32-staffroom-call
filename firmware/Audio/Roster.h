#pragma once
#include <FS.h>
#include "CommonCore.h"
#include "NetworkValidation.h"
struct Person { String id, className, maskedName, displayName, audioFile; };
inline String cell(const std::vector<std::string>&r,int i){return i>=0&&size_t(i)<r.size()?String(r[i].c_str()):String();}
inline bool safeAudioPath(const String&p){
 if(!p.startsWith("/")||p.length()>100||!p.endsWith(".wav")||p.indexOf("..")>=0||p.indexOf('\\')>=0)return false;
 for(size_t i=0;i<p.length();i++){char c=p[i];if(!(isalnum((unsigned char)c)||c=='/'||c=='_'||c=='-'||c=='.'))return false;}return true;
}
inline bool findPerson(fs::FS&fs,const char*file,const char*key,const String&id,Person&out,String&error){
 File f=fs.open(file,FILE_READ);if(!f){error="roster_missing";return false;}
 std::vector<std::string> header,row;std::string err;
 if(!staffroom::csvRecord(f,header,err)){error="roster_invalid";return false;}
 staffroom::stripBom(header);int k=staffroom::column(header,key);
 if(k<0){error="roster_invalid";return false;}
 bool found=false;Person candidate;
 while(staffroom::csvRecord(f,row,err)){
  if(row.size()==1&&row[0].empty())continue;
  if(row.size()!=header.size()){error="roster_invalid";return false;}
  String rawId=cell(row,k);
  String matchId=key[0]=='s'?String(staffroom::studentUiId(rawId.c_str()).c_str()):rawId;
  if(key[0]=='s'&&matchId.isEmpty()){error="invalid_student_id";return false;}
  if(matchId!=id)continue;
  if(found){error=String(key[0]=='s'?"duplicate_student":"duplicate_teacher");return false;}
  candidate.id=id;candidate.className=cell(row,staffroom::column(header,"class"));
  candidate.maskedName=cell(row,staffroom::column(header,"maskedName"));
  candidate.displayName=cell(row,staffroom::column(header,"displayName"));
  candidate.audioFile=cell(row,staffroom::column(header,"audioFile"));
  if(candidate.audioFile.isEmpty())candidate.audioFile=String(key[0]=='s'?"/students/":"/teachers/")+rawId+".wav";
  if(!safeAudioPath(candidate.audioFile)){error="audio_path_invalid";return false;}
  found=true;
 }
 if(!err.empty()){error="roster_invalid";return false;}
 if(!found){error="id_not_found";return false;}
 out=candidate;return true;
}
inline bool loadTeachers(fs::FS&fs,Person*out,size_t&count,String&error){
 count=0;File f=fs.open("/teachers.csv",FILE_READ);if(!f){error="teachers_missing";return false;}
 std::vector<std::string>h,r;std::string e;
 if(!staffroom::csvRecord(f,h,e)){error="teachers_invalid";return false;}staffroom::stripBom(h);
 int id=staffroom::column(h,"teacherId"),name=staffroom::column(h,"displayName"),audio=staffroom::column(h,"audioFile");
 if(id<0||name<0){error="teachers_invalid";return false;}
 while(staffroom::csvRecord(f,r,e)){
  if(r.size()==1&&r[0].empty())continue;
  if(r.size()!=h.size()||count>=15||!staffroom::identifier(std::string(cell(r,id).c_str()))||cell(r,name).isEmpty()){error="teachers_invalid";return false;}
  for(size_t j=0;j<count;j++)if(out[j].id==cell(r,id)){error="duplicate_teacher";return false;}
  out[count].id=cell(r,id);out[count].displayName=cell(r,name);out[count].audioFile=cell(r,audio);
  if(out[count].audioFile.isEmpty())out[count].audioFile="/teachers/"+out[count].id+".wav";
  ++count;
 }
 if(!e.empty()||count==0){error="teachers_invalid";return false;}return true;
}
