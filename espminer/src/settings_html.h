// Settings form. %PLACEHOLDERS% are filled in by webui.cpp.
#pragma once
#include <Arduino.h>

static const char SETTINGS_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESPMiner settings</title>
<style>
:root{--bg:#0b0d10;--card:#14181d;--line:#232a32;--fg:#e8edf2;--dim:#8b97a5;--accent:#f7931a}
*{box-sizing:border-box}
body{margin:0;padding:24px 16px 48px;background:var(--bg);color:var(--fg);
     font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto}
h1{font-size:20px;margin:0 0 4px}
h1 span{color:var(--accent)}
p.sub{color:var(--dim);font-size:13px;margin:0 0 22px}
fieldset{border:1px solid var(--line);background:var(--card);border-radius:12px;padding:16px 18px 20px;margin:0 0 16px}
legend{color:var(--accent);font-size:12px;text-transform:uppercase;letter-spacing:1.4px;padding:0 6px}
label{display:block;font-size:12px;color:var(--dim);margin:12px 0 4px;text-transform:uppercase;letter-spacing:1px}
input{width:100%;padding:10px 12px;border-radius:8px;border:1px solid var(--line);
      background:#0e1216;color:var(--fg);font:14px ui-monospace,SFMono-Regular,Menlo,monospace}
input:focus{outline:2px solid var(--accent);outline-offset:1px}
.hint{font-size:12px;color:var(--dim);margin-top:6px}
.row{display:flex;gap:10px;margin-top:8px;flex-wrap:wrap}
button,a.btn{background:var(--accent);color:#111;border:0;border-radius:9px;padding:11px 20px;
  font-size:14px;font-weight:600;cursor:pointer;text-decoration:none;display:inline-block}
a.btn.ghost{background:transparent;color:var(--fg);border:1px solid var(--line)}
a.danger{color:#ff7b7b;font-size:12px;text-decoration:none}
</style></head><body><div class="wrap">
<h1>ESP<span>Miner</span> settings</h1>
<p class="sub">Saved to flash and applied after a reboot.</p>
<form method="POST" action="/save">

<fieldset><legend>Wi-Fi</legend>
  <label for="ssid">Network name (2.4 GHz)</label>
  <input id="ssid" name="ssid" value="%SSID%" maxlength="32" required>
  <label for="pass">Password</label>
  <input id="pass" name="pass" type="password" placeholder="%PASSPH%" maxlength="63">
  <div class="hint">Leave blank to keep the current password.</div>
</fieldset>

<fieldset><legend>Payout</legend>
  <label for="btc">Bitcoin address</label>
  <input id="btc" name="btc" value="%BTC%" maxlength="90" required>
  <div class="hint">Where the block reward goes. Must be an address you hold the keys to.</div>
  <label for="worker">Worker name</label>
  <input id="worker" name="worker" value="%WORKER%" maxlength="24">
</fieldset>

<fieldset><legend>Pool</legend>
  <label for="host">Stratum host</label>
  <input id="host" name="host" value="%HOST%" maxlength="64" required>
  <label for="port">Port</label>
  <input id="port" name="port" type="number" min="1" max="65535" value="%PORT%" required>
  <label for="ppass">Stratum password</label>
  <input id="ppass" name="ppass" value="%PPASS%" maxlength="32">
  <label for="sdiff">Suggested difficulty (0 = pool decides)</label>
  <input id="sdiff" name="sdiff" value="%SDIFF%" maxlength="16">
</fieldset>

<div class="row">
  <button type="submit">Save &amp; reboot</button>
  <a class="btn ghost" href="/">Cancel</a>
</div>
</form>
<p style="margin-top:22px"><a class="danger" href="/reset" onclick="return confirm('Erase saved settings and reboot?')">Factory reset</a></p>
</div></body></html>)HTML";
