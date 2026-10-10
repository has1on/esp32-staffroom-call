#pragma once
// Local delta: the Audio browser console, kept in flash.
//
// This lives in a HEADER, not in Audio.ino, and that is deliberate. arduino-cli
// builds .ino function prototypes with a ctags-based TEXT scan that does not
// understand raw string literals: with this string inside Audio.ino it read the
// JavaScript below as C++ and injected prototypes like `function esc(s);` at
// global scope, failing with
//   Audio.ino:285:10: error: expected constructor, destructor, or type
//   conversion before '(' token
// Only .ino files are scanned, so a header is immune. Do not move it back.
#include <Arduino.h>

// Local delta: the whole panel, kept in flash so it costs no heap to hold.
static const char CONSOLE_HTML[] PROGMEM = R"AUDIOCONSOLE(<!doctype html>
<html lang="zh-Hant"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Staffroom Audio 控制台</title>
<style>
:root{--bg:#0f1216;--card:#181d24;--fg:#e8edf2;--mut:#8b98a5;--ok:#3ddc84;--bad:#ff5c5c;--warn:#ffc107;--acc:#3b82f6}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,"Microsoft JhengHei",sans-serif}
h1{font-size:18px;margin:0 0 12px}
.card{background:var(--card);border-radius:10px;padding:14px;margin-bottom:12px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(148px,1fr));gap:12px}
.k{color:var(--mut);font-size:12px}
.v{font-size:15px;font-weight:600;word-break:break-all}
button{background:var(--acc);color:#fff;border:0;border-radius:8px;padding:11px 14px;font-size:14px;margin:4px 6px 4px 0;cursor:pointer}
button.g{background:#2a323c}
button:disabled{opacity:.45;cursor:default}
table{width:100%;border-collapse:collapse;font-size:12px}
th,td{padding:5px 6px;text-align:left;border-bottom:1px solid #232a33}
th{color:var(--mut);font-weight:500}
.ok{color:var(--ok)}.bad{color:var(--bad)}.warn{color:var(--warn)}.mut{color:var(--mut)}
#out{white-space:pre-wrap;word-break:break-all;font-family:ui-monospace,Consolas,monospace;font-size:12px;color:var(--mut);max-height:170px;overflow:auto;margin:0}
input{background:#0c0f13;border:1px solid #2a323c;color:var(--fg);border-radius:8px;padding:11px;font-size:16px;width:100%}
select{background:#0c0f13;border:1px solid #2a323c;color:var(--fg);border-radius:8px;padding:11px;font-size:15px;width:100%}
label{display:block;color:var(--mut);font-size:12px;margin-bottom:6px}
#gate{max-width:340px;margin:44px auto}
</style></head><body>

<div id="gate">
  <div class="card">
    <h1>Staffroom Audio 控制台</h1>
    <label>管理員 PIN</label>
    <input id="pin" type="password" autocomplete="off" autocapitalize="characters" placeholder="CHANGE_ME">
    <div style="margin-top:12px"><button id="go" style="width:100%">進入</button></div>
    <div id="gateout" class="mut" style="margin-top:10px;font-size:12px"></div>
  </div>
</div>

<div id="main" style="display:none">
  <h1>Staffroom Audio 控制台<br><span id="live" class="mut" style="font-size:12px;font-weight:400"></span></h1>
  <div class="card"><div class="grid" id="stat"></div></div>
  <div class="card">
    <button id="bIp">朗讀本機 IP</button>
    <button id="bCall" class="g">測試叫號 12345 / 601</button>
    <button id="bPoll" class="g">暫停輪詢</button>
    <button id="bFull" class="g">顯示完整 JSON</button>
  </div>
  <div class="card"><pre id="out">&mdash;</pre></div>
  <div class="card">
    <div class="k" style="margin-bottom:8px">最近叫號（新 &rarr; 舊）</div>
    <table><thead><tr><th>時間</th><th>學號</th><th>老師</th><th>狀態</th><th>來自</th></tr></thead>
    <tbody id="log"><tr><td colspan="5" class="mut">尚無資料</td></tr></tbody></table>
  </div>

  <div class="card">
    <div class="k" style="margin-bottom:8px">網路設定 &mdash; 改 Audio 自己連的 Wi-Fi</div>
    <div id="netnow" class="mut" style="font-size:12px;margin-bottom:10px">&mdash;</div>
    <div id="netlock" class="warn" style="font-size:12px;margin-bottom:10px;display:none"></div>
    <div id="netform" style="display:none">
      <label>加密方式</label>
      <select id="nmode">
        <option value="0">一般 Wi-Fi（WPA2 個人 / 開放）</option>
        <option value="1">企業網路 PEAP（學校這種）</option>
        <option value="2">企業網路 TTLS</option>
      </select>
      <label style="margin-top:10px">Wi-Fi 名稱 (SSID)</label>
      <input id="nssid" autocomplete="off" placeholder="例如 MY-WIFI-2.4G">
      <div id="entfields" style="display:none">
        <label style="margin-top:10px">帳號 (username)</label>
        <input id="nuser" autocomplete="off" placeholder="例如 s1234567">
        <label style="margin-top:10px">身分 (identity，通常同帳號)</label>
        <input id="nident" autocomplete="off" placeholder="留空 = 用帳號">
      </div>
      <label style="margin-top:10px">密碼（每次都要重打，板子不外洩已存的密碼）</label>
      <input id="npass" type="password" autocomplete="new-password" placeholder="一般網路至少 8 碼；開放網路留空">
      <label style="margin-top:10px">Matrix 螢幕 IP（還沒有就留空）</label>
      <input id="nmatrix" autocomplete="off" placeholder="例如 192.168.4.2">
      <div style="margin-top:14px"><button id="bNet">套用並讓 Audio 重連</button></div>
      <div class="warn" style="font-size:12px;margin-top:10px">
        套用後 Audio 會離開現在的 Wi-Fi。但<b>熱點不會關閉</b>，所以就算名稱或密碼打錯，
        你只要重新整理這一頁就能再改一次，不會把板子變孤兒。
      </div>
    </div>
    <div id="netout" style="font-size:12px;margin-top:10px"></div>
  </div>
</div>

<script>
var TOKEN=localStorage.getItem('srToken')||'';
var POLL=3000, timer=null, SYS='', BOOT='', lastStat='', lastLogKey=null;
function $(s){return document.querySelector(s)}
function esc(s){return String(s==null?'':s).replace(/[&<>"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function dur(ms){if(ms<1000)return (ms|0)+'ms';var s=ms/1000;if(s<60)return s.toFixed(s<10?1:0)+'秒';var m=s/60;if(m<60)return m.toFixed(m<10?1:0)+'分';return (m/60).toFixed(1)+'時'}
function say(m,cls){var e=$('#out');e.textContent=m;e.className=cls||'mut'}

async function api(path,opt){
  opt=opt||{};
  opt.headers=Object.assign({'X-Device-Key':TOKEN},opt.headers||{});
  opt.cache='no-store';
  var r=await fetch(path,opt), t=await r.text(), j=null;
  try{j=JSON.parse(t)}catch(e){}
  return {status:r.status,json:j,text:t};
}
function post(path,body){
  return api(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
}

$('#go').onclick=unlock;
$('#pin').addEventListener('keydown',function(e){if(e.key==='Enter')unlock()});
async function unlock(){
  var pin=$('#pin').value.trim();
  if(!pin)return;
  $('#go').disabled=true;$('#gateout').textContent='檢查中…';
  try{
    var r=await post('/console-unlock',{pin:pin});
    var j=r.json||{};
    if(j.ok&&j.token){TOKEN=j.token;localStorage.setItem('srToken',TOKEN);start()}
    else if(j.error==='locked'){$('#gateout').textContent='錯誤太多次，請等 '+Math.ceil((j.retryMs||0)/1000)+' 秒'}
    else{$('#gateout').textContent='PIN 不正確'}
  }catch(e){$('#gateout').textContent='連不上板子：'+e}
  $('#go').disabled=false;
}
function lock(msg){
  localStorage.removeItem('srToken');TOKEN='';
  if(timer){clearInterval(timer);timer=null}
  $('#main').style.display='none';$('#gate').style.display='';
  $('#gateout').textContent=msg;
}
function start(){$('#gate').style.display='none';$('#main').style.display='';tick();timer=setInterval(tick,POLL)}

async function tick(){
  var d;
  try{d=await api('/diag')}catch(e){$('#live').textContent='（連線中斷，重試中…）';return}
  if(d.status===403){lock('工作階段已過期，請重新輸入 PIN');return}
  if(!d.json||!d.json.ok){$('#live').textContent='（板子回應 '+d.status+'，'+((d.json&&d.json.error)||'無資料')+'）';return}
  var j=d.json, up=j.uptimeMs||0;
  SYS=j.systemId||SYS;BOOT=j.bootId||BOOT;
  $('#live').textContent='每 '+(POLL/1000)+' 秒更新 &middot; 已開機 '+dur(up)+' &middot; '+(j.staMac||'');
  var st=[
    ['狀態',j.ready?'就緒':'異常',j.ready?'ok':'bad'],
    ['錯誤',j.error||'無',j.error?'bad':''],
    ['本機 IP',j.staIp||'未連線',j.staIp&&j.staIp!=='0.0.0.0'?'':'bad'],
    ['Wi-Fi',j.staSsid||'&mdash;',''],
    ['訊號',j.rssi+' dBm',''],
    ['熱點',j.apOn?('開啟 &middot; '+j.apIp):'關閉',j.apOn?'ok':''],
    ['熱點名稱',j.apSsid||'&mdash;',''],
    ['熱點裝置數',String(j.apClients|0),(j.apClients|0)>0?'ok':'warn'],
    ['佇列',(j.queued|0)+' &middot; 播放中 '+(j.playing?'是':'否'),''],
    ['網路語音檔',j.networkVoiceReady?'齊全':'缺失',j.networkVoiceReady?'ok':'bad'],
    ['可用記憶體',Math.round((j.freeHeap|0)/1024)+' KB',''],
    ['bootId',(j.bootId||'').slice(0,8),'']
  ];
  var html=st.map(function(x){return '<div><div class="k">'+esc(x[0])+'</div><div class="v '+x[2]+'">'+x[1]+'</div></div>'}).join('');
  if(html!==lastStat){$('#stat').innerHTML=html;lastStat=html}

  var rows=j.log||[], key=rows.length?rows[0].requestId+'/'+rows[0].state:'empty';
  if(key!==lastLogKey){
    lastLogKey=key;
    $('#log').innerHTML=rows.length?rows.map(function(r){
      return '<tr><td class="mut">'+(up>=r.at?dur(up-r.at)+'前':'&mdash;')+'</td><td>'+esc(r.studentId)+'</td><td>'+esc(r.line2||r.teacherId)+'</td>'
        +'<td class="'+(r.state==='done'?'ok':r.state==='failed'?'bad':'warn')+'">'+esc(r.state)+(r.error?' ('+esc(r.error)+')':'')+'</td>'
        +'<td class="mut">'+esc(r.sourceIp||'&mdash;')+'</td></tr>';
    }).join(''):'<tr><td colspan="5" class="mut">尚無資料</td></tr>';
  }
  netTickUi(j);
}

$('#bIp').onclick=async function(){
  say('朗讀 IP …');
  try{var r=await post('/say-ip',{systemId:SYS});var j=r.json||{};
    say('朗讀本機 IP → '+(j.ok?'已排入佇列，播放中就會念出來':'失敗：'+(j.error||r.status)),j.ok?'ok':'bad');
  }catch(e){say('失敗：'+e,'bad')}
};
$('#bCall').onclick=async function(){
  if(!BOOT){say('尚未取得 bootId，等第一次輪詢完成','bad');return}
  var id='WEB'+Date.now().toString(36).toUpperCase();
  say('送出叫號 '+id+' …');
  try{var r=await post('/announce',{systemId:SYS,requestId:id,studentId:'12345',teacherId:'01',expectedBootId:BOOT});var j=r.json||{};
    say('測試叫號 → '+(j.ok?'成功，佇列位置 '+j.queuePosition:'失敗：'+(j.error||r.status))
      +'\n'+JSON.stringify(j),j.ok?'ok':'bad');
    lastLogKey=null;tick();
  }catch(e){say('失敗：'+e,'bad')}
};
$('#bPoll').onclick=function(){
  if(timer){clearInterval(timer);timer=null;this.textContent='開始輪詢';say('已暫停輪詢')}
  else{tick();timer=setInterval(tick,POLL);this.textContent='暫停輪詢';say('開始輪詢（每 '+(POLL/1000)+' 秒）')}
};
$('#bFull').onclick=async function(){
  var d=await api('/diag');
  say(d.text||'（無回應）');
};

var APIP='', APSSID='', netPrefilled=false, netBusy=false;
function netsay(m,cls){var e=$('#netout');e.textContent=m;e.className=cls||'mut'}
function entToggle(){$('#entfields').style.display=(parseInt($('#nmode').value,10)?'':'none')}
$('#nmode').addEventListener('change',entToggle);

function netTickUi(j){
  APIP=j.apIp||APIP; APSSID=j.apSsid||APSSID;
  var modes=['一般 Wi-Fi','企業 PEAP','企業 TTLS'];
  $('#netnow').innerHTML='現在連線：<b>'+(j.staSsid?esc(j.staSsid):'未連線')+'</b>'
    +(j.netValid?' <span class="ok">設定有效</span>':' <span class="bad">設定無效</span>')
    +' &middot; IP '+(j.staIp||'&mdash;')
    +' &middot; '+(modes[j.netMode]||'&mdash;')
    +(j.netMatrix?' &middot; Matrix '+esc(j.netMatrix):' &middot; Matrix 未設');
  var onAp=!!APIP&&location.hostname===APIP;
  $('#netform').style.display=onAp?'':'none';
  $('#netlock').style.display=onAp?'none':'';
  if(!onAp){
    $('#netlock').innerHTML='只能在 Audio 熱點上改網路。請用手機連上 <b>'+esc(APSSID||'Staffroom-…-Audio')
      +'</b>（密碼 = DEVICE_KEY 前 12 碼），然後開 <b>http://'+esc(APIP||'192.168.4.1')+'/</b>。'
      +'（板子刻意不讓區網上的電腦改 Wi-Fi，免得被別人改走。）';
  }
  if(onAp&&!netPrefilled&&j.netSsid){
    netPrefilled=true;
    $('#nmode').value=String(j.netMode||0);
    $('#nssid').value=j.netSsid;
    $('#nmatrix').value=j.netMatrix||'';
    entToggle();
    netsay('已帶入目前設定；密碼請重打。');
  }
}

$('#bNet').onclick=async function(){
  if(netBusy)return;
  var mode=parseInt($('#nmode').value,10)||0;
  var ssid=$('#nssid').value.trim();
  var pass=$('#npass').value;
  var user=$('#nuser').value.trim();
  var ident=$('#nident').value.trim();
  var matrix=$('#nmatrix').value.trim();
  if(!ssid)return netsay('請填 Wi-Fi 名稱','bad');
  if(mode===0&&pass.length>0&&pass.length<8)return netsay('一般 Wi-Fi 的密碼至少 8 個字元','bad');
  if(mode!==0&&(!user||!pass))return netsay('企業網路需要帳號和密碼','bad');
  if(matrix&&!/^\d+\.\d+\.\d+\.\d+$/.test(matrix))return netsay('Matrix IP 格式不對','bad');
  if(!confirm('確定要把 Audio 改連到「'+ssid+'」嗎？\n套用後它會離開現在的 Wi-Fi。\n（熱點不會關，改錯還能救回來）'))return;
  netBusy=true;$('#bNet').disabled=true;netsay('套用中…請等 10~20 秒');
  try{
    var r=await post('/network',{version:1,systemId:SYS,mode:mode,ssid:ssid,password:pass,
      username:mode?user:'',identity:mode?(ident||user):'',audioHost:'',matrixHost:matrix});
    var j=r.json||{};
    if(j.ok){
      netsay('已寫入，Audio 正在重連 '+ssid+' …\n如果 30 秒後「本機 IP」還是沒變，代表名稱或密碼有誤。\n熱點仍在，重新整理本頁即可再改。','ok');
      netPrefilled=false;$('#npass').value='';
    } else {
      netsay('失敗：'+(j.error||('HTTP '+r.status))
        +(j.error==='audio_busy'?'（板上正在播號或排隊中，等它空閒再試）':'')
        +(j.error==='setup_ap_required'?'（你不是從熱點進來的）':'')
        +(j.error==='invalid_network_or_ca_missing'?'（欄位不合法：企業網路需要帳號+密碼，且本機允許無 CA）':''),'bad');
    }
  }catch(e){netsay('失敗：'+e,'bad')}
  netBusy=false;$('#bNet').disabled=false;
};

if(TOKEN){$('#gateout').textContent='使用上次的工作階段…';start()}
else{$('#gateout').textContent='PIN 預設 CHANGE_ME（與門口機的網路管理 PIN 相同）';$('#pin').focus()}
</script></body></html>
)AUDIOCONSOLE";
