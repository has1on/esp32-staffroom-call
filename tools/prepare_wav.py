"""Conservative PCM16 WAV conversion/composition. Original recording is never overwritten."""
import argparse,struct,wave,math,sys
from pathlib import Path

def read(path):
 with wave.open(str(path),'rb') as f:
  if f.getsampwidth()!=2 or f.getnchannels() not in (1,2) or f.getcomptype()!='NONE':raise ValueError('只支援PCM16 mono/stereo輸入；MP3、24-bit或浮點WAV請先用Audacity轉換')
  ch=f.getnchannels();rate=f.getframerate();raw=f.readframes(f.getnframes())
 values=struct.unpack('<'+'h'*(len(raw)//2),raw)
 samples=[round(sum(values[i:i+ch])/ch) for i in range(0,len(values),ch)]
 return samples,rate

def resample(values,old,new):
 if old==new:return values
 if new<old:raise ValueError('為避免簡易降採樣混疊失真，本工具不降採樣；先用Audacity高品質轉至16000或22050Hz')
 if not values:return []
 n=round(len(values)*new/old);out=[]
 for j in range(n):
  x=j*old/new;i=min(int(x),len(values)-1);k=min(i+1,len(values)-1);t=x-i
  out.append(round(values[i]*(1-t)+values[k]*t))
 return out

def compose(inputs,output,rate=16000,gap_ms=80):
 output=Path(output)
 if output.exists():raise ValueError('輸出已存在：為保護原錄音，拒絕覆蓋')
 audio=[]
 for index,p in enumerate(inputs):
  samples,old=read(p);samples=resample(samples,old,rate)
  if index:audio.extend([0]*round(rate*gap_ms/1000))
  audio.extend(samples)
 if not audio or len(audio)>rate*30:raise ValueError('輸出為空或超過30秒')
 output.parent.mkdir(parents=True,exist_ok=True)
 with wave.open(str(output),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(struct.pack('<'+'h'*len(audio),*audio))
 return len(audio)/rate
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('inputs',nargs='+',help='依序拼接：姓名.wav 同學找.wav');p.add_argument('--output',required=True);p.add_argument('--rate',type=int,choices=[16000,22050],default=16000);p.add_argument('--gap-ms',type=int,default=80);a=p.parse_args()
 try:
  if not 0<=a.gap_ms<=1000:raise ValueError('間隙需0-1000ms')
  print(f'已建立新檔，原檔不變；長度 {compose(a.inputs,a.output,a.rate,a.gap_ms):.2f}s。必須人工聽一次。')
 except (ValueError,OSError,wave.Error) as e:print('停止：',e);sys.exit(1)
