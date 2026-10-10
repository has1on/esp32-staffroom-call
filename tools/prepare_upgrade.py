"""Build a private upgrade copy, preserving the working install's config/pins.
Never modifies the source install, TF cards or recordings. No secret values printed.
"""
import argparse,getpass,json,re,secrets,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ROLES=('Door','Audio','Matrix')
def value(text,name,default=None):
 m=re.search(r'\b'+re.escape(name)+r'\s*=\s*("(?:\\.|[^"\\])*"|true|false|\d+)\s*;',text)
 if not m:
  if default is not None:return default
  raise ValueError('Missing required field: '+name)
 token=m[1]
 try:return json.loads(token)
 except ValueError:raise ValueError('Unsupported literal in '+name+'; configure it manually')
def prepare(source,output,pin,ca_file=None,allow_insecure=False):
 source=Path(source).resolve();output=Path(output).resolve()
 if output.exists():raise ValueError('Output already exists; use a new directory')
 if source==output or source in output.parents or output in source.parents:raise ValueError('Source and output must be separate, non-nested directories')
 if not re.fullmatch(r'[0-9]{6,12}',pin):raise ValueError('Admin PIN must be 6-12 digits')
 ca=Path(ca_file).read_text(encoding='utf-8-sig') if ca_file else ''
 if ca and ('-----BEGIN CERTIFICATE-----' not in ca or '-----END CERTIFICATE-----' not in ca):raise ValueError('CA must be a PEM certificate')
 plans=[];pairs=[]
 for role in ROLES:
  folder=source/'firmware'/role
  config=(folder/'config.h').read_text(encoding='utf-8-sig')
  key=value(config,'DEVICE_KEY');system=value(config,'SYSTEM_ID')
  if not isinstance(key,str) or len(key)<32 or not re.fullmatch(r'[A-Za-z0-9_-]{1,16}',system):raise ValueError(role+': source is not configured')
  pairs.append((system,key))
  defaults=folder/'NetworkDefaults.h';legacy=folder/'WifiJoin.h'
  wifi=defaults.read_text(encoding='utf-8-sig') if defaults.exists() else legacy.read_text(encoding='utf-8-sig') if legacy.exists() else ''
  enterprise=value(wifi,'WIFI_ENTERPRISE',False);method=value(wifi,'WIFI_EAP_METHOD',1)
  username=value(wifi,'WIFI_EAP_USERNAME','');identity=value(wifi,'WIFI_EAP_IDENTITY','')
  certificate=ca or value(wifi,'WIFI_EAP_CA','')
  if enterprise and (method not in (1,2) or not username):raise ValueError(role+': only configured PEAP / TTLS-MSCHAPv2 is supported')
  # Do NOT silently retain unsafe legacy no-CA behavior.
  if enterprise and not certificate and not allow_insecure:raise ValueError('Enterprise source has no server CA. Supply --ca school-ca.pem; only with school approval use --allow-insecure-eap')
  fields={'WIFI_ENTERPRISE':enterprise,'WIFI_EAP_METHOD':method,'WIFI_EAP_IDENTITY':identity,'WIFI_EAP_USERNAME':username,'WIFI_EAP_CA':certificate,'ALLOW_INSECURE_EAP':allow_insecure,'NETWORK_ADMIN_PIN':pin,'NETWORK_SETUP_WINDOW_MS':600000}
  text='#pragma once\n// PRIVATE CONFIGURATION. Do not publish this directory.\n'
  for name,val in fields.items():
   typ='bool' if isinstance(val,bool) else 'uint32_t' if isinstance(val,int) else 'const char *'
   text+='constexpr '+typ+' '+name+'='+json.dumps(val,ensure_ascii=False)+';\n'
  if role=='Door' and not re.search(r'\bMATRIX_HOST\s*=',config):
   audio=(source/'firmware/Audio/config.h').read_text(encoding='utf-8-sig')
   config+='\nconstexpr const char *MATRIX_HOST='+json.dumps(value(audio,'MATRIX_HOST'))+';\n'
  if role=='Door':
   for name,default in [('BANNER_TEXT','歡迎光臨'),('BANNER_PASSWORD',pin)]:
    if not re.search(r'\b'+name+r'\s*=',config):config+='\nconstexpr const char *'+name+'='+json.dumps(default,ensure_ascii=False)+';\n'
  plans.append((role,config,text))
 if len(set(pairs))!=1:raise ValueError('SYSTEM_ID / DEVICE_KEY do not match across roles; resolve before upgrading')
 # Only copy firmware and the font generator dependencies; never copy real CSV/WAV.
 output.mkdir(parents=True)
 shutil.copytree(ROOT/'firmware',output/'firmware',ignore=shutil.ignore_patterns('*.bak','*.pending','__pycache__'))
 (output/'tools').mkdir();shutil.copy2(ROOT/'tools/generate_fonts.py',output/'tools/generate_fonts.py')
 (output/'assets').mkdir()
 for name in ('NotoSansCJKsc-Regular.otf','FONT-LICENSE.txt'):
  shutil.copy2(ROOT/'assets'/name,output/'assets'/name)
 shutil.copy2(ROOT/'requirements.txt',output/'requirements.txt')
 for role,config,defaults in plans:
  (output/'firmware'/role/'config.h').write_text(config,encoding='utf8',newline='\n')
  (output/'firmware'/role/'NetworkDefaults.h').write_text(defaults,encoding='utf8',newline='\n')
 # Keep all pre-existing Chinese roster glyphs and add new UI glyphs.
 for role in ('Door','Matrix'):
  old=source/'firmware'/role/'glyphs.h';new=output/'firmware'/role/'glyphs.h'
  if old.exists():
   oldtext=old.read_text(encoding='utf8');text=new.read_text(encoding='utf8')
   def entries(s):return {int(m[1]):m[0] for m in re.finditer(r'^\{(\d+),[^\n]+\}',s,re.M)}
   combined=entries(oldtext);combined.update(entries(text))
   # Existing roster bitmap shapes remain intact unless the same glyph is in the new UI.
   start=text.index('static const Glyph GLYPHS[]={\n');end=text.index('\n};',start)
   text=text[:start]+'static const Glyph GLYPHS[]={\n'+',\n'.join(combined[k].rstrip(',') for k in sorted(combined))+text[end:]
   new.write_text(text,encoding='utf8',newline='\n')
 (output/'PRIVATE_UPGRADE_README.txt').write_text('PRIVATE: retains real Wi-Fi settings, device key, pin maps and hardware confirmation flags. Do not share.\nUpload Door/Audio/Matrix from firmware. No original files were modified.\nAdmin PIN is in NetworkDefaults.h. NVS saved network overrides these compile-time defaults.\nFont updates: use tools/generate_fonts.py with explicit --students and --teachers paths to your approved AUDIO CSV backups; bundled defaults are NOT real rosters. Font license: assets/FONT-LICENSE.txt.\n'+('WARNING: server CA verification explicitly disabled. Use only on an approved isolated network.\n' if allow_insecure else 'Enterprise requires the installed server CA.\n'),encoding='utf8')
 return output

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',help='configured installation folder containing firmware');p.add_argument('output',help='new private output folder');p.add_argument('--ca');p.add_argument('--allow-insecure-eap',action='store_true');a=p.parse_args()
 if a.allow_insecure_eap:
  print('WARNING: no server identity validation may expose school credentials to a spoofed AP.')
  if input('School-approved exception: type ALLOW to continue: ')!='ALLOW':raise ValueError('Cancelled, nothing written')
 pin=getpass.getpass('New network admin PIN (6-12 digits, hidden): ')
 if pin!=getpass.getpass('Repeat PIN: '):raise ValueError('PIN mismatch')
 print('Created private upgrade:',prepare(a.source,a.output,pin,a.ca,a.allow_insecure_eap))
 print('No original config, TF card or audio files were modified. Do not publish the output.')
if __name__=='__main__':
 try:main()
 except (ValueError,OSError) as exc:print('Stopped:',exc);sys.exit(1)
