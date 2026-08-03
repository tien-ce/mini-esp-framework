#ifndef INFO_HTML_H
#define INFO_HTML_H

#include <Arduino.h>

/**
 * @file info_html.h
 * @brief Embedded Web Dashboard UI - Tasmota Information Page.
 */

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
<tr><td class='k'>Flash Write Count</td><td class='v'>Unknown</td></tr>
<tr><td class='k'>Boot Count</td><td class='v'>Unknown</td></tr>
<tr><td class='k'>Restart Reason</td><td class='v'>Unknown</td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>AP1 Information</td><td class='v'>SSID Juniper_Secured<br>RSSI <span id='infoRssi'>-</span><br>Mode HT20<br>Channel 5<br>BSSId 9C:1C:12:8A:9B:00</td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>Hostname</td><td class='v'><span id='infoHostname'>-</span></td></tr>
<tr><td class='k'>IPv6 Local (WiFi)</td><td class='v'>fe80::2a84:85ff:fe55:1a24%st2</td></tr>
<tr><td class='k'>MAC Address</td><td class='v'><span id='infoMac'>-</span></td></tr>
<tr><td class='k'>IP Address (WiFi)</td><td class='v'><span id='infoIp'>-</span></td></tr>
<tr><td class='k'>Gateway</td><td class='v'><span id='infoGw'>-</span></td></tr>
<tr><td class='k'>Subnet Mask</td><td class='v'><span id='infoMask'>-</span></td></tr>
<tr><td class='k'>DNS Server1</td><td class='v'><span id='infoDns'>-</span></td></tr>
<tr><td class='k'>DNS Server2</td><td class='v'>0.0.0.0</td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>HTTP API</td><td class='v'>Disabled</td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>MQTT Host</td><td class='v'></td></tr>
<tr><td class='k'>MQTT Port</td><td class='v'>1883</td></tr>
<tr><td class='k'>MQTT TLS</td><td class='v'>Disabled</td></tr>
<tr><td class='k'>MQTT User</td><td class='v'>DVES_USER</td></tr>
<tr><td class='k'>MQTT Client</td><td class='v'>DVES_551A24</td></tr>
<tr><td class='k'>MQTT Topic</td><td class='v'>tasmota_%06X</td></tr>
<tr><td class='k'>MQTT Group Topic 1</td><td class='v'>cmnd/tasmotas/</td></tr>
<tr><td class='k'>MQTT Full Topic</td><td class='v'>cmnd/tasmota_551A24/</td></tr>
<tr><td class='k'>MQTT Fallback Topic</td><td class='v'>cmnd/DVES_551A24_fb/</td></tr>
<tr><td class='k'>MQTT No Retain</td><td class='v'>Disabled</td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>Emulation</td><td class='v'>None</td></tr>
<tr><td class='k'>mDNS Discovery</td><td class='v'>Disabled</td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>ESP Chip Id</td><td class='v'><span id='infoChipModel'>-</span></td></tr>
<tr><td class='k'>Flash Chip Id</td><td class='v'>0x184020 (QIO)</td></tr>
<tr><td class='k'>Flash Size</td><td class='v'><span id='infoFlashSize'>-</span></td></tr>
<tr><td class='k'>Program Flash Size</td><td class='v'>16384 KB</td></tr>
<tr><td class='k'>Program Size</td><td class='v'>2030 KB</td></tr>
<tr><td class='k'>Free Program Space</td><td class='v'>849 KB</td></tr>
<tr><td class='k'>Free Memory</td><td class='v'><span id='infoRam'>-</span></td></tr>
</table>
<hr>
<table class='info-table'>
<tr><td class='k'>Partition safeboot</td><td class='v'>832 KB (used 92%)</td></tr>
<tr><td class='k'>Partition app0*</td><td class='v'>2880 KB (used 70%)</td></tr>
<tr><td class='k'>Partition fs</td><td class='v'>12608 KB</td></tr>
</table>

<a href='/'><button>Main Menu</button></a>

<div class='footer-text' id='footerText'></div>
</div>

<script>
const authHeader='Basic '+btoa('%WEB_USERNAME%:%WEB_PASSWORD%');

function formatUptime(seconds){
  let days=Math.floor(seconds/86400);
  seconds%=86400;
  let hrs=String(Math.floor(seconds/3600)).padStart(2,'0');
  seconds%=3600;
  let mins=String(Math.floor(seconds/60)).padStart(2,'0');
  let secs=String(seconds%60).padStart(2,'0');
  return `${days}T${hrs}:${mins}:${secs}`;
}

function updateInfoPage(){
  fetch('/stats',{headers:{'Authorization':authHeader}})
    .then(r=>r.json())
    .then(d=>{
      if(d.headerTitle) document.getElementById('deviceHeader').textContent=d.headerTitle;
      if(d.headerSubTitle) document.getElementById('deviceSubHeader').textContent=d.headerSubTitle;
      if(d.footerText) document.getElementById('footerText').textContent=d.footerText;
      if(d.programVersion) document.getElementById('infoVersion').textContent=d.programVersion;
      if(d.buildDate) document.getElementById('infoBuildDate').textContent=d.buildDate;
      if(d.sdkVersion) document.getElementById('infoSdkVersion').textContent=d.sdkVersion;
      if(d.uptime!==undefined) document.getElementById('infoUptime').textContent=formatUptime(d.uptime);
      if(d.freeHeap!==undefined) document.getElementById('infoRam').textContent=(d.freeHeap/1024).toFixed(1)+' KB';
      if(d.rssi!==undefined) document.getElementById('infoRssi').textContent=d.rssi+' dBm';
      if(d.hostname) document.getElementById('infoHostname').textContent=d.hostname;
      if(d.mac!==undefined) document.getElementById('infoMac').textContent=d.mac;
      if(d.ip!==undefined) document.getElementById('infoIp').textContent=d.ip;
      if(d.gw!==undefined) document.getElementById('infoGw').textContent=d.gw;
      if(d.mask!==undefined) document.getElementById('infoMask').textContent=d.mask;
      if(d.dns1!==undefined) document.getElementById('infoDns').textContent=d.dns1;
      if(d.chipModel) document.getElementById('infoChipModel').textContent=d.chipModel;
      if(d.flashSize) document.getElementById('infoFlashSize').textContent=d.flashSize;
    }).catch(e=>console.log(e));
}

updateInfoPage();
</script>
</body>
</html>
)rawliteral";

#endif // INFO_HTML_H
