#ifndef CONFIG_MODULE_HTML_H
#define CONFIG_MODULE_HTML_H

#include <Arduino.h>

// Generated from html_test/config-module.html (comments and indentation stripped to save heap).
// The pin rows are built in the browser from GET /api/gpio; there is no %GPIO_TABLE_ROWS% placeholder.
const char CONFIG_MODULE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Module Settings</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@400;500;600&family=Spline+Sans+Mono:wght@400;500&display=swap">
<style>
:root {
--mask: #164236; --mask-deep: #0f2f28; --mask-raised: #1f5244;
--line: #28584a; --line-strong: #3a6f60;
--silk: #e8ede6; --silk-dim: #a3b8ae; --silk-faint: #88aca0;
--gold: #d4af5a; --mint: #7ce0a8; --fault: #f07178;
--font-ui: "Barlow Semi Condensed", "Segoe UI", system-ui, -apple-system, sans-serif;
--font-code: "Spline Sans Mono", Consolas, "SF Mono", Menlo, monospace;
color-scheme: dark;
}
* { margin: 0; padding: 0; box-sizing: border-box; }
body { min-height: 100vh; background: var(--mask-deep); color: var(--silk); font-family: var(--font-ui); font-size: 16px; line-height: 1.4; -webkit-font-smoothing: antialiased; }
button, a, input { font: inherit; color: inherit; }
button { cursor: pointer; }
:focus-visible { outline: 2px solid var(--gold); outline-offset: 2px; }
.page { max-width: 760px; margin: 0 auto; padding: 0 16px 40px; }
.masthead { display: flex; align-items: center; gap: 16px; padding: 28px 0 22px; }
.chip { position: relative; flex-shrink: 0; width: 40px; height: 40px; margin: 0 7px; display: grid; place-items: center; border: 1px solid var(--line-strong); border-radius: 4px; background: var(--mask); color: var(--silk-dim); font-size: 14px; font-weight: 600; }
.chip::before, .chip::after { content: ''; position: absolute; top: 5px; width: 5px; height: 30px; background: repeating-linear-gradient(to bottom, var(--gold) 0 2px, transparent 2px 6px); }
.chip::before { left: -7px; }
.chip::after { right: -7px; }
.titles { min-width: 0; }
.titles h1 { font-size: 26px; font-weight: 600; line-height: 1.1; }
.titles p { margin-top: 2px; color: var(--silk-dim); font-size: 16px; }
.home { margin-left: auto; flex-shrink: 0; display: inline-flex; align-items: center; gap: 6px; height: 34px; padding: 0 12px 0 10px; border: 1px solid var(--line-strong); border-radius: 5px; color: var(--silk); font-size: 16px; font-weight: 500; text-decoration: none; transition: background-color .15s; }
.home:hover { background: rgba(232, 237, 230, .08); }
.home svg { width: 16px; height: 16px; }
.panel { border: 1px solid var(--line); border-radius: 6px; background: var(--mask); }
.panel + .panel { margin-top: 16px; }
.panel-head { display: flex; align-items: center; gap: 12px; min-height: 44px; padding: 0 16px; border-bottom: 1px solid var(--line); }
.panel-head h2 { flex: 1; font-size: 18px; font-weight: 600; }
.panel-body { padding: 16px; }
.note { color: var(--silk-dim); font-size: 16px; }
.live { display: flex; align-items: center; gap: 8px; color: var(--silk-dim); font-size: 15px; }
.led { width: 8px; height: 8px; border-radius: 50%; background: var(--silk-faint); }
.live[data-state="live"] .led { background: var(--mint); box-shadow: 0 0 0 3px rgba(124, 224, 168, .16), 0 0 10px rgba(124, 224, 168, .55); }
.live[data-state="wait"] .led { background: var(--gold); animation: blink .9s ease-in-out infinite alternate; }
.live[data-state="down"] .led { background: var(--fault); }
@keyframes blink { to { opacity: .25; } }
.links { list-style: none; }
.links li + li { border-top: 1px solid var(--line); }
.links a { display: flex; align-items: center; gap: 14px; padding: 14px 16px; text-decoration: none; transition: background-color .15s; }
.links a:hover { background: var(--mask-raised); }
.links svg.icon { flex-shrink: 0; width: 24px; height: 24px; color: var(--gold); }
.links svg.go { flex-shrink: 0; width: 16px; height: 16px; margin-left: auto; color: var(--silk-faint); }
.links strong { display: block; font-variant-ligatures: none; font-feature-settings: "liga" 0, "calt" 0; font-size: 19px; font-weight: 600; }
.links span.sub { display: block; color: var(--silk-dim); font-size: 16px; }
.later { list-style: none; display: flex; flex-wrap: wrap; gap: 8px; padding: 14px 16px; }
.later li { padding: 4px 12px; border: 1px dashed var(--line-strong); border-radius: 999px; color: var(--silk-faint); font-size: 16px; }
.spec { list-style: none; padding: 6px 16px 10px; }
.spec li { display: flex; flex-wrap: wrap; align-items: baseline; gap: 10px; min-height: 38px; padding: 7px 0; }
.spec .key { flex-shrink: 0; color: var(--silk-dim); font-size: 17px; }
.spec .leader { flex: 1; min-width: 16px; align-self: end; margin-bottom: 7px; border-bottom: 2px dotted var(--line-strong); }
.spec .val { font-family: var(--font-code); font-size: 16px; font-variant-numeric: tabular-nums; text-align: right; margin-left: auto; overflow-wrap: anywhere; }
.btn { display: inline-flex; align-items: center; justify-content: center; gap: 8px; height: 40px; padding: 0 18px; border: 1px solid var(--line-strong); border-radius: 5px; background: transparent; font-size: 17px; font-weight: 500; transition: background-color .15s; }
.btn svg { width: 16px; height: 16px; }
.btn:hover { background: rgba(232, 237, 230, .08); }
.btn:disabled { opacity: .5; cursor: default; }
.btn-primary { background: var(--gold); border-color: var(--gold); color: var(--mask-deep); }
.btn-primary:hover { background: #e0bf70; }
.footer { margin-top: 28px; padding-top: 16px; border-top: 1px solid var(--line); color: var(--silk-faint); font-size: 15px; }
.facts { display: flex; flex-wrap: wrap; gap: 6px 28px; margin-bottom: 8px; color: var(--silk-dim); font-size: 16px; }
.facts div { display: flex; gap: 8px; }
.facts dt { color: var(--silk-faint); }
.facts dd { font-family: var(--font-code); font-size: 15px; color: var(--silk); align-self: center; }
@media (max-width: 560px) {
.titles h1 { font-size: 23px; }
.home span { display: none; }
.home { padding: 0 10px; }
}
@media (prefers-reduced-motion: reduce) { *, *::before, *::after { animation: none !important; transition: none !important; } }
.fields { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px 20px; padding: 16px; }
.field { display: flex; flex-direction: column; gap: 6px; min-width: 0; }
.field.wide { grid-column: 1 / -1; }
.field label { font-size: 17px; font-weight: 500; }
.field .hint { color: var(--silk-faint); font-size: 15px; }
.field input, .field select { width: 100%; height: 42px; padding: 0 12px; border: 1px solid var(--line-strong); border-radius: 5px; background: var(--mask-deep); color: var(--silk); font-size: 17px; transition: border-color .15s; }
.field input.mono { font-family: var(--font-code); font-size: 16px; }
.field input::placeholder { color: #5d8579; }
.field input:hover, .field select:hover { border-color: var(--silk-faint); }
.field input:focus, .field select:focus { outline: none; border-color: var(--gold); box-shadow: 0 0 0 3px rgba(212, 175, 90, .18); }
.field input:disabled, .field select:disabled { opacity: .55; cursor: not-allowed; }
.field input.bad { border-color: var(--fault); }
.field .err { display: none; color: var(--fault); font-size: 15px; }
.field.has-error .err { display: block; }
.secret { position: relative; }
.secret input { padding-right: 64px; }
.secret button { position: absolute; top: 4px; right: 4px; height: 34px; padding: 0 10px; border: 0; border-radius: 4px; background: transparent; color: var(--silk-dim); font-size: 15px; }
.secret button:hover { background: rgba(232, 237, 230, .08); color: var(--silk); }
.group-note { grid-column: 1 / -1; margin-top: -6px; color: var(--silk-dim); font-size: 16px; }
.form-actions { display: flex; align-items: center; gap: 12px; margin-top: 16px; }
.form-actions .push { margin-left: auto; }
.form-actions .after { color: var(--silk-dim); font-size: 16px; }
.btn.link { text-decoration: none; }
.msg { display: none; margin-top: 16px; padding: 10px 12px; border-radius: 5px; font-size: 16px; }
.msg.show { display: block; }
.msg.ok { background: rgba(124, 224, 168, .12); color: var(--mint); }
.msg.err { background: rgba(240, 113, 120, .12); color: var(--fault); }
.msg.busy, .msg.warn { background: rgba(212, 175, 90, .12); color: var(--gold); }
.pinbar { display: flex; align-items: center; gap: 12px; padding: 12px 16px; border-bottom: 1px solid var(--line); }
.pinbar label { display: inline-flex; align-items: center; gap: 8px; color: var(--silk-dim); font-size: 16px; cursor: pointer; }
.pinbar input { width: 18px; height: 18px; accent-color: var(--gold); }
.pinbar .count { margin-left: auto; color: var(--silk-faint); font-size: 15px; }
.pin { display: flex; align-items: center; gap: 12px; }
.pin label { flex: 0 0 76px; font-family: var(--font-code); font-size: 15px; color: var(--silk-dim); }
.pin select { flex: 1; min-width: 0; height: 42px; padding: 0 12px; border: 1px solid var(--line-strong); border-radius: 5px; background: var(--mask-deep); color: var(--silk); font-size: 17px; }
.pin select:focus { outline: none; border-color: var(--gold); box-shadow: 0 0 0 3px rgba(212, 175, 90, .18); }
.pin select:disabled { opacity: .55; cursor: not-allowed; }
.pin.is-set label { color: var(--gold); }
.pin label.gpio-red { color: #c9777c; }
.group-note.bad { color: var(--fault); }
.group-note .btn { height: 30px; margin-left: 8px; font-size: 15px; }
.hide-none .pin[data-unused="1"] { display: none; }
@media (max-width: 560px) {
.fields { grid-template-columns: 1fr; padding: 14px; }
.form-actions { flex-wrap: wrap; }
.form-actions .btn { flex: 1; }
.form-actions .after { flex-basis: 100%; order: 3; }
}
</style>
</head>
<body>
<div class="page">
<header class="masthead">
<span class="chip" aria-hidden="true">ESP</span>
<div class="titles">
<h1 id="deviceHeader">%HEADER_TITLE%</h1>
<p id="deviceSubHeader">%HEADER_SUBTITLE%</p>
</div>
<a class="home" href="/" title="Back to the main menu" aria-label="Main menu"><svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2.25 7.5 8 2.75l5.75 4.75"/><path d="M3.75 6.5v6.75h3.5V9.75h1.5v3.5h3.5V6.5"/></svg><span>Main menu</span></a>
</header>
<main>
<form id="settingsForm" action="/saveModule" method="POST" novalidate>
<section class="panel" aria-labelledby="pinsTitle">
<div class="panel-head"><h2 id="pinsTitle">Pin assignment</h2></div>
<div class="pinbar">
<label><input type="checkbox" id="onlyUsed"> Show only assigned pins</label>
<span class="count" id="pinCount"></span>
</div>
<div class="fields" id="pinGrid" aria-live="polite"><p class="group-note" id="pinState">Loading pins…</p></div>
</section>
<div class="msg" id="pinWarn" role="status" aria-live="polite"></div>
<div class="msg" id="formMsg" role="status" aria-live="polite"></div>
<div class="form-actions">
<a class="btn link" href="/config">Back to configuration</a>
<span class="push after">Saving restarts the board.</span>
<button class="btn btn-primary" type="submit" id="saveBtn">Save and restart</button>
</div>
</form>
</main>
<footer class="footer">
<dl class="facts">
<div><dt>Model</dt><dd>%CHIP_MODEL%</dd></div>
<div><dt>MAC</dt><dd>%MAC_ADDR%</dd></div>
</dl>
<p id="footerText">%FOOTER_TEXT%</p>
</footer>
</div>
<script>
var form = document.getElementById('settingsForm');
var saveBtn = document.getElementById('saveBtn');
var msgEl = document.getElementById('formMsg');
var RESTART_WAIT_MS = 3000;
var POLL_MS = 1500;
var POLL_TRIES = 40;
function showMsg(kind, text) { msgEl.className = 'msg show ' + kind; msgEl.textContent = text; }
document.querySelectorAll('.secret button').forEach(function (b) {
b.addEventListener('click', function () {
var input = b.parentElement.querySelector('input');
var show = input.type === 'password';
input.type = show ? 'text' : 'password';
b.textContent = show ? 'Hide' : 'Show';
});
});
function checkField(input) {
var v = input.value.trim();
var kind = input.dataset.check;
if (!kind || (v === '' && !input.required)) return '';
if (kind === 'ipv4') {
var ok = /^\d{1,3}(\.\d{1,3}){3}$/.test(v) && v.split('.').every(function (n) { return Number(n) <= 255; });
return ok ? '' : 'Enter an address like 192.168.1.100.';
}
if (kind === 'port') return /^\d+$/.test(v) && Number(v) >= 1 && Number(v) <= 65535 ? '' : 'Enter a port from 1 to 65535.';
if (kind === 'positive') return /^\d+$/.test(v) && Number(v) >= 1 ? '' : 'Enter a whole number of 1 or more.';
return '';
}
function validateForm() {
var first = null;
form.querySelectorAll('input[data-check]').forEach(function (input) {
var problem = checkField(input);
var holder = input.closest('.field');
input.classList.toggle('bad', !!problem);
holder.classList.toggle('has-error', !!problem);
holder.querySelector('.err').textContent = problem;
if (problem && !first) first = input;
});
if (first) { first.focus(); return false; }
if (!form.checkValidity()) { form.reportValidity(); return false; }
return true;
}
function waitForBoard(tries) {
fetch('/stats', { cache: 'no-store' }).then(function (r) {
if (!r.ok) throw new Error('down');
window.location.href = '/';
}).catch(function () {
if (tries <= 0) { showMsg('err', "The board hasn't come back yet. Check its address, then reload this page."); saveBtn.disabled = false; return; }
setTimeout(function () { waitForBoard(tries - 1); }, POLL_MS);
});
}
form.addEventListener('input', function (e) {
e.target.classList.remove('bad');
var holder = e.target.closest('.field');
if (holder) holder.classList.remove('has-error');
});
form.addEventListener('submit', function (e) {
e.preventDefault();
if (!validateForm()) return;
saveBtn.disabled = true;
showMsg('busy', 'Saving…');
fetch(form.action, { method: 'POST', body: new URLSearchParams(new FormData(form)) })
.then(function (res) { if (!res.ok) throw new Error('status ' + res.status); })
.then(function () {
showMsg('ok', 'Saved. The board is restarting; this page returns to the main menu when it is back.');
setTimeout(function () { waitForBoard(POLL_TRIES); }, RESTART_WAIT_MS);
})
.catch(function (err) {
saveBtn.disabled = false;
showMsg('err', "Couldn't save the settings (" + err.message + '). Check the connection and try again.');
});
});
var grid = document.getElementById('pinGrid');
var UNIQUE_SKIP = ['None', 'User', 'PWM / LED', 'ADC Input'];
saveBtn.disabled = true;   // nothing to save until the pins are loaded
function refreshPins() {
var seen = {}, dupes = [], assigned = 0, total = 0;
grid.querySelectorAll('.pin').forEach(function (row) {
var sel = row.querySelector('select');
var v = sel.disabled ? 'None' : sel.value;
var set = v !== 'None';
total++;
row.classList.toggle('is-set', set);
row.dataset.unused = (set || sel.disabled) ? '0' : '1';
if (set) assigned++;
if (UNIQUE_SKIP.indexOf(v) === -1) {
if (seen[v]) { if (dupes.indexOf(v) === -1) dupes.push(v); } else seen[v] = true;
}
});
document.getElementById('pinCount').textContent = total ? assigned + ' of ' + total + ' pins assigned' : '';
var warn = document.getElementById('pinWarn');
warn.className = dupes.length ? 'msg show warn' : 'msg';
warn.textContent = dupes.length ? 'Assigned to more than one pin: ' + dupes.join(', ') + '. You can still save.' : '';
}
function makeOption(value, text, selected) {
var o = document.createElement('option');
o.value = value;
o.textContent = text;
o.selected = selected;
return o;
}
function buildPins(data) {
var frag = document.createDocumentFragment();
data.pins.forEach(function (p) {
var row = document.createElement('div');
row.className = 'pin';
var label = document.createElement('label');
label.htmlFor = 'gpio' + p.gpio;
label.textContent = p.label;
var sel = document.createElement('select');
sel.id = 'gpio' + p.gpio;
sel.name = 'gpio' + p.gpio;
if (p.fixed) {
label.className = 'gpio-red';
sel.disabled = true;
sel.appendChild(makeOption('-1', p.value, true));
} else {
var current = String(p.value).toLowerCase();
var matched = false;
data.options.forEach(function (name) {
var on = !matched && String(name).toLowerCase() === current;
matched = matched || on;
sel.appendChild(makeOption(name, name, on));
});
if (!matched) sel.appendChild(makeOption(p.value, p.value, true));
}
row.appendChild(label);
row.appendChild(sel);
frag.appendChild(row);
});
grid.textContent = '';
grid.appendChild(frag);
}
function showPinError(text) {
grid.textContent = '';
var p = document.createElement('p');
p.className = 'group-note bad';
p.textContent = text + ' ';
var retry = document.createElement('button');
retry.type = 'button';
retry.className = 'btn';
retry.textContent = 'Try again';
retry.addEventListener('click', loadPins);
grid.appendChild(p);
grid.appendChild(retry);
}
function loadPins() {
saveBtn.disabled = true;
grid.innerHTML = '<p class="group-note" id="pinState">Loading pins…</p>';
fetch('/api/gpio', { cache: 'no-store' })
.then(function (res) { if (!res.ok) throw new Error('status ' + res.status); return res.json(); })
.then(function (data) {
if (!Array.isArray(data.options) || !Array.isArray(data.pins)) throw new Error('unexpected answer');
buildPins(data);
saveBtn.disabled = false;
refreshPins();
})
.catch(function (err) { showPinError("Couldn't load the pins (" + err.message + '). Check the connection to the board.'); });
}
grid.addEventListener('change', refreshPins);
document.getElementById('onlyUsed').addEventListener('change', function (e) { grid.classList.toggle('hide-none', e.target.checked); });
loadPins();
</script>
</body>
</html>
)rawliteral";

#endif
