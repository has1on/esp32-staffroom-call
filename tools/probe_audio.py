"""Authorized local diagnostics for AUDIO. Auth/key is prompted, not stored in command history."""
import argparse,getpass,json,urllib.request,urllib.error,uuid,sys
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('host',help='AUDIO IPv4 address');p.add_argument('--system',default='ROOM_A');p.add_argument('--student');p.add_argument('--teacher',default='01');p.add_argument('--request-id');p.add_argument('--expected-boot',help='重試原請求時，填原來bootId；不要在重啟後改用新bootId');p.add_argument('--status');a=p.parse_args()
 if a.request_id and not a.expected_boot:p.error('--request-id 重試必須同時提供原 --expected-boot，避免重啟後重播')
 import ipaddress
 host=str(ipaddress.IPv4Address(a.host));key=getpass.getpass('本套DEVICE_KEY（所有接口均必填）：')
 url='http://'+host+'/health';data=None
 if a.status:url='http://'+host+'/status?requestId='+a.status
 elif a.student:
  rid=a.request_id or uuid.uuid4().hex
  print('本次requestId:',rid,'（要測重試，下一次使用同一ID）')
  url='http://'+host+'/announce';
  try:health=json.load(urllib.request.urlopen(urllib.request.Request('http://'+host+'/health',headers={'X-Device-Key':key}),timeout=5))
  except (OSError,ValueError) as e:print('無法查詢設備，未送出：',e);sys.exit(1)
  if health.get('role')!='AUDIO' or health.get('ready') is not True or health.get('systemId')!=a.system or not health.get('bootId'):
   print('目标不是本套已就绪Audio，未送出：',health);sys.exit(1)
  boot=a.expected_boot or health['bootId'];print('expectedBootId:',boot,'（重試時保留此值）')
  data=json.dumps({'expectedBootId':boot,'systemId':a.system,'requestId':rid,'studentId':a.student,'teacherId':a.teacher}).encode()
 req=urllib.request.Request(url,data=data,headers={'Content-Type':'application/json','X-Device-Key':key})
 try:
  with urllib.request.urlopen(req,timeout=5) as r:print('HTTP',r.status);print(r.read().decode())
 except urllib.error.HTTPError as e:print('HTTP',e.code);print(e.read().decode());sys.exit(1)
 except OSError as e:print('連線失敗／結果未明。不要換requestId重送：',e);sys.exit(1)
if __name__=='__main__':main()
