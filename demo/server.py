"""Loopback-only preview; never exposes private files or talks to hardware."""
import argparse,csv,json,time,threading,re
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Preview:
 def __init__(self,capacity=8,play_seconds=3.5,clock=time.monotonic):
  self.capacity=capacity;self.play_seconds=play_seconds;self.clock=clock;self.lock=threading.Lock();self.queue=[];self.history={};self.current=None;self.playing=False;self.closed=threading.Event()
  with (ROOT/'sd_templates/AUDIO/students.csv').open(encoding='utf-8',newline='') as f:self.students={r['studentId']:r for r in csv.DictReader(f)}
  with (ROOT/'sd_templates/AUDIO/teachers.csv').open(encoding='utf-8',newline='') as f:self.teachers={r['teacherId']:r for r in csv.DictReader(f)}
 def submit(self,d):
  rid,sid,tid=(d.get(k,'') for k in ('requestId','studentId','teacherId'))
  if not all(isinstance(x,str) for x in (rid,sid,tid)) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}',rid):return 400,{'ok':False,'error':'非法請求'}
  with self.lock:
   now=self.clock();self.history={k:v for k,v in self.history.items() if now-v['at']<600 or v['state'] in ('queued','playing')}
   if rid in self.history:
    r=self.history[rid]
    if (r['studentId'],r['teacherId'])!=(sid,tid):return 409,{'ok':False,'error':'requestId與原資料不一致'}
    return 200,{'ok':True,'requestId':rid,'state':r['state'],'duplicate':True}
   if not re.fullmatch(r'[0-9]{5}',sid) or sid not in self.students or tid not in self.teachers:return 404,{'ok':False,'error':'找不到學生編號或老師'}
   if len(self.queue)+int(self.playing)>=self.capacity:return 429,{'ok':False,'error':'隊列已滿，請稍候'}
   if len(self.history)>=128:return 429,{'ok':False,'error':'紀錄已滿，請稍候'}
   for r in self.history.values():
    if (r['studentId'],r['teacherId'])==(sid,tid) and now-r['at']<30:return 429,{'ok':False,'error':'請等待30秒後再召喚'}
   self.history[rid]={'studentId':sid,'teacherId':tid,'at':now,'state':'queued'}
   self.queue.append({'requestId':rid,'line1':self.teachers[tid]['displayName'],'line2':self.students[sid]['displayName']+'同學找'})
   return 202,{'ok':True,'requestId':rid,'state':'queued','queuePosition':len(self.queue)}
 def snapshot(self):
  with self.lock:return {'ok':True,'queued':len(self.queue),'playing':self.playing,'current':self.current}
 def worker(self):
  while not self.closed.wait(.05):
   with self.lock:
    if not self.queue:continue
    item=self.queue.pop(0);self.current=item;self.playing=True;self.history[item['requestId']]['state']='playing'
   if self.closed.wait(self.play_seconds):return
   with self.lock:self.history[item['requestId']]['state']='done';self.playing=False

def create_server(port=8765,app=None):
 app=app or Preview()
 class Handler(BaseHTTPRequestHandler):
  def log_message(self,*args):pass # do not log student IDs
  def json(self,code,data):
   raw=json.dumps(data,ensure_ascii=False).encode();self.send_response(code);self.send_header('Content-Type','application/json; charset=utf-8');self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
  def do_GET(self):
   if self.path in ('/','/index.html'):
    raw=(ROOT/'demo/index.html').read_bytes();self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'");self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
   elif self.path=='/api/health':self.json(200,{'ok':True,'mode':'preview'})
   elif self.path=='/api/teachers':self.json(200,{'teachers':[{'teacherId':r['teacherId'],'displayName':r['displayName']} for r in app.teachers.values()]})
   elif self.path=='/api/state':self.json(200,app.snapshot())
   else:self.json(404,{'ok':False,'error':'not_found'})
  def do_POST(self):
   # Block cross-origin browser pages from injecting local demo requests.
   origin=self.headers.get('Origin');expected='http://'+self.headers.get('Host','')
   if origin and origin!=expected:self.json(403,{'ok':False,'error':'cross_origin_forbidden'});return
   try:
    length=int(self.headers.get('Content-Length','0'))
    if not 0<length<=1024:raise ValueError()
    d=json.loads(self.rfile.read(length));
    if not isinstance(d,dict):raise ValueError()
   except (ValueError,UnicodeError):self.json(400,{'ok':False,'error':'invalid_json'});return
   if self.path=='/api/lookup':
    sid=d.get('studentId','');s=app.students.get(sid) if isinstance(sid,str) and re.fullmatch(r'[0-9]{5}',sid) else None
    self.json(200,{k:s[k] for k in ('studentId','class','maskedName')}) if s else self.json(404,{'ok':False,'error':'找不到編號，請檢查輸入'})
   elif self.path=='/api/announce':code,data=app.submit(d);self.json(code,data)
   else:self.json(404,{'ok':False,'error':'not_found'})
 return ThreadingHTTPServer(('127.0.0.1',port),Handler),app
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',type=int,default=8765);a=p.parse_args();server,app=create_server(a.port);threading.Thread(target=app.worker,daemon=True).start()
 print(f'操作預覽：http://127.0.0.1:{a.port}/ （僅本機，Ctrl+C停止；不連實機）',flush=True)
 try:server.serve_forever()
 except KeyboardInterrupt:pass
 finally:app.closed.set();server.server_close()
