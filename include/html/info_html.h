#ifndef INFO_HTML_H
#define INFO_HTML_H

#include <Arduino.h>

const char INFO_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Information</title>
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
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>

<div class='section-header'>Information</div>
<table class='info-table'>
<tr><td class='k'>Program Version</td><td class='v'>%APP_VERSION%</td></tr>
<tr><td class='k'>Build Date & Time</td><td class='v'>%BUILD_DATE%</td></tr>
<tr><td class='k'>Core/SDK Version</td><td class='v'>%SDK_VERSION%</td></tr>
<tr><td class='k'>Uptime</td><td class='v'><span id='infoUptime'>-</span></td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>Hostname</td><td class='v'>%HOSTNAME%</td></tr>
<tr><td class='k'>MAC Address</td><td class='v'>%MAC_ADDR%</td></tr>
<tr><td class='k'>IP Address (WiFi)</td><td class='v'><span id='infoIp'>-</span></td></tr>
<tr><td class='k'>Gateway</td><td class='v'><span id='infoGw'>-</span></td></tr>
<tr><td class='k'>Subnet Mask</td><td class='v'><span id='infoMask'>-</span></td></tr>
<tr><td class='k'>DNS Server1</td><td class='v'><span id='infoDns'>-</span></td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>ESP Chip Id</td><td class='v'>%CHIP_MODEL%</td></tr>
<tr><td class='k'>Flash Size</td><td class='v'>%FLASH_SIZE%</td></tr>
<tr><td class='k'>Free Memory</td><td class='v'><span id='infoRam'>-</span></td></tr>
</table>

<a href='/'><button>Main Menu</button></a>

<div class='footer-text' id='footerText'>%FOOTER_TEXT%</div>
</div>

<script>
function formatUptime(seconds){
  let days = Math.floor(seconds / 86400);
  seconds %= 86400;
  let hrs = String(Math.floor(seconds / 3600)).padStart(2, '0');
  seconds %= 3600;
  let mins = String(Math.floor(seconds / 60)).padStart(2, '0');
  let secs = String(seconds % 60).padStart(2, '0');
  return `${days}T${hrs}:${mins}:${secs}`;
}

function updateDynamicInfo() {
  fetch('/stats')
    .then(r => r.json())
    .then(d => {
      if (d.uptime !== undefined) document.getElementById('infoUptime').textContent = formatUptime(d.uptime);
      if (d.freeHeap !== undefined) document.getElementById('infoRam').textContent = (d.freeHeap / 1024).toFixed(1) + ' KB';
      if (d.ip !== undefined) document.getElementById('infoIp').textContent = d.ip;
      if (d.gw !== undefined) document.getElementById('infoGw').textContent = d.gw;
      if (d.mask !== undefined) document.getElementById('infoMask').textContent = d.mask;
      if (d.dns1 !== undefined) document.getElementById('infoDns').textContent = d.dns1;
    })
    .catch(e => console.log(e));
}

updateDynamicInfo();
</script>
</body>
</html>
)rawliteral";

#endif
