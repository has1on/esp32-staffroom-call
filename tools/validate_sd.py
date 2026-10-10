"""Read-only TF card validation. No format, rename, overwrite, or upload actions."""
import argparse,csv,re,struct,json,sys
from pathlib import Path
ID=re.compile(r"^[A-Za-z0-9_-]{1,24}$")
def student_ui_id(value):
 if re.fullmatch(r"[0-9]{5}", value):return value
 if re.fullmatch(r"E[0-9]{5}", value):return value[1:]
 raise ValueError("學生編號只接受5位數字或既有E+5位數字："+value)

def read_csv(path,required,errors):
 try:
  with path.open(encoding="utf-8-sig",newline="") as f:
   reader=csv.DictReader(f,strict=True);h=reader.fieldnames or []
   if any(x not in h for x in required) or len(h)!=len(set(h)):
    errors.append(f"{path.name}: 缺少必要表頭或表頭重複，需要 {','.join(required)}");return []
   rows=[];seen=set()
   for i,r in enumerate(reader,2):
    if None in r or any(v is None for v in r.values()):errors.append(f"{path.name} 第{i}行: 欄位數不符");continue
    sid=r[required[0]]
    if not ID.fullmatch(sid):errors.append(f"{path.name} 第{i}行: 編號須為1-24個英數字、- 或 _，不可有空格")
    if sid!=sid.upper():errors.append(f'{path.name} 第{i}行: 門口鍵盤只輸入大寫字母，請核對學校編號規則')
    if sid in seen:errors.append(f"{path.name} 第{i}行: 重複編號 {sid}")
    seen.add(sid)
    if len(sid)>1 and sid.isdigit() and sid[0]!='0':pass # leading zeros cannot be inferred, never repair silently
    if len(','.join(r.values()).encode('utf-8'))>1900:errors.append(f"{path.name} 第{i}行: 記錄太長")
    rows.append(r)
   return rows
 except (OSError,UnicodeError,csv.Error) as e:errors.append(f"{path.name}: 無法讀取UTF-8 CSV ({e})");return []

def inspect_wav(path):
 with path.open('rb') as f:
  size=path.stat().st_size;head=f.read(12)
  if len(head)!=12 or head[:4]!=b'RIFF' or head[8:]!=b'WAVE':raise ValueError('不是 RIFF/WAVE；不能把 MP3 改名成 WAV')
  end=struct.unpack('<I',head[4:8])[0]+8
  if end>size or end<12:raise ValueError('RIFF長度不正確／檔案損壞')
  pos=12;fmt=None;data=None
  while pos+8<=end:
   f.seek(pos);h=f.read(8);kind=h[:4];n=struct.unpack('<I',h[4:])[0];nxt=pos+8+n+(n&1)
   if nxt>end:raise ValueError('WAV區塊長度越界')
   if kind==b'fmt ':
    if fmt is not None or n<16:raise ValueError('fmt區塊不正確')
    fmt=struct.unpack('<HHIIHH',f.read(16))
   elif kind==b'data':
    if data is not None:raise ValueError('不支援多個data區塊')
    data=n
   pos=nxt
  if fmt is None or not data:raise ValueError('缺少音訊格式／資料')
  code,ch,rate,bps,align,bits=fmt
  if code!=1 or bits!=16 or ch not in (1,2) or rate not in (16000,22050) or align!=ch*2 or bps!=rate*align:raise ValueError('需要 PCM16、mono或stereo、16000或22050Hz')
  if data%align or data>rate*align*30:raise ValueError('資料未對齊或錄音超過30秒')
  return {'sampleRate':rate,'channels':ch,'bits':bits,'seconds':round(data/(rate*align),3)}

def audio_path(root,row,prefix,errors):
 p=row.get('audioFile') or f"/{prefix}/{row['studentId' if prefix=='students' else 'teacherId']}.wav"
 if not re.fullmatch(r'/[A-Za-z0-9_./-]{1,99}',p) or '..' in p or not p.endswith('.wav'):
  errors.append(f"不安全／不支援的音訊路徑: {p}");return None
 candidate=root/p.lstrip('/')
 try:candidate.resolve().relative_to(root.resolve())
 except ValueError:errors.append('音訊路徑超出TF卡');return None
 return candidate

def validate(root,role='audio',door=None,chime=False,network_voice=False):
 root=Path(root);errors=[];warnings=[];files=[]
 if not root.is_dir():return {'ok':False,'errors':['TF卡／資料夾不存在'],'warnings':[],'audio':[]}
 students=read_csv(root/'students.csv',['studentId','class','maskedName']+(['displayName'] if role=='audio' else []),errors)
 teachers=read_csv(root/'teachers.csv',['teacherId','displayName'],errors)
 if role=='door':
  for filename,expected in [('students.csv',['studentId','class','maskedName']),('teachers.csv',['teacherId','displayName'])]:
   try:
    with (root/filename).open(encoding='utf-8-sig',newline='') as f:
     if next(csv.reader(f),[])!=expected:errors.append(filename+': 門口表頭及順序必須完全為 '+','.join(expected))
   except (OSError,UnicodeError,csv.Error):pass
 seen_students=set()
 for row in students:
  try:
   ui=student_ui_id(row.get('studentId',''))
   if ui in seen_students:errors.append('學生五位輸入衝突：'+ui)
   seen_students.add(ui)
  except ValueError as exc:errors.append(str(exc))
 if any(not s.get('class') for s in students):errors.append('學生班別class不可空白')
 if not students:errors.append('學生名冊為空')
 if not 1<=len(teachers)<=15:errors.append('老師數量必須介於1到15；本UI上限15')
 elif len(teachers)!=15:warnings.append(f'目前只有{len(teachers)}位老師；不足15不是故障')
 for s in students:
  if not s.get('maskedName'):errors.append(f"學生 {s.get('studentId')}: 缺少遮蔽姓名")
  if len(s.get('displayName','').encode('utf-8'))>90:errors.append(f"學生 {s.get('studentId')}: 顯示姓名超過90 UTF-8 bytes")
  if role=='audio' and not s.get('displayName'):errors.append(f"學生 {s.get('studentId')}: 缺少室內顯示姓名")
 for t in teachers:
  if not t.get('displayName'):errors.append(f"老師 {t.get('teacherId')}: 缺少顯示姓名")
  elif len(t['displayName'])>10:warnings.append(f"老師 {t.get('teacherId')}: 名稱較長，門外按鈕可能截斷；需調整UI或簡稱")
 if role=='door':
  if any(s.get('displayName') for s in students):errors.append('門外卡不應包含完整學生displayName')
  if (root/'students').exists():warnings.append('門外卡似乎包含學生录音資料夾，應移除敏感錄音（先備份）')
 if role=='audio':
  paths=[audio_path(root,r,'students',errors) for r in students]+[audio_path(root,r,'teachers',errors) for r in teachers]
  if chime:paths.append(root/'chime.wav')
  if network_voice:paths += [root/'network'/f'{n}.wav' for n in ['ip','dot',*map(str,range(10))]]
  for p in paths:
   if p is None:continue
   try:files.append({'file':str(p.relative_to(root)),**inspect_wav(p)})
   except (OSError,ValueError,struct.error) as e:errors.append(f'{p.relative_to(root)}: {e}')
  rates={f['sampleRate'] for f in files}
  if len(rates)>1:warnings.append('採樣率混合：本播放器逐段重設可播放，但建議統一減少切換間隙')
  if any(f['channels']==2 for f in files):warnings.append('部分為stereo：本播放器支援，原文建議mono可節省空間')
 if door:
  de=[];ds=read_csv(Path(door)/'students.csv',['studentId','class','maskedName'],de);dt=read_csv(Path(door)/'teachers.csv',['teacherId','displayName'],de);errors+=de
  if {r['studentId'] for r in ds}!={r['studentId'] for r in students}:errors.append('門外／語音端學生編號集合不一致')
  if {(r['teacherId'],r['displayName']) for r in dt}!={(r['teacherId'],r['displayName']) for r in teachers}:errors.append('門外／語音端老師編號或名稱不一致')
 return {'ok':not errors,'role':role,'studentCount':len(students),'teacherCount':len(teachers),'errors':errors,'warnings':warnings,'audio':files}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',help='TF卡根目錄，例如 E:\\');p.add_argument('--role',choices=['audio','door'],default='audio');p.add_argument('--door');p.add_argument('--chime',action='store_true');p.add_argument('--network-voice',action='store_true',help='check the 12 offline IP voice WAV files');p.add_argument('--json',action='store_true');a=p.parse_args()
 r=validate(a.root,a.role,a.door,a.chime,a.network_voice)
 if a.json:print(json.dumps(r,ensure_ascii=False,indent=2))
 else:
  print('PASS / 通過' if r['ok'] else 'FAIL / 不通過')
  print(f"名冊：學生{r.get('studentCount',0)}，老師{r.get('teacherCount',0)}；此工具不會修改TF卡")
  for x in r['errors']:print('錯誤：'+x)
  for x in r['warnings']:print('注意：'+x)
  if r['ok']:print('檔案結構通過不代表錄音讀音、接線或實機播放已通過。')
 return 0 if r['ok'] else 1
if __name__=='__main__':sys.exit(main())
