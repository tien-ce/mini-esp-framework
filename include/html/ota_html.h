#ifndef OTA_HTML_H
#define OTA_HTML_H

#include <Arduino.h>

// Generated from html_test/ota.html (comments and indentation stripped to save heap).
// Uses the chunked OTA routes /api/ota/{info,begin,chunk,end,abort,status}; falls back to POST /doUpdate on old firmware.
const char OTA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Firmware Upgrade</title>
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
.drop { display: block; padding: 22px 16px; border: 1px dashed var(--line-strong); border-radius: 6px; text-align: center; cursor: pointer; transition: border-color .15s, background-color .15s; }
.drop:hover, .drop.over, .drop:focus-within { border-color: var(--gold); background: rgba(232, 237, 230, .04); }
.drop input { position: absolute; width: 1px; height: 1px; opacity: 0; }
.drop strong { display: block; font-size: 19px; font-weight: 600; }
.drop span { color: var(--silk-dim); font-size: 16px; }
.picked { display: none; align-items: baseline; gap: 10px; margin-top: 14px; font-size: 17px; }
.picked.show { display: flex; }
.picked .name { font-family: var(--font-code); font-size: 15px; word-break: break-all; }
.picked .size { margin-left: auto; flex-shrink: 0; color: var(--silk-dim); font-variant-numeric: tabular-nums; }
.bar { display: none; height: 8px; margin-top: 14px; border-radius: 4px; background: var(--mask-deep); overflow: hidden; }
.bar.show { display: block; }
.bar i { display: block; height: 100%; width: 0; background: var(--gold); transition: width .15s; }
.bar.err i { background: var(--fault); }
.bar.ok i { background: var(--mint); }
.stats { display: none; flex-wrap: wrap; gap: 4px 18px; margin-top: 10px; font-family: var(--font-code); font-size: 14px; color: var(--silk-dim); font-variant-numeric: tabular-nums; }
.stats.show { display: flex; }
.stats .pct { color: var(--silk); font-weight: 500; }
.stats .fill { margin-left: auto; }
.msg strong { display: block; font-weight: 600; }
.msg .detail { display: block; margin-top: 6px; font-family: var(--font-code); font-size: 14px; color: var(--silk-dim); word-break: break-word; }
.msg .btn { margin-top: 10px; }
.limits { margin-top: 12px; color: var(--silk-dim); font-size: 16px; }
.limits.bad { color: var(--fault); }
[hidden] { display: none !important; }
.msg { display: none; margin-top: 14px; padding: 10px 12px; border-radius: 5px; font-size: 16px; }
.msg.show { display: block; }
.msg.ok { background: rgba(124, 224, 168, .12); color: var(--mint); }
.msg.err { background: rgba(240, 113, 120, .12); color: var(--fault); }
.msg.busy { background: rgba(212, 175, 90, .12); color: var(--gold); }
.row-actions { display: flex; justify-content: flex-end; gap: 10px; margin-top: 16px; }
.warn { margin-top: 14px; color: var(--silk-dim); font-size: 16px; }
@media (max-width: 560px) { .row-actions { flex-direction: column-reverse; } .row-actions .btn { width: 100%; } .stats .fill { margin-left: 0; } }
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
<section class="panel" aria-labelledby="fileTitle">
<div class="panel-head"><h2 id="fileTitle">Upload a firmware file</h2></div>
<form class="panel-body" id="uploadForm">
<label class="drop" id="drop">
<input type="file" id="file" accept=".bin" required>
<strong>Choose a .bin file</strong>
<span>or drop it here</span>
</label>
<div class="picked" id="picked"><span class="name" id="pickedName"></span><span class="size" id="pickedSize"></span></div>
<p class="limits" id="limits" hidden></p>
<div class="bar" id="bar" role="progressbar" aria-label="Upgrade progress" aria-valuemin="0" aria-valuemax="100" aria-valuenow="0"><i id="barFill"></i></div>
<div class="stats" id="stats"><span class="pct" id="statPct">0%</span><span id="statChunk"></span><span id="statBytes"></span><span class="fill" id="statNote"></span></div>
<div class="msg" id="status" role="status" aria-live="polite"></div>
<p class="warn">Keep the board powered and this page open until it restarts.</p>
<div class="row-actions">
<button class="btn" type="button" id="btnCancel" hidden>Cancel upgrade</button>
<button class="btn btn-primary" type="submit" id="btnUpgrade" disabled>Start upgrade</button>
</div>
</form>
</section>
<section class="panel" aria-labelledby="urlTitle">
<div class="panel-head"><h2 id="urlTitle">Upgrade from a web address</h2></div>
<div class="panel-body"><p class="note">Not available yet. Upload a file instead.</p></div>
</section>
</main>
<footer class="footer">
<p id="footerText">%FOOTER_TEXT%</p>
</footer>
</div>
<script>
var fileInput = document.getElementById('file');
var btn = document.getElementById('btnUpgrade');
var btnCancel = document.getElementById('btnCancel');
var drop = document.getElementById('drop');
var statusEl = document.getElementById('status');
var limitsEl = document.getElementById('limits');
var bar = document.getElementById('bar');
var barFill = document.getElementById('barFill');
var statsEl = document.getElementById('stats');
var MAX_RETRIES = 3;          // per chunk, for errors the board marks as retryable and for network trouble
var CHUNK_TIMEOUT_MS = 20000; // one chunk request
var BOOT_WAIT_MS = 2500;      // give the board time to go down before polling for it
var BOOT_POLL_MS = 1500;
var BOOT_POLL_TRIES = 40;
var busy = false;
var cancelled = false;
var info = null;              // answer of GET /api/ota/info, or null when the board has no such route
var session = null;
function formatSize(bytes) {
if (bytes < 1024) return bytes + ' B';
if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
return (bytes / 1048576).toFixed(2) + ' MB';
}
function sleep(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }
var CRC_TABLE = (function () {
var t = new Uint32Array(256);
for (var n = 0; n < 256; n++) {
var c = n;
for (var k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
t[n] = c >>> 0;
}
return t;
})();
function crc32Hex(bytes) {
var c = 0xFFFFFFFF;
for (var i = 0; i < bytes.length; i++) c = CRC_TABLE[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8);
return ('00000000' + ((c ^ 0xFFFFFFFF) >>> 0).toString(16)).slice(-8);
}
function showStatus(kind, text) {
statusEl.className = 'msg show ' + kind;
statusEl.textContent = text;
}
function showError(title, err) {
statusEl.className = 'msg show err';
statusEl.textContent = '';
var strong = document.createElement('strong');
strong.textContent = title;
statusEl.appendChild(strong);
if (err && err.message) statusEl.appendChild(document.createTextNode(err.message));
var bits = [];
if (err && err.stage) bits.push('stage ' + err.stage);
if (err && err.code) bits.push(err.code);
if (err && err.chunk != null) bits.push('chunk ' + err.chunk);
if (err && err.offset != null) bits.push('offset ' + err.offset);
if (err && err.http) bits.push('HTTP ' + err.http);
if (bits.length) {
var d = document.createElement('span');
d.className = 'detail';
d.textContent = bits.join(' · ');
statusEl.appendChild(d);
}
bar.classList.add('err');
}
function setProgress(percent, chunkNo, chunkTotal, written, total) {
var pct = Math.max(0, Math.min(100, percent));
barFill.style.width = pct + '%';
bar.setAttribute('aria-valuenow', Math.round(pct));
document.getElementById('statPct').textContent = pct.toFixed(1) + '%';
document.getElementById('statChunk').textContent = chunkTotal ? 'chunk ' + Math.min(chunkNo, chunkTotal) + ' / ' + chunkTotal : '';
document.getElementById('statBytes').textContent = total ? formatSize(written) + ' of ' + formatSize(total) : '';
}
function setNote(text) { document.getElementById('statNote').textContent = text || ''; }
function resetView() {
bar.classList.remove('show', 'err', 'ok');
statsEl.classList.remove('show');
barFill.style.width = '0%';
statusEl.className = 'msg';
}
function setBusy(on) {
busy = on;
btnCancel.hidden = !on;
btnCancel.disabled = false;
btn.hidden = on;
btn.disabled = on || !fileInput.files[0];
fileInput.disabled = on;
}
function api(method, url, body, timeoutMs) {
var ctl = new AbortController();
var timer = setTimeout(function () { ctl.abort(); }, timeoutMs || 10000);
return fetch(url, { method: method, body: body, signal: ctl.signal, cache: 'no-store' }).then(function (res) {
clearTimeout(timer);
return res.text().then(function (text) {
var data = null;
try { data = JSON.parse(text); } catch (e) { /* not JSON */ }
if (res.ok && data && data.ok !== false) return data;
var err = (data && typeof data === 'object') ? data : {};
err.http = res.status;
if (!err.message) err.message = 'The board answered with status ' + res.status + '.';
if (res.status === 404 && !data) err.noRoute = true;
throw err;
});
}, function (e) {
clearTimeout(timer);
throw { network: true, message: e.name === 'AbortError' ? 'The board did not answer in time.' : "Couldn't reach the board." };
});
}
function loadInfo() {
return api('GET', '/api/ota/info').then(function (d) {
info = d;
var lim = 'Largest file: ' + formatSize(d.max_size) + (d.chip ? ' · board: ' + d.chip : '');
limitsEl.textContent = d.busy ? 'Another upgrade is already running on the board. Wait for it to finish, then reload this page.' : lim;
limitsEl.classList.toggle('bad', !!d.busy);
limitsEl.hidden = false;
showFile();
}).catch(function (e) {
info = null;
if (!e.noRoute) {
limitsEl.textContent = (e.network ? "Couldn't reach the board to read its upgrade limits." : 'The board did not give its upgrade limits.') + ' You can still try the upgrade.';
limitsEl.classList.add('bad');
limitsEl.hidden = false;
}
});
}
function problemWithFile(f, head) {
if (!/\.bin$/i.test(f.name)) return 'This is not a .bin file. Choose the firmware image built for this board.';
if (f.size < 1024) return 'This file is too small to be a firmware image.';
if (info && info.max_size && f.size > info.max_size) return 'This file (' + formatSize(f.size) + ') is larger than the board accepts (' + formatSize(info.max_size) + ').';
if (head && head[0] !== 0xE9) return 'This file does not look like an ESP firmware image (it should start with byte 0xE9).';
return '';
}
function showFile() {
var f = fileInput.files[0];
var picked = document.getElementById('picked');
picked.classList.toggle('show', !!f);
btn.disabled = !f || busy || !!(info && info.busy);
resetView();
if (!f) return;
document.getElementById('pickedName').textContent = f.name;
document.getElementById('pickedSize').textContent = formatSize(f.size);
f.slice(0, 4).arrayBuffer().then(function (buf) {
var why = problemWithFile(f, new Uint8Array(buf));
if (why) { showStatus('err', why); btn.disabled = true; }
});
}
fileInput.addEventListener('change', showFile);
['dragenter', 'dragover'].forEach(function (t) { drop.addEventListener(t, function (e) { e.preventDefault(); drop.classList.add('over'); }); });
['dragleave', 'drop'].forEach(function (t) { drop.addEventListener(t, function (e) { e.preventDefault(); drop.classList.remove('over'); }); });
drop.addEventListener('drop', function (e) {
if (e.dataTransfer.files.length) { fileInput.files = e.dataTransfer.files; showFile(); }
});
window.addEventListener('beforeunload', function (e) {
if (busy) { e.preventDefault(); e.returnValue = ''; }
});
function sendChunk(file, offset, chunkSize, total) {
var end = Math.min(offset + chunkSize, total);
return file.slice(offset, end).arrayBuffer().then(function (buf) {
var crc = crc32Hex(new Uint8Array(buf));
var url = '/api/ota/chunk?session=' + encodeURIComponent(session) + '&offset=' + offset + '&crc=' + crc;
return api('POST', url, new Blob([buf], { type: 'application/octet-stream' }), CHUNK_TIMEOUT_MS);
});
}
function sendChunkWithRetry(file, offset, chunkSize, total, chunkNo) {
var attempt = 0;
function once() {
if (cancelled) return Promise.reject({ cancelled: true });
return sendChunk(file, offset, chunkSize, total).then(function (ans) {
setNote('');
return ans;
}, function (err) {
var transient = err.network || err.retryable;
if (!transient || attempt >= MAX_RETRIES) { err.chunk = err.chunk != null ? err.chunk : chunkNo; if (err.offset == null) err.offset = offset; throw err; }
attempt++;
setNote('Chunk ' + chunkNo + ' failed (' + (err.stage || 'network') + '); retry ' + attempt + ' of ' + MAX_RETRIES + '…');
var resync = err.network
? sleep(800).then(function () { return api('GET', '/api/ota/status').catch(function () { return null; }); })
: Promise.resolve(null);
return resync.then(function (st) {
if (st && st.state === 'failed') throw st.error || { message: 'The board stopped the upgrade.' };
if (st && st.state === 'receiving' && st.offset > offset) return { ok: true, offset: st.offset, total: st.total, percent: st.percent }; // the board already has this chunk
if (err.expected_offset != null && err.expected_offset !== offset) return { ok: true, offset: err.expected_offset, total: total, percent: err.expected_offset / total * 100 };
return sleep(300 * attempt).then(once);
});
});
}
return once();
}
function waitForBoard(tries) {
return fetch('/api/telemetry', { cache: 'no-store' }).then(function (r) {
if (!r.ok) throw new Error('down');
}).catch(function () {
if (tries <= 0) throw new Error('timeout');
return sleep(BOOT_POLL_MS).then(function () { return waitForBoard(tries - 1); });
});
}
function runUpgrade(file) {
var total = file.size;
var started = Date.now();
session = null;
cancelled = false;
setBusy(true);
bar.classList.remove('err', 'ok');
bar.classList.add('show');
statsEl.classList.add('show');
setProgress(0, 0, 0, 0, total);
setNote('');
showStatus('busy', 'Starting the upgrade…');
var beginUrl = '/api/ota/begin?size=' + total;
return api('POST', beginUrl).then(function (b) {
session = b.session;
var chunkSize = b.chunk_size;
if (!(chunkSize > 0)) throw { message: 'The board has too little free memory to receive the firmware. Restart it and try again.', stage: 'oom' };
var chunkTotal = Math.ceil(total / chunkSize);
var offset = b.offset || 0;
showStatus('busy', 'Writing firmware to the board. Keep this page open.');
function next() {
if (cancelled) return Promise.reject({ cancelled: true });
if (offset >= total) return Promise.resolve();
var chunkNo = Math.floor(offset / chunkSize) + 1;
return sendChunkWithRetry(file, offset, chunkSize, total, chunkNo).then(function (ans) {
offset = ans.offset;
var pct = ans.percent != null ? ans.percent : offset / total * 100;
setProgress(pct, Math.ceil(offset / chunkSize), chunkTotal, offset, total);
var secs = (Date.now() - started) / 1000;
if (secs > 1) setNote(formatSize(offset / secs) + '/s');
return next();
});
}
return next();
}).then(function () {
showStatus('busy', 'Checking the whole image…');
setProgress(100, 0, 0, total, total);
document.getElementById('statChunk').textContent = 'verifying';
return api('POST', '/api/ota/end?session=' + encodeURIComponent(session), null, 30000);
}).then(function (end) {
session = null;
bar.classList.add('ok');
setNote('');
document.getElementById('statChunk').textContent = 'written';
showStatus('ok', 'Firmware written. The board is restarting; this page returns to the main menu when it is back.');
return sleep(Math.max(end.reboot_in_ms || 0, 0) + BOOT_WAIT_MS).then(function () { return waitForBoard(BOOT_POLL_TRIES); }).then(function () {
window.location.href = '/';
}, function () {
setBusy(false);
showStatus('err', "The board hasn't come back yet. Check its power and address, then reload this page.");
});
}).catch(function (err) {
var sid = session;
session = null;
setBusy(false);
btn.disabled = false;
btn.textContent = 'Try again';
if (err && err.cancelled) {
bar.classList.add('err');
showStatus('busy', 'Upgrade cancelled. The board keeps its current firmware.');
} else {
showError(err && err.network ? "Couldn't finish the upgrade" : 'The upgrade failed', err);
if (err && err.stage) setNote('Stopped at ' + document.getElementById('statPct').textContent);
}
if (sid) api('POST', '/api/ota/abort?session=' + encodeURIComponent(sid)).catch(function () {});
});
}
btnCancel.addEventListener('click', function () {
cancelled = true;
btnCancel.disabled = true;
showStatus('busy', 'Cancelling…');
});
function runLegacy(f) {
setBusy(true);
btnCancel.hidden = true;
bar.classList.remove('err', 'ok');
bar.classList.add('show');
statsEl.classList.add('show');
setProgress(0, 0, 0, 0, f.size);
showStatus('busy', 'Uploading firmware…');
var formData = new FormData();
formData.append('file', f);
var xhr = new XMLHttpRequest();
xhr.upload.onprogress = function (ev) {
if (!ev.lengthComputable) return;
setProgress(ev.loaded / ev.total * 100, 0, 0, ev.loaded, ev.total);
};
xhr.onload = function () {
setBusy(false);
if (xhr.status === 200 && xhr.responseText.trim() === 'OK') {
setProgress(100, 0, 0, f.size, f.size);
bar.classList.add('ok');
showStatus('ok', 'Upgrade finished. The board is restarting; this page returns to the main menu in a few seconds.');
btn.hidden = true;
setTimeout(function () { window.location.href = '/'; }, 5000);
} else {
btn.disabled = false;
showError('The board rejected this firmware', { message: 'Check that it was built for this board, then try again.', http: xhr.status });
}
};
xhr.onerror = function () {
setBusy(false);
btn.disabled = false;
showError("Couldn't reach the board", { message: 'Check the connection and try again.' });
};
xhr.open('POST', '/doUpdate');
xhr.send(formData);
}
document.getElementById('uploadForm').addEventListener('submit', function (e) {
e.preventDefault();
var f = fileInput.files[0];
if (!f || busy) return;
f.slice(0, 4).arrayBuffer().then(function (buf) {
var why = problemWithFile(f, new Uint8Array(buf));
if (why) { showStatus('err', why); return; }
resetView();
if (info) runUpgrade(f); else runLegacy(f);
});
});
loadInfo();
</script>
</body>
</html>
)rawliteral";

#endif
