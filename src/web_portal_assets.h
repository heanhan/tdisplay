#ifndef __WEB_PORTAL_ASSETS_H__
#define __WEB_PORTAL_ASSETS_H__

// ESP32 的 Arduino core 没有 avr/pgmspace.h；文件作用域的 const 数组本身就被链进
// .rodata（flash），WebServer 的 send_P 在这里等价于按长度直接写 flash 内容。

// WiFiManager 自带页面（/update、/info）的美化样式，通过 setCustomHeadElement 注入。
// 只改颜色与排版，不动它的表单结构，保证 POST 字段名不变。
const char PORTAL_NATIVE_CSS[] = R"CSS(
<style>
:root{color-scheme:light dark}
body{background:#f2f4f7;color:#1b2030;text-align:left;
  font-family:-apple-system,BlinkMacSystemFont,"PingFang SC","Microsoft YaHei",sans-serif}
.wrap{text-align:left;display:block;margin:16px auto;padding:18px;background:#fff;border:1px solid #e3e7ee;
  border-radius:16px;box-shadow:0 1px 2px rgba(16,24,40,.06),0 10px 26px rgba(16,24,40,.07)}
h1{font-size:19px;font-weight:650;margin:0 0 12px}
h3{font-size:15px;margin:16px 0 8px}
a{color:#2f6df6;font-weight:500}
button,input,select{border:1px solid #d5dbe5;border-radius:11px;padding:10px 12px;font-size:15px}
button{background:#2f6df6;color:#fff;border:0;font-weight:600;cursor:pointer}
button.D{background:#d93a30}
.msg{border:1px solid #e3e7ee;border-left:4px solid #5b6577;border-radius:11px;background:#f8fafc}
.msg.S{border-left-color:#12a150}.msg.D{border-left-color:#d93a30}
@media (prefers-color-scheme:dark){
body{background:#0f1218;color:#e7ebf2}
.wrap{background:#1a1f28;border-color:#2a313d;box-shadow:none}
a{color:#5b8cff}
input,select{background:#12161d;color:#e7ebf2;border-color:#2a313d}
.msg{background:#151a22;border-color:#2a313d}
}
</style>
)CSS";

// 主页面：自包含单文件，不引用任何外部字体 / CDN —— 配网时设备侧没有外网，外链会直接白屏。
// 所有来自 WiFi 扫描结果和 NVS 的动态文本一律用 textContent / .value 写入，不用 innerHTML 拼接，
// 避免热点名称里带 < > 造成脚本注入。
const char PORTAL_PAGE[] = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="format-detection" content="telephone=no">
<title>T-Display 配置</title>
<style>
:root{
  color-scheme:light dark;
  --bg:#f2f4f7; --card:#fff; --ink:#1b2030; --sub:#5b6577; --line:#e3e7ee;
  --acc:#2f6df6; --ok:#12a150; --warn:#c98a06; --bad:#d93a30;
  --chip:#eef2f9; --field:#f7f9fc; --sh:0 1px 2px rgba(16,24,40,.06),0 10px 26px rgba(16,24,40,.07);
}
@media (prefers-color-scheme:dark){:root{
  --bg:#0f1218; --card:#1a1f28; --ink:#e7ebf2; --sub:#9aa5b5; --line:#2a313d;
  --acc:#5b8cff; --ok:#3ecf8e; --warn:#e0b23c; --bad:#f0655c;
  --chip:#232a35; --field:#12161d; --sh:0 1px 2px rgba(0,0,0,.4);
}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);-webkit-text-size-adjust:100%;
  font:16px/1.5 -apple-system,BlinkMacSystemFont,"PingFang SC","Microsoft YaHei",sans-serif}
main{max-width:480px;margin:0 auto;padding:14px 14px 96px}
header{display:flex;align-items:center;gap:10px;padding:6px 2px 14px}
.logo{width:38px;height:38px;border-radius:11px;background:var(--card);border:1px solid var(--line);
  display:grid;place-items:center;font-size:20px;flex:none;box-shadow:var(--sh)}
h1{font-size:17px;font-weight:650;margin:0}
.ver{font-size:12px;color:var(--sub);margin-top:1px;word-break:break-all}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px;
  margin-bottom:14px;box-shadow:var(--sh)}
.card h2{font-size:15px;font-weight:650;margin:0 0 4px}
.desc{font-size:12.5px;color:var(--sub);margin:0 0 12px}
.chip{display:inline-flex;align-items:center;gap:6px;background:var(--chip);color:var(--sub);
  border-radius:999px;padding:3px 10px;font-size:12.5px;font-weight:550;flex:none}
.chip.ok{background:rgba(18,161,80,.14);color:var(--ok)}
.chip.bad{background:rgba(217,58,48,.14);color:var(--bad)}
.chip.warn{background:rgba(201,138,6,.16);color:var(--warn)}
label{display:block;font-size:13px;color:var(--sub);font-weight:550;margin:14px 0 6px}
input,textarea{width:100%;padding:11px 12px;font-size:15px;font-family:inherit;color:var(--ink);
  background:var(--field);border:1px solid var(--line);border-radius:11px;outline:none}
input:focus,textarea:focus{border-color:var(--acc);background:var(--card)}
input.bad,textarea.bad{border-color:var(--bad)}
textarea{min-height:104px;resize:vertical;font-family:ui-monospace,Menlo,Consolas,monospace;
  font-size:13px;line-height:1.45}
.err{font-size:12.5px;color:var(--bad);margin-top:6px;display:none}
.err.on{display:block}
.help{font-size:12px;color:var(--sub);margin-top:6px;line-height:1.55}
.help code{background:var(--chip);border-radius:5px;padding:1px 5px;font-size:11.5px}
.row{display:flex;gap:10px}.row>div{flex:1;min-width:0}
.radio{display:flex;gap:8px}
.radio b{flex:1;text-align:center;padding:9px;border:1px solid var(--line);border-radius:11px;
  background:var(--field);font-size:14px;font-weight:500;cursor:pointer;user-select:none}
.radio b.sel{border-color:var(--acc);background:rgba(47,109,246,.12);color:var(--acc);font-weight:600}
button{appearance:none;border:0;border-radius:11px;background:var(--acc);color:#fff;font-size:15px;
  font-weight:600;padding:11px 14px;cursor:pointer;font-family:inherit}
button:disabled{opacity:.45;cursor:not-allowed}
button.g{background:transparent;color:var(--acc);border:1px solid var(--line)}
button.d{background:transparent;color:var(--bad);border:1px solid var(--line)}
button.s{padding:7px 11px;font-size:13px;flex:none}
ul.wifi{list-style:none;margin:12px 0 0;padding:0}
ul.wifi li{display:flex;align-items:center;gap:10px;padding:12px 2px;border-top:1px solid var(--line)}
ul.wifi li:first-child{border-top:0}
.nm{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:550}
.nm.cur{color:var(--ok)}
.bars{display:flex;align-items:flex-end;gap:2px;height:15px;width:20px;flex:none}
.bars i{width:3px;background:var(--sub);border-radius:1px;opacity:.25}
.bars i:nth-child(1){height:5px}.bars i:nth-child(2){height:8px}
.bars i:nth-child(3){height:11px}.bars i:nth-child(4){height:15px}
.bars.l1 i:nth-child(-n+1),.bars.l2 i:nth-child(-n+2),.bars.l3 i:nth-child(-n+3),.bars.l4 i:nth-child(-n+4)
  {opacity:1;background:var(--ok)}
.lock{flex:none;font-size:12px;opacity:.5}
.pw{display:none;margin-top:12px;padding:12px;background:var(--field);border:1px solid var(--line);
  border-radius:12px}
.pw.on{display:block}
.pw .t{font-size:13.5px;font-weight:600;margin-bottom:8px;word-break:break-all}
.sho{display:flex;align-items:center;gap:6px;font-size:12.5px;color:var(--sub);margin-top:8px}
.sho input{width:auto;margin:0}
.acts{margin-top:10px;display:flex;gap:8px}
.acts button{flex:1}
.spin{display:inline-block;width:13px;height:13px;border:2px solid rgba(255,255,255,.35);
  border-top-color:#fff;border-radius:50%;animation:sp .8s linear infinite;vertical-align:-1px}
button.g .spin{border-color:var(--line);border-top-color:var(--acc)}
@keyframes sp{to{transform:rotate(360deg)}}
.bar{position:fixed;left:0;right:0;bottom:0;background:var(--card);border-top:1px solid var(--line);
  padding:10px 14px calc(10px + env(safe-area-inset-bottom));max-width:480px;margin:0 auto}
.bar button{width:100%}
#toast{position:fixed;left:50%;top:14px;transform:translate(-50%,-16px);background:var(--ink);
  color:var(--bg);padding:10px 16px;border-radius:12px;font-size:14px;font-weight:550;opacity:0;
  transition:.25s;pointer-events:none;max-width:88%;box-shadow:var(--sh);z-index:9}
#toast.on{opacity:.96;transform:translate(-50%,0)}
#toast.bad{background:var(--bad);color:#fff}
#toast.ok{background:var(--ok);color:#fff}
.muted{font-size:12.5px;color:var(--sub)}
.sep{height:1px;background:var(--line);margin:14px 0}
.pair{display:flex;align-items:center;justify-content:space-between;gap:12px}
.pair+.pair{margin-top:12px}
footer{text-align:center;font-size:11.5px;color:var(--sub);margin:14px 0 4px}
</style>
</head>
<body>
<main>
<header>
  <div class="logo">◐</div>
  <div style="flex:1;min-width:0">
    <h1>T-Display 墨水屏桌历</h1>
    <div class="ver" id="ver">正在连接设备…</div>
  </div>
  <span class="chip" id="chip">读取中</span>
</header>

<section class="card">
  <h2>WiFi 网络</h2>
  <p class="desc" id="netdesc">正在读取连接状态…</p>
  <div class="pair">
    <button class="g" id="btnScan">扫描附近网络</button>
    <span class="muted" id="scanHint"></span>
  </div>
  <ul class="wifi" id="nets"></ul>
  <div class="pw" id="pwbox">
    <div class="t" id="pwname"></div>
    <input id="mssid" maxlength="32" autocapitalize="none" autocomplete="off" placeholder="网络名称 SSID">
    <input id="mpass" type="password" maxlength="64" autocomplete="off" placeholder="密码，开放网络留空">
    <div class="sho"><input type="checkbox" id="show"><label for="show" style="margin:0">显示密码</label></div>
    <div class="acts">
      <button id="btnGo">连接</button>
      <button class="g" id="btnBack">取消</button>
    </div>
  </div>
  <div class="sep"></div>
  <div class="pair"><span class="muted">列表里没有我的网络</span><button class="g" id="btnManual">手动输入</button></div>
  <div class="pair"><span class="muted">清除已保存的账号，下次开机重新配网</span><button class="d" id="btnClear">清除</button></div>
</section>

<section class="card">
  <h2>天气服务</h2>
  <p class="desc">和风天气 QWeather。API Key 留空则不刷新天气，屏幕只显示日历。</p>
  <label for="host">专属 API Host</label>
  <input id="host" maxlength="64" autocapitalize="none" autocomplete="off" placeholder="xxxxxx.re.qweatherapi.com">
  <div class="err" id="ehost"></div>
  <div class="help">和风控制台「设置 → API 信息」里的 Web 服务 Host，形如 <code>xxxx.re.qweatherapi.com</code>；
    老版 Key 用 <code>api.qweather.com</code></div>
  <label for="key">API Key</label>
  <input id="key" maxlength="32" autocapitalize="none" autocomplete="off" placeholder="密钥">
  <div class="err" id="ekey"></div>
  <label>天气类型</label>
  <div class="radio" id="rtype"><b data-v="0">每日天气</b><b data-v="1">实时天气</b></div>
  <div class="help">每日：每天凌晨刷新一次，最省电。实时：每 2 小时刷新一次。</div>
  <label for="loc">位置 ID</label>
  <input id="loc" maxlength="64" autocapitalize="none" autocomplete="off" placeholder="101010100 或 116.41,39.92">
  <div class="err" id="eloc"></div>
  <div class="help">城市 ID，或「经度,纬度」（东经为正、北纬为正，英文逗号分隔）</div>
</section>

<section class="card">
  <h2>日历显示</h2>
  <p class="desc">留空的项不会出现在屏幕上。</p>
  <label>每周第一天</label>
  <div class="radio" id="rweek"><b data-v="0">周日</b><b data-v="1">周一</b></div>
  <div class="row">
    <div>
      <label for="cddlab">倒数日名称</label>
      <input id="cddlab" maxlength="10" placeholder="最多 4 个汉字">
      <div class="err" id="ecddlab"></div>
    </div>
    <div>
      <label for="cdddate">目标日期</label>
      <input id="cdddate" maxlength="8" inputmode="numeric" placeholder="20261231">
      <div class="err" id="ecdddate"></div>
    </div>
  </div>
  <label for="tags">日期 Tag</label>
  <input id="tags" maxlength="30" autocapitalize="none" autocomplete="off" placeholder="00000015b;00000312a">
  <div class="err" id="etags"></div>
  <div class="help">格式 <code>yyyyMMdd</code> 加图标字母 <code>a-d</code>：a 书签 / b 金钱 / c 笑脸 / d 警告。
    年份写 <code>0000</code> 表示每年，月份写 <code>00</code> 表示每月。最多 3 个，英文分号分隔。</div>
</section>

<section class="card">
  <h2>课程表</h2>
  <p class="desc">非空时，单击按键可在「月历 / 课程表」之间切换。</p>
  <label for="sch">课程表内容</label>
  <textarea id="sch" maxlength="4000" spellcheck="false" placeholder="430;一,语文,数学,体育;二,数学,英语,科学"></textarea>
  <div class="err" id="esch"></div>
  <div class="help">第 1 段是三位课数：<code>上午 / 下午 / 晚上</code> 各一位，如 <code>430</code>，不可省略。<br>
    之后每天一段，段首是星期，课程用英文逗号分隔，段间用英文分号分隔。<br>
    例：<code>430;一,语文,数学,体育;二,数学,英语</code></div>
</section>

<section class="card">
  <h2>系统</h2>
  <div class="pair"><span class="muted">保存参数后重启，校时、天气、刷屏会按新配置走一遍</span>
    <button class="g" id="btnReboot">重启</button></div>
  <div class="pair"><span class="muted">退出配置，设备按当前配置刷新并休眠</span>
    <button class="g" id="btnExit">退出</button></div>
  <div class="pair"><span class="muted">上传 .bin 升级固件（WiFiManager 原页面）</span>
    <button class="g" id="btnOta">OTA 升级</button></div>
</section>

<footer>配置热点 T-Display / password 仅供本机访问，用完请退出配置</footer>
</main>

<div class="bar"><button id="btnSave" disabled>保存参数</button></div>
<div id="toast"></div>

<script>
var $=function(i){return document.getElementById(i)};
var CT={};               // 当前状态
var ORIG='';             // 已保存参数的快照，用于脏检查
var PICK=null;           // {ssid, manual}
var pollTimer=null;

function toast(m,k){var t=$('toast');t.textContent=m;t.className='on'+(k?' '+k:'');
  clearTimeout(t._h);t._h=setTimeout(function(){t.className=''},2800)}
// UTF-8 字节数：NVS 按字节限制单值长度，前端的 maxlength 数的是字符，必须换算
function blen(s){return unescape(encodeURIComponent(s)).length}
function api(path,body,ms){
  var o={headers:{'Content-Type':'application/json'}};
  if(body!==undefined){o.method='POST';o.body=body}
  var ctl=new AbortController();o.signal=ctl.signal;
  var to=setTimeout(function(){ctl.abort()},ms||12000);
  var p=path;
  if(o.method!=='POST')p+=(p.indexOf('?')<0?'?t=':'&t=')+Date.now();
  return fetch(p,o).then(function(r){clearTimeout(to);
    if(!r.ok)throw new Error('HTTP '+r.status);return r.json()});
}
// 破坏性操作不用 confirm()：Android 的 captive webview 会吞掉模态框
function arm(btn,label,text){
  if(btn._armed){clearTimeout(btn._armed);btn._armed=null;btn.textContent=label;return true}
  btn.textContent=text;
  btn._armed=setTimeout(function(){btn._armed=null;btn.textContent=label},3000);
  return false;
}
function chip(t,c){var e=$('chip');e.textContent=t;e.className='chip'+(c?' '+c:'')}
function setRadio(g,v){var b=$(g).children;for(var i=0;i<b.length;i++)
  b[i].className=(''+b[i].getAttribute('data-v')===''+v)?'sel':''}
function getRadio(g){var b=$(g).children;for(var i=0;i<b.length;i++)
  if(b[i].className==='sel')return b[i].getAttribute('data-v');return '0'}

/* ---------- 校验：与固件 / 渲染端的边界规则保持一致 ---------- */
var FLDS=[
 {k:'host',in:'host',er:'ehost',ck:function(v){if(!v)return '';
   if(!/^[A-Za-z0-9.-]{4,64}$/.test(v)||v.indexOf('.')<0)return '不像合法域名，例：xxxxxx.re.qweatherapi.com';return ''}},
 {k:'key',in:'key',er:'ekey',ck:function(v){if(!v)return '';
   if(!/^[A-Za-z0-9_-]{1,32}$/.test(v))return '只能含字母、数字、下划线、连字符，最长 32';return ''}},
 {k:'loc',in:'loc',er:'eloc',ck:function(v){if(!v)return '';
   if(/^\d{1,15}$/.test(v))return '';
   if(/^-?\d{1,3}(\.\d{1,6})?,-?\d{1,2}(\.\d{1,6})?$/.test(v))return '';
   return '填城市 ID（纯数字）或「经度,纬度」，例：116.41,39.92'}},
 {k:'cdd_label',in:'cddlab',er:'ecddlab',ck:function(v){if(!v)return '';
   if(Array.from(v).length>10)return '名称最多 10 个字符，建议 4 个汉字以内';return ''}},
 {k:'cdd_date',in:'cdddate',er:'ecdddate',ck:function(v){if(!v)return '';
   if(!/^\d{8}$/.test(v))return '日期格式 yyyyMMdd，例：20261231';
   var m=+v.slice(4,6),d=+v.slice(6,8);
   if(m<1||m>12||d<1||d>31)return '月份或日期不在范围内';
   if(!$('cddlab').value.trim())return '倒数日要同时填名称和日期';return ''}},
 {k:'tags',in:'tags',er:'etags',ck:function(v){if(!v)return '';
   if(v.length>30)return '总长不能超过 30 字符';
   var p=v.split(';').map(function(x){return x.trim()}).filter(function(x){return x!==''});
   if(!p.length)return '日期 Tag 至少要有 1 项，或者留空';
   if(p.length>3)return '最多 3 个 Tag';
   for(var i=0;i<p.length;i++)if(!/^\d{8}[a-d]$/.test(p[i]))
     return '「'+p[i]+'」不合法：应为 8 位日期 + a/b/c/d';
   return ''}},
 {k:'schedule',in:'sch',er:'esch',ck:function(v){v=v.trim();if(!v)return '';
   var b=blen(v);
   if(b>3500)return '不能超过 3500 字节（当前 '+b+'，一个字占 3 字节）';
   if(!/^\d{3};/.test(v))return '必须以三位课数加 ; 开头，例：430;';
   var n=+v[0]+ +v[1]+ +v[2];
   if(n===0)return '上午/下午/晚上课数不能全为 0，否则课程表不渲染';
   var seg=v.slice(4).split(';').filter(function(x){return x.trim()!==''});
   if(!seg.length)return '课数之后至少要写一天的课程';
   if(seg.length>7)return '每日课程最多 7 段，超出的不显示';
   for(var i=0;i<seg.length;i++)if(seg[i].split(',').length<2)
     return '第 '+(i+1)+' 段只有星期没有课程';
   return ''}}
];
function collect(){var o={};FLDS.forEach(function(f){o[f.k]=$(f['in']).value.trim()});
  o.type=getRadio('rtype');o.week1st=getRadio('rweek');return o}
function validate(){var bad=0;FLDS.forEach(function(f){var m=f.ck($(f['in']).value.trim());
  var e=$(f.er);e.textContent=m;e.className=m?'err on':'err';$(f['in']).className=m?'bad':'';if(m)bad++});
  // 跨字段规则，与固件 paramsValidate 保持一致
  if($('key').value.trim()&&!$('host').value.trim()&&!$('ehost').textContent){
    $('ehost').textContent='填了 API Key 就必须填 Host';$('ehost').className='err on';
    $('host').className='bad';bad++}
  return bad===0}
function refreshSave(){var ok=validate();var cur=JSON.stringify(collect());
  var dirty=cur!==ORIG&&ORIG!=='';
  $('btnSave').disabled=!(ok&&dirty);
  $('btnSave').textContent=!ok?'请先修正标红的项':(dirty?'保存参数':'已保存');
}

/* ---------- 状态 ---------- */
function paintState(st){
  CT=st||{};
  $('ver').textContent='固件 '+(st.ver||'?')+' · '+(st.ap||'192.168.4.1');
  $('mssid').style.display=st.connected?'':'none';
  if(st.connected){
    chip('已连接','ok');
    $('netdesc').textContent='已连接 '+st.sta+'（'+st.ip+(st.rssi?'，'+st.rssi+' dBm':'')+'）。改完参数点下面的保存。';
    stopPoll();
  }else if(st.connecting===1){
    chip('连接中','warn');
    $('netdesc').textContent='正在连接 '+(st.attempt||'所选网络')+'，最长等待 '+st.timeout_left+' 秒…';
    startPoll();
  }else if(st.connecting===3){
    chip('连接失败','bad');stopPoll();
    $('netdesc').textContent='连不上「'+(st.attempt||'')+'」。多为密码错误，或该网络是 5GHz / 隐藏 SSID。';
    toast('连接失败，请检查密码','bad');
  }else{
    chip(st.saved?'未连接':'配置模式',st.saved?'warn':'');
    $('netdesc').textContent=st.saved
      ?('已保存的网络：'+st.saved+'，当前没连上。可以直接点下面的同名网络重连。')
      :'还没有保存任何 WiFi 账号。扫描并选择你的 2.4GHz 网络。';
  }
  $('scanHint').textContent=st.saved?'已保存 '+st.saved:'';
}
function startPoll(){if(pollTimer)return;
  pollTimer=setInterval(function(){api('/api/state').then(paintState).catch(function(){})},1500)}
function stopPoll(){if(pollTimer){clearInterval(pollTimer);pollTimer=null}}

/* ---------- 扫描 ---------- */
function mkBars(r){var d=document.createElement('div');
  var l=r>=-55?4:r>=-65?3:r>=-75?2:r>=-85?1:0;d.className='bars'+(l?' l'+l:'');
  for(var i=0;i<4;i++)d.appendChild(document.createElement('i'));return d}
function renderNets(list){
  var ul=$('nets');ul.textContent='';
  if(!list||!list.length){var li=document.createElement('li');li.className='muted';
    li.textContent='没有找到网络。本设备只支持 2.4GHz，请确认路由器已开启该频段。';
    ul.appendChild(li);return}
  list.forEach(function(n){
    var li=document.createElement('li');
    li.appendChild(mkBars(n.r));
    var a=document.createElement('div');a.className='nm';a.textContent=n.s;
    if(n.s===CT.sta)a.className='nm cur';
    li.appendChild(a);
    if(n.o){var k=document.createElement('div');k.className='lock';k.textContent='🔒';li.appendChild(k)}
    var b=document.createElement('button');b.className='g s';b.textContent=n.s===CT.sta?'重连':'选择';
    b.onclick=function(){openPw(n.s,false)};
    li.appendChild(b);
    ul.appendChild(li)});
}
function openPw(ssid,manual){
  PICK={ssid:ssid||'',manual:!!manual};
  $('pwbox').className='pw on';
  $('pwname').textContent=manual?'手动填写网络信息':ssid;
  $('mssid').style.display=manual?'':'none';
  $('mssid').value=manual?'':'';
  $('mpass').value='';$('mpass').type='password';$('show').checked=false;
  $('btnGo').disabled=false;$('btnGo').textContent='连接';
  (manual?$('mssid'):$('mpass')).focus();
}
function closePw(){$('pwbox').className='pw';PICK=null}
$('btnScan').onclick=function(){
  var b=$('btnScan');b.disabled=true;b.innerHTML='<span class="spin"></span> 扫描中';
  api('/api/scan','{}').then(function(){scanPoll(0)}).catch(function(e){
    b.disabled=false;b.textContent='扫描附近网络';toast('扫描启动失败','bad')});
};
function scanPoll(n){
  var b=$('btnScan');
  if(n>30){b.disabled=false;b.textContent='扫描附近网络';toast('扫描超时，请重试','bad');return}
  api('/api/scan').then(function(d){
    if(d.scanning){setTimeout(function(){scanPoll(n+1)},600);return}
    b.disabled=false;b.textContent='重新扫描';
    $('scanHint').textContent=(d.nets||[]).length+' 个网络';
    renderNets(d.nets);
    if(!(d.nets||[]).length)toast('没有扫到 2.4GHz 网络','bad');
  }).catch(function(){b.disabled=false;b.textContent='扫描附近网络'});
}

/* ---------- 连接 ---------- */
$('btnGo').onclick=function(){
  if(!PICK)return;
  var ssid=PICK.manual?$('mssid').value.trim():PICK.ssid;
  if(!ssid){toast('请填写网络名称','bad');return}
  var pass=$('mpass').value;
  if(pass.length>64){toast('密码过长','bad');return}
  $('btnGo').disabled=true;$('btnGo').textContent='连接中';
  api('/api/connect',JSON.stringify({ssid:ssid,pass:pass})).then(function(r){
    if(r.ok!==1){$('btnGo').disabled=false;$('btnGo').textContent='重试';
      toast(r.err||'设备拒绝了请求','bad');return}
    closePw();chip('连接中','warn');startPoll();
    api('/api/state').then(paintState).catch(function(){});
  }).catch(function(e){$('btnGo').disabled=false;$('btnGo').textContent='连接';
    toast('请求失败，请刷新页面','bad')});
};
$('btnBack').onclick=closePw;
$('btnManual').onclick=function(){openPw('',true)};
$('show').onclick=function(){$('mpass').type=this.checked?'text':'password'};
$('btnClear').onclick=function(){
  var b=$('btnClear');
  if(!arm(b,'清除','再次点击确认'))return;
  api('/api/clear','{}').then(function(){toast('已清除 WiFi 账号','ok');
    renderNets(null);api('/api/state').then(paintState).catch(function(){})})
    .catch(function(){toast('清除失败','bad')});
  setTimeout(function(){b.textContent='清除'},3200);
};

/* ---------- 参数 ---------- */
function fillParams(p){
  $('host').value=p.host||'';$('key').value=p.key||'';$('loc').value=p.loc||'';
  $('cddlab').value=p.cdd_label||'';$('cdddate').value=p.cdd_date||'';$('tags').value=p.tags||'';
  $('sch').value=p.schedule||'';
  setRadio('rtype',p.type||'0');setRadio('rweek',p.week1st||'0');
  ORIG=JSON.stringify(collect());
  refreshSave();
}
$('btnSave').onclick=function(){
  if(!validate())return;
  var b=$('btnSave');b.disabled=true;b.textContent='保存中';
  api('/api/params',JSON.stringify(collect())).then(function(r){
    if(r.ok!==1){b.textContent='保存参数';toast(r.err||'保存失败','bad');return}
    fillParams(r.values);b.textContent='已保存';toast('参数已保存，重启后屏幕生效','ok');
  }).catch(function(){b.textContent='保存参数';toast('保存失败，请重试','bad')});
};
FLDS.forEach(function(f){$(f['in']).addEventListener('input',refreshSave)});
['rtype','rweek'].forEach(function(g){$(g).onclick=function(e){
  var v=e.target.getAttribute&&e.target.getAttribute('data-v');
  if(!v)return;setRadio(g,v);refreshSave()}});

/* ---------- 系统 ---------- */
$('btnReboot').onclick=function(){
  var b=$('btnReboot');
  if(!arm(b,'重启','确认重启？'))return;
  api('/api/reboot','{}').then(function(){chip('重启中','warn');
    toast('设备正在重启，热点会关闭，请重新连接')} ).catch(function(){chip('重启中','warn')});
};
$('btnExit').onclick=function(){location.href='/exit'};
$('btnOta').onclick=function(){location.href='/update'};

/* ---------- 启动 ---------- */
api('/api/params').then(fillParams).catch(function(){toast('读取参数失败，请刷新页面','bad')});
api('/api/state').then(paintState).catch(function(){$('ver').textContent='与设备连接中断，请刷新页面'});
setInterval(function(){if(!pollTimer)api('/api/state').then(paintState).catch(function(){})},9000);
</script>
</body>
</html>
)HTML";

#endif
