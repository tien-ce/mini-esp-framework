#ifndef MAIN_HTML_H
#define MAIN_HTML_H

#include <Arduino.h>

/**
 * @file main_html.h
 * @brief Embedded Web Dashboard UI - Tasmota Main Menu Page.
 */

const char MAIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Tasmota</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#232323;color:#ffffff;text-align:center;padding:20px 10px;}
.wrapper{max-width:440px;margin:0 auto;}
h1{font-size:22px;font-weight:bold;margin-bottom:4px;color:#ffffff;}
h2{font-size:26px;font-weight:bold;margin-bottom:20px;color:#ffffff;}
button,.btn{display:block;width:100%;background:#1fa3ec;color:#ffffff;border:none;border-radius:6px;padding:12px 10px;font-size:18px;font-weight:normal;margin:8px 0;cursor:pointer;text-decoration:none;transition:background 0.2s;}
button:hover,.btn:hover{background:#1887c9;}
button.btn-red{background:#d43f3a;}
button.btn-red:hover{background:#b92c28;}
button.inactive{opacity:0.9;cursor:default;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
a{text-decoration:none;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>...</h1>
<h2 id='deviceSubHeader'>...</h2>

<button class='inactive'>Configuration</button>
<a href='/info'><button>Information</button></a>
<a href='/ota'><button>Firmware Upgrade</button></a>
<a href='/tools'><button>Tools</button></a>
<button class='btn-red inactive'>Restart</button>

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

loadSystemInfo();
</script>
</body>
</html>
)rawliteral";

#endif // MAIN_HTML_H
