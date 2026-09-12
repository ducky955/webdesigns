// Dashboard page. Kept in flash; it polls /api/status once a second.
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESPMiner</title>
<style>
:root{--bg:#0b0d10;--card:#14181d;--line:#232a32;--fg:#e8edf2;--dim:#8b97a5;--accent:#f7931a;--ok:#3ddc84;--bad:#ff5c5c}
*{box-sizing:border-box}
body{margin:0;padding:24px 16px 48px;background:var(--bg);color:var(--fg);
     font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:880px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:20px}
h1{font-size:20px;margin:0;letter-spacing:.5px}
h1 span{color:var(--accent)}
.pill{margin-left:auto;display:flex;align-items:center;gap:8px;font-size:13px;color:var(--dim);
      background:var(--card);border:1px solid var(--line);padding:6px 12px;border-radius:999px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--bad);transition:background .3s}
.dot.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.hero{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:24px;margin-bottom:16px}
.hero .label{color:var(--dim);font-size:12px;text-transform:uppercase;letter-spacing:1.4px}
.hero .value{font:600 46px/1.1 ui-monospace,SFMono-Regular,Menlo,monospace;color:var(--accent);margin-top:6px}
.hero .value small{font-size:18px;color:var(--dim);font-weight:400;margin-left:6px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-bottom:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px}
.card .k{color:var(--dim);font-size:11px;text-transform:uppercase;letter-spacing:1.2px}
.card .v{font:500 20px/1.3 ui-monospace,SFMono-Regular,Menlo,monospace;margin-top:4px;word-break:break-all}
table{width:100%;border-collapse:collapse;background:var(--card);border:1px solid var(--line);border-radius:12px;overflow:hidden}
td{padding:10px 16px;border-top:1px solid var(--line);font-size:14px}
tr:first-child td{border-top:0}
td:first-child{color:var(--dim);width:40%}
td:last-child{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;word-break:break-all}
.row{display:flex;gap:10px;margin-top:16px;flex-wrap:wrap}
a.btn,button{display:inline-block;background:var(--accent);color:#111;border:0;border-radius:9px;
  padding:10px 18px;font-size:14px;font-weight:600;text-decoration:none;cursor:pointer}
a.btn.ghost{background:transparent;color:var(--fg);border:1px solid var(--line)}
footer{color:var(--dim);font-size:12px;margin-top:22px;text-align:center}
.blk{background:linear-gradient(90deg,#f7931a,#ffd280);color:#111;padding:14px 16px;border-radius:12px;
     font-weight:700;margin-bottom:16px;display:none}
</style></head><body><div class="wrap">
<header>
  <h1>ESP<span>Miner</span></h1>
  <div class="pill"><span class="dot" id="dot"></span><span id="state">connecting</span></div>
</header>

<div class="blk" id="blk">BLOCK CANDIDATE FOUND - check your pool dashboard!</div>

<div class="hero">
  <div class="label">Hash rate</div>
  <div class="value"><span id="hr">0.00</span><small id="hru">H/s</small></div>
</div>

<div class="grid">
  <div class="card"><div class="k">Shares accepted</div><div class="v" id="acc">0</div></div>
  <div class="card"><div class="k">Rejected</div><div class="v" id="rej">0</div></div>
  <div class="card"><div class="k">Best difficulty</div><div class="v" id="best">0</div></div>
  <div class="card"><div class="k">All-time best</div><div class="v" id="bestAll">0</div></div>
  <div class="card"><div class="k">Pool difficulty</div><div class="v" id="pdiff">-</div></div>
  <div class="card"><div class="k">Total hashes</div><div class="v" id="total">0</div></div>
</div>

<table>
  <tr><td>Payout address</td><td id="addr">-</td></tr>
  <tr><td>Worker</td><td id="worker">-</td></tr>
  <tr><td>Pool</td><td id="pool">-</td></tr>
  <tr><td>Jobs received</td><td id="jobs">0</td></tr>
  <tr><td>Wi-Fi</td><td id="wifi">-</td></tr>
  <tr><td>IP address</td><td id="ip">-</td></tr>
  <tr><td>Uptime</td><td id="up">-</td></tr>
  <tr><td>Free heap</td><td id="heap">-</td></tr>
  <tr><td>Firmware</td><td id="fw">-</td></tr>
</table>

<div class="row">
  <a class="btn" href="/settings">Settings</a>
  <a class="btn ghost" href="#" id="reboot">Reboot</a>
</div>
<footer>Solo mining is a lottery ticket. Good luck.</footer>
</div>
<script>
const $=id=>document.getElementById(id);
function rate(h){
  const u=["H/s","kH/s","MH/s","GH/s"];let i=0;
  while(h>=1000&&i<u.length-1){h/=1000;i++}
  return [h.toFixed(2),u[i]];
}
function big(n){return n.toLocaleString(undefined,{maximumFractionDigits:0})}
function dur(s){
  const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  return (d?d+"d ":"")+(h||d?h+"h ":"")+m+"m "+(s%60)+"s";
}
async function tick(){
  try{
    const r=await fetch("/api/status",{cache:"no-store"});
    const j=await r.json();
    const [v,u]=rate(j.hashrate);
    $("hr").textContent=v; $("hru").textContent=u;
    $("state").textContent=j.pool.state;
    $("dot").className="dot"+(j.pool.state==="mining"?" on":"");
    $("acc").textContent=big(j.shares.accepted);
    $("rej").textContent=big(j.shares.rejected);
    $("best").textContent=j.best.toFixed(3);
    $("bestAll").textContent=j.bestAllTime.toFixed(3);
    $("pdiff").textContent=j.poolDifficulty;
    $("total").textContent=big(j.totalHashes);
    $("addr").textContent=j.address;
    $("worker").textContent=j.worker;
    $("pool").textContent=j.pool.host+":"+j.pool.port;
    $("jobs").textContent=big(j.jobs);
    $("wifi").textContent=j.wifi.ssid+"  ("+j.wifi.rssi+" dBm)";
    $("ip").textContent=j.wifi.ip;
    $("up").textContent=dur(j.uptime);
    $("heap").textContent=big(j.heap)+" bytes";
    $("fw").textContent=j.version;
    if(j.blocks>0)$("blk").style.display="block";
  }catch(e){$("dot").className="dot";$("state").textContent="no link";}
}
$("reboot").onclick=async e=>{
  e.preventDefault();
  if(confirm("Reboot the miner?")){await fetch("/restart",{method:"POST"});}
};
tick();setInterval(tick,1000);
</script></body></html>)HTML";
