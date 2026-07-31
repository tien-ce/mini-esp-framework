#ifndef INDEX_HTML_H
#define INDEX_HTML_H

#include <Arduino.h>

/**
 * @file index_html.h
 * @brief Embedded Web Dashboard UI (HTML, CSS, JavaScript) for Sensor Monitor.
 */

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Sensor Monitor</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#f0f0f0;}
.header{background:#27ae60;color:white;padding:15px 20px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;}
.header h1{font-size:20px;}
.header .info{font-size:13px;opacity:0.9;}
.tabs{background:white;display:flex;box-shadow:0 2px 5px rgba(0,0,0,0.1);}
.tab{flex:1;padding:15px;text-align:center;cursor:pointer;border-bottom:3px solid transparent;transition:all 0.3s;}
.tab:hover{background:#ecf0f1;}
.tab.active{border-bottom-color:#27ae60;color:#27ae60;font-weight:bold;}
.content{display:none;padding:20px;}
.content.active{display:block;}
#terminal{background:#1e1e1e;color:#00ff00;padding:15px;height:500px;overflow-y:auto;font-family:monospace;font-size:14px;border-radius:5px;}
#terminal div{margin-bottom:5px;word-wrap:break-word;}
.upload-box,.settings-box{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:600px;margin:0 auto;}
.upload-box h2,.settings-box h2{margin-bottom:20px;color:#333;}
.settings-box h3{margin-top:30px;margin-bottom:15px;color:#555;border-bottom:2px solid #27ae60;padding-bottom:10px;}
.form-group{margin-bottom:15px;}
.form-group label{display:block;margin-bottom:5px;color:#555;font-weight:bold;}
.form-group input[type=text],.form-group input[type=password],.form-group input[type=number]{width:100%;padding:10px;border:1px solid #ccc;border-radius:5px;font-size:14px;}
.form-group small{color:#777;font-size:12px;display:block;margin-top:5px;}
input[type=file]{margin:20px 0;padding:10px;width:100%;border:2px dashed #ccc;border-radius:5px;}
button{background:#27ae60;color:white;padding:12px 30px;border:none;border-radius:5px;font-size:16px;cursor:pointer;width:100%;margin-top:10px;}
button:hover{background:#229954;}
button.restart{background:#8e44ad;margin-top:15px;}
button.restart:hover{background:#7d3c98;}
#progress{display:none;margin-top:20px;}
.progress-bar{background:#ecf0f1;border-radius:5px;height:30px;overflow:hidden;}
.progress-fill{background:#27ae60;height:100%;width:0%;transition:width 0.3s;text-align:center;color:white;line-height:30px;}
#status,#settingsStatus{margin-top:15px;padding:10px;border-radius:5px;display:none;}
.success-msg{background:#d4edda;color:#155724;display:block!important;}
.error-msg{background:#f8d7da;color:#721c24;display:block!important;}
.stat-box{margin-top:15px;padding:15px;background:#ecf0f1;border-radius:5px;display:grid;grid-template-columns:1fr 1fr;gap:10px;}
.stat-item{text-align:center;}
.stat-item .val{font-size:24px;font-weight:bold;color:#27ae60;}
.stat-item .lbl{font-size:12px;color:#777;margin-top:5px;}
</style>
</head>
<body>
<div class='header'>
<h1>SENSOR Monitor (FreeRTOS)</h1>
<div class='info'>IP: %LOCAL_IP% | RAM: <span id='ram'>-</span> KB | FW: %FIRMWARE_VERSION% | Up: <span id='uptime'>0</span>s</div>
</div>

<div class='tabs'>
<div class='tab active' onclick='openTab(0)'>Serial Monitor</div>
<div class='tab' onclick='openTab(1)'>Settings</div>
<div class='tab' onclick='openTab(2)'>Firmware Update</div>
</div>

<!-- TAB 0: SERIAL MONITOR -->
<div class='content active' id='tab0'>
<div class='stat-box'>
<div class='stat-item'>
<div class='val' id='sensorCount'>0</div>
<div class='lbl'>Sensor Count</div>
</div>
<div class='stat-item'>
<div class='val' id='rssiVal'>-</div>
<div class='lbl'>RSSI (dBm)</div>
</div>
</div>
<button class='restart' onclick='if(confirm("Restart ESP32?"))sendCmd("ESP_RESTART")'>RESTART ESP32</button>
<div id='terminal' style='margin-top:15px;'></div>
</div>

<!-- TAB 1: SETTINGS -->
<div class='content' id='tab1'>
<div class='settings-box'>
<h2>Settings</h2>

<form id='settingsForm'>
<h3>WiFi Configuration</h3>
<div class='form-group'>
<label>WiFi SSID</label>
<input type='text' id='wifiSSID' required>
</div>
<div class='form-group'>
<label>WiFi Password</label>
<input type='password' id='wifiPass' required>
<small>Static IP address defined in code</small>
</div>

<h3>Device Configuration</h3>
<div class='form-group'>
<label>Client ID</label>
<input type='text' id='deviceClientID' required>
<small>E.g., ANDONA7, ANDONB3, SENSOR_LINE1...</small>
</div>
<div class='form-group'>
<label>API URL</label>
<input type='text' id='apiUrl' required>
<small>API URL for logging sensor data</small>
</div>

<button type='submit'>Save & Restart</button>
</form>
<div id='settingsStatus'></div>
</div>
</div>

<!-- TAB 2: FIRMWARE UPDATE -->
<div class='content' id='tab2'>
<div class='upload-box'>
<h2>Firmware Update</h2>
<form id='uploadForm'>
<input type='file' id='file' accept='.bin' required>
<button type='submit'>Upload Firmware</button>
</form>
<div id='progress'>
<div class='progress-bar'>
<div class='progress-fill' id='bar'>0%</div>
</div>
</div>
<div id='status'></div>
</div>
</div>

<script>
const MAX_LINES=200;
const authHeader='Basic '+btoa('%WEB_USERNAME%:%WEB_PASSWORD%');
let gateway=`ws://${window.location.host}/ws`;
let wsConn;

function initWS(){
  wsConn=new WebSocket(gateway);
  wsConn.onopen=()=>{console.log('WS connected');};
  wsConn.onclose=()=>{console.log('WS disconnected');setTimeout(initWS,2000);};
  wsConn.onmessage=(e)=>{
    let term=document.getElementById('terminal');
    let line=document.createElement('div');
    line.innerHTML=e.data;
    term.appendChild(line);
    while(term.children.length>MAX_LINES) term.removeChild(term.firstChild);
    term.scrollTop=term.scrollHeight;
  };
}

function openTab(n){
  let tabs=document.getElementsByClassName('tab');
  let contents=document.getElementsByClassName('content');
  for(let i=0;i<tabs.length;i++){
    tabs[i].classList.remove('active');
    contents[i].classList.remove('active');
  }
  tabs[n].classList.add('active');
  contents[n].classList.add('active');
  if(n===1) loadSettings();
}

function sendCmd(cmd){
  fetch('/cmd?msg='+cmd,{headers:{'Authorization':authHeader}})
    .then(r=>r.text()).then(d=>console.log(d))
    .catch(e=>console.log(e));
}

function updateStats(){
  fetch('/stats',{headers:{'Authorization':authHeader}})
    .then(r=>r.json())
    .then(d=>{
      document.getElementById('uptime').textContent=d.uptime;
      document.getElementById('ram').textContent=(d.freeHeap/1024).toFixed(1);
      document.getElementById('sensorCount').textContent=d.count;
      document.getElementById('rssiVal').textContent=d.rssi;
    }).catch(e=>console.log(e));
}

function loadSettings(){
  fetch('/getConfig',{headers:{'Authorization':authHeader}})
    .then(r=>r.json())
    .then(d=>{
      document.getElementById('wifiSSID').value=d.wifiSSID;
      document.getElementById('wifiPass').value=d.wifiPass;
      document.getElementById('deviceClientID').value=d.clientID;
      document.getElementById('apiUrl').value=d.apiUrl;
    }).catch(e=>console.log(e));
}

document.getElementById('settingsForm').onsubmit=async(e)=>{
  e.preventDefault();
  const config={
    wifiSSID:document.getElementById('wifiSSID').value,
    wifiPass:document.getElementById('wifiPass').value,
    clientID:document.getElementById('deviceClientID').value,
    apiUrl:document.getElementById('apiUrl').value
  };
  try{
    const r=await fetch('/saveConfig',{method:'POST',headers:{'Content-Type':'application/json','Authorization':authHeader},body:JSON.stringify(config)});
    const st=document.getElementById('settingsStatus');
    if(r.ok){st.className='success-msg';st.textContent='Saved! Restarting in 5s...';st.style.display='block';setTimeout(()=>window.location.href='/',5000);}
    else{st.className='error-msg';st.textContent='Failed to save!';st.style.display='block';}
  }catch(err){
    const st=document.getElementById('settingsStatus');
    st.className='error-msg';st.textContent='Error: '+err.message;st.style.display='block';
  }
};

document.getElementById('uploadForm').onsubmit=async(e)=>{
  e.preventDefault();
  const file=document.getElementById('file').files[0];
  if(!file){alert('Select a file');return;}
  document.getElementById('progress').style.display='block';
  document.getElementById('status').style.display='none';
  const formData=new FormData();
  formData.append('file',file);
  try{
    const xhr=new XMLHttpRequest();
    xhr.upload.addEventListener('progress',(e)=>{
      if(e.lengthComputable){
        const p=Math.round((e.loaded/e.total)*100);
        document.getElementById('bar').style.width=p+'%';
        document.getElementById('bar').textContent=p+'%';
      }
    });
    xhr.onload=()=>{
      const st=document.getElementById('status');
      if(xhr.status===200){st.className='success-msg';st.textContent='Upload OK! Rebooting...';st.style.display='block';setTimeout(()=>window.location.reload(),5000);}
      else{st.className='error-msg';st.textContent='Upload failed!';st.style.display='block';}
    };
    xhr.onerror=()=>{const st=document.getElementById('status');st.className='error-msg';st.textContent='Connection error!';st.style.display='block';};
    xhr.open('POST','/doUpdate');
    xhr.setRequestHeader('Authorization',authHeader);
    xhr.send(formData);
  }catch(err){
    const st=document.getElementById('status');st.className='error-msg';st.textContent='Error: '+err.message;st.style.display='block';
  }
};

initWS();
updateStats();
setInterval(updateStats,3000);
</script>
</body>
</html>
)rawliteral";

#endif // INDEX_HTML_H
