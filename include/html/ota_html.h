#ifndef OTA_HTML_H
#define OTA_HTML_H

#include <Arduino.h>

/**
 * @file ota_html.h
 * @brief Embedded Web Dashboard UI - Tasmota Firmware Upgrade Page.
 */

const char OTA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Tasmota - Firmware Upgrade</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#232323;color:#ffffff;text-align:center;padding:20px 10px;}
.wrapper{max-width:440px;margin:0 auto;}
h1{font-size:22px;font-weight:bold;margin-bottom:4px;color:#ffffff;}
h2{font-size:26px;font-weight:bold;margin-bottom:20px;color:#ffffff;}
.section-header{font-size:20px;font-weight:bold;padding:8px 0;border-top:1px solid #555;border-bottom:1px solid #555;margin:15px 0;}
button,.btn{display:block;width:100%;background:#1fa3ec;color:#ffffff;border:none;border-radius:6px;padding:12px 10px;font-size:18px;font-weight:normal;margin:8px 0;cursor:pointer;text-decoration:none;transition:background 0.2s;}
button:hover,.btn:hover{background:#1887c9;}
button.inactive{opacity:0.9;cursor:default;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
fieldset{border:1px solid #777;border-radius:4px;padding:15px;margin-bottom:15px;text-align:left;}
legend{font-size:16px;font-weight:bold;color:#ffffff;padding:0 8px;}
label{font-size:14px;font-weight:bold;display:block;margin-bottom:6px;}
input[type=text]{width:100%;padding:8px;border:1px solid #ccc;border-radius:4px;font-size:14px;background:#ffffff;color:#000000;}
input[type=file]{margin:10px 0;width:100%;background:#e0e0e0;color:#000000;padding:6px;border-radius:4px;}
#status{margin-top:10px;padding:8px;border-radius:4px;display:none;font-size:13px;}
.success-msg{background:#d4edda;color:#155724;}
.error-msg{background:#f8d7da;color:#721c24;}
a{text-decoration:none;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>...</h1>
<h2 id='deviceSubHeader'>...</h2>

<div class='section-header'>Firmware Upgrade</div>
<fieldset>
<legend>Use web server</legend>
<label>OTA Url</label>
<input type='text' id='otaUrl' value='http://ota.tasmota.com/tasmota32/release/tasmota32.bin'>
<button style='margin-top:10px;' class='inactive'>Start upgrade</button>
</fieldset>

<fieldset>
<legend>Use file upload</legend>
<form id='uploadForm'>
<input type='file' id='file' accept='.bin' required>
<button type='submit'>Start upgrade</button>
</form>
</fieldset>
<div id='status'></div>

<a href='/'><button>Main Menu</button></a>

<div class='footer-text' id='footerText'></div>
</div>

<script>
const authHeader='Basic '+btoa('%WEB_USERNAME%:%WEB_PASSWORD%');

function loadSystemInfo(){
  fetch('/stats',{headers:{'Authorization':authHeader}})
    .then(r=>r.json())
    .then(d=>{
      if(d.headerTitle) document.getElementById('deviceHeader').textContent=d.headerTitle;
      if(d.headerSubTitle) document.getElementById('deviceSubHeader').textContent=d.headerSubTitle;
      if(d.footerText) document.getElementById('footerText').textContent=d.footerText;
    }).catch(e=>console.log(e));
}

document.getElementById('uploadForm').onsubmit=async(e)=>{
  e.preventDefault();
  const file=document.getElementById('file').files[0];
  if(!file){alert('Select a file');return;}
  const st=document.getElementById('status');
  st.style.display='block';
  st.className='success-msg';
  st.textContent='Uploading firmware...';
  const formData=new FormData();
  formData.append('file',file);
  try{
    const xhr=new XMLHttpRequest();
    xhr.onload=()=>{
      if(xhr.status===200){st.className='success-msg';st.textContent='Upload OK! Rebooting...';setTimeout(()=>window.location.href='/',5000);}
      else{st.className='error-msg';st.textContent='Upload failed!';}
    };
    xhr.onerror=()=>{st.className='error-msg';st.textContent='Connection error!';};
    xhr.open('POST','/doUpdate');
    xhr.setRequestHeader('Authorization',authHeader);
    xhr.send(formData);
  }catch(err){
    st.className='error-msg';st.textContent='Error: '+err.message;
  }
};

loadSystemInfo();
</script>
</body>
</html>
)rawliteral";

#endif // OTA_HTML_H
