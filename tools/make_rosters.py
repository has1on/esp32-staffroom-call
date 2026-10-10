"""Generate public door and private audio rosters from a school-managed master CSV."""
import argparse,csv,sys
from pathlib import Path
from validate_sd import ID, student_ui_id

def generate(master,teachers,output):
 def load(p):
  with Path(p).open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f,strict=True))
 students=load(master);ts=load(teachers)
 if not students or not 1<=len(ts)<=15:raise ValueError('學生名冊為空或老師數量超出1-15')
 for rows,key,required in [(students,'studentId',['class','displayName']),(ts,'teacherId',['displayName'])]:
  ids=set()
  for r in rows:
   rid=student_ui_id(r.get(key,'')) if key=='studentId' else r.get(key,'')
   if not ID.fullmatch(r.get(key,'')) or rid in ids or any(not r.get(n) for n in required):raise ValueError('編號非法、重複或缺欄位：'+str(r.get(key,'')))
   if r[key]!=r[key].upper():raise ValueError('請保留學校編號大小寫；學生只接受5位數字或E+5位數字')
   ids.add(rid)
 output=Path(output)
 # Exclusive creation prevents overwriting prepared rosters.
 output.mkdir(parents=True,exist_ok=False)
 for role in ('DOOR','AUDIO'):(output/role).mkdir()
 def write(path,fields,rows):
  with path.open('x',encoding='utf-8',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
 doors=[];audios=[]
 for r in students:
  name=r['displayName'];masked=r.get('maskedName') or (name[0]+'○'+name[-1] if len(name)>2 else name[0]+'○')
  public={'studentId':r['studentId'],'class':r['class'],'maskedName':masked};doors.append(public)
  audios.append({**public,'displayName':name,'audioFile':r.get('audioFile') or '/students/'+r['studentId']+'.wav'})
 write(output/'DOOR/students.csv',['studentId','class','maskedName'],doors)
 write(output/'AUDIO/students.csv',['studentId','class','maskedName','displayName','audioFile'],audios)
 write(output/'DOOR/teachers.csv',['teacherId','displayName'],[{'teacherId':r['teacherId'],'displayName':r['displayName']} for r in ts])
 write(output/'AUDIO/teachers.csv',['teacherId','displayName','audioFile'],[{'teacherId':r['teacherId'],'displayName':r['displayName'],'audioFile':r.get('audioFile') or '/teachers/'+r['teacherId']+'.wav'} for r in ts])
 return output
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('students');p.add_argument('teachers');p.add_argument('output');a=p.parse_args()
 try:print('已建立：',generate(a.students,a.teachers,a.output));print('只把DOOR內兩個CSV放門外卡；AUDIO含全名，要受管。')
 except (ValueError,OSError,csv.Error) as e:print('停止：',e);sys.exit(1)
