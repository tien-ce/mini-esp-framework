#ifndef INFO_HTML_H
#define INFO_HTML_H

#include <Arduino.h>

const char INFO_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Tasmota - Information</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#232323;color:#ffffff;text-align:center;padding:20px 10px;}
.wrapper{max-width:440px;margin:0 auto;}
h1{font-size:22px;font-weight:bold;margin-bottom:4px;color:#ffffff;}
h2{font-size:26px;font-weight:bold;margin-bottom:20px;color:#ffffff;}
.section-header{font-size:20px;font-weight:bold;padding:8px 0;border-top:1px solid #555;border-bottom:1px solid #555;margin:15px 0;}
button,.btn{display:block;width:100%;background:#1fa3ec;color:#ffffff;border:none;border-radius:6px;padding:12px 10px;font-size:18px;font-weight:normal;margin:8px 0;cursor:pointer;text-decoration:none;transition:background 0.2s;}
button:hover,.btn:hover{background:#1887c9;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
table.info-table{width:100%;border-collapse:collapse;text-align:left;font-size:14px;margin-bottom:10px;}
table.info-table td{padding:4px 0;vertical-align:top;}
table.info-table td.k{font-weight:bold;width:45%;color:#ffffff;}
table.info-table td.v{color:#dddddd;word-break:break-all;}
hr{border:0;border-top:1px solid #555;margin:10px 0;}
a{text-decoration:none;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>...</h1>
<h2 id='deviceSubHeader'>...</h2>

<div class='section-header'>Information</div>
<table class='info-table'>
<tr><td class='k'>Program Version</td><td class='v'><span id='infoVersion'>-</span></td></tr>
<tr><td class='k'>Build Date & Time</td><td class='v'><span id='infoBuildDate'>-</span></td></tr>
<tr><td class='k'>Core/SDK Version</td><td class='v'><span id='infoSdkVersion'>-</span></td></tr>
<tr><td class='k'>Uptime</td><td class='v'><span id='infoUptime'>-</span></td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>Hostname</td><td class='v'><span id='infoHostname'>-</span></td></tr>
<tr><td class='k'>MAC Address</td><td class='v'><span id='infoMac'>-</span></td></tr>
<tr><td class='k'>IP Address (WiFi)</td><td class='v'><span id='infoIp'>-</span></td></tr>
<tr><td class='k'>Gateway</td><td class='v'><span id='infoGw'>-</span></td></tr>
<tr><td class='k'>Subnet Mask</td><td class='v'><span id='infoMask'>-</span></td></tr>
<tr><td class='k'>DNS Server1</td><td class='v'><span id='infoDns'>-</span></td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>ESP Chip Id</td><td class='v'><span id='infoChipModel'>-</span></td></tr>
<tr><td class='k'>Flash Size</td><td class='v'><span id='infoFlashSize'>-</span></td></tr>
<tr><td class='k'>Free Memory</td><td class='v'><span id='infoRam'>-</span></td></tr>
</table>

<a href='/'><button>Main Menu</button></a>

<div class='footer-text' id='footerText'></div>
</div>

<script>
const rawAuth = '%WEB_USERNAME%:%WEB_PASSWORD%';
const authHeader = 'Basic ' + btoa(unescape(encodeURIComponent(rawAuth)));

function formatUptime(seconds){
  let days=Math.floor(seconds/86400);
  seconds%=86400;
  let hrs=String(Math.floor(seconds/3600)).padStart(2,'0');
  seconds%=3600;
  let mins=String(Math.floor(seconds/60)).padStart(2,'0');
  let secs=String(seconds%60).padStart(2,'0');
  return `${days}T${hrs}:${mins}:${secs}`;
}

function updateInfoPage() {
  // 1. Check if static info is already cached in sessionStorage
  const cached = sessionStorage.getItem('sys_info');
  if (cached) {
    const d = JSON.parse(cached);
    applyStaticInfo(d);
  }

  // 2. Always fetch /stats to retrieve dynamic data (uptime, freeHeap, IP, etc.)
  fetch('/stats', { headers: { 'Authorization': authHeader } })
    .then(r => r.json())
    .then(d => {
      // Update cache if not set yet
      if (!sessionStorage.getItem('sys_info')) {
        sessionStorage.setItem('sys_info', JSON.stringify(d));
        applyStaticInfo(d);
      }
      
      // Update real-time dynamic system information
      if (d.uptime !== undefined) document.getElementById('infoUptime').textContent = formatUptime(d.uptime);
      if (d.freeHeap !== undefined) document.getElementById('infoRam').textContent = (d.freeHeap / 1024).toFixed(1) + ' KB';
      if (d.rssi !== undefined) document.getElementById('infoRssi').textContent = d.rssi + ' dBm';
      if (d.ip !== undefined) document.getElementById('infoIp').textContent = d.ip;
    })
    .catch(e => console.log(e));
}

// Helper function to assign static header, footer, and hardware data to DOM elements
function applyStaticInfo(d) {
  if (d.headerTitle) document.getElementById('deviceHeader').textContent = d.headerTitle;
  if (d.headerSubTitle) document.getElementById('deviceSubHeader').textContent = d.headerSubTitle;
  if (d.footerText) document.getElementById('footerText').textContent = d.footerText;
  if (d.programVersion) document.getElementById('infoVersion').textContent = d.programVersion;
  if (d.buildDate) document.getElementById('infoBuildDate').textContent = d.buildDate;
  if (d.sdkVersion) document.getElementById('infoSdkVersion').textContent = d.sdkVersion;
  if (d.hostname) document.getElementById('infoHostname').textContent = d.hostname;
  if (d.mac !== undefined) document.getElementById('infoMac').textContent = d.mac;
  if (d.gw !== undefined) document.getElementById('infoGw').textContent = d.gw;
  if (d.mask !== undefined) document.getElementById('infoMask').textContent = d.mask;
  if (d.dns1 !== undefined) document.getElementById('infoDns').textContent = d.dns1;
  if (d.chipModel) document.getElementById('infoChipModel').textContent = d.chipModel;
  if (d.flashSize) document.getElementById('infoFlashSize').textContent = d.flashSize;
}
updateInfoPage();
</script>
</body>
</html>
)rawliteral";

#endif