#ifndef MAIN_HTML_H
#define MAIN_HTML_H

#include <Arduino.h>

const char MAIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Main Menu</title>
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
.data-table {
  width: 100%;
  max-width: 320px;
  margin: 10px auto 20px auto;
  border-collapse: collapse;
  background: transparent;
  border-radius: 0;
  overflow: visible;
  resize: none;
}

.data-table td {
  padding: 3px 8px;
  border-bottom: none;
  font-size: 20px;
  line-height: 1.3;
}

.data-table td.label {
  text-align: left;
  color: #ffffff;
  font-weight: bold;
}

.data-table td.value {
  text-align: right;
  color: #ffffff;
  font-weight: bold;
}

</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>
<table class="data-table">
  %SENSOR_TABLE_ROWS%
</table>
<a href='/config'><button>Configuration</button></a>
<a href='/info'><button>Information</button></a>
<a href='/ota'><button>Firmware Upgrade</button></a>
<a href='/tools'><button>Tools</button></a>
<a href='/resetConfig' onclick="return confirm('Restart ESP32?');"><button class='btn-red'>Restart</button></a>


<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>

<script>
// Initialize WebSocket connection to /ws-home endpoint
var gateway = `ws://${window.location.host}/ws-home`;
var websocket;

function initWebSocket() {
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function onOpen(event) {
  console.log('WebSocket connection established');
}

function onClose(event) {
  console.log('WebSocket connection closed, retrying...');
  setTimeout(initWebSocket, 2000); // Auto-reconnect after 2s
}

function onMessage(event) {
  try {
    // Parse received batch JSON payload: [{"id":1,"val":"26.5"},{"id":2,"val":"60"}]
    var updates = JSON.parse(event.data);
    
    // Update DOM elements matching id "val-X"
    updates.forEach(function(item) {
      var elem = document.getElementById('val-' + item.id);
      if (elem) {
        elem.innerText = item.val;
      }
    });
  } catch (e) {
    console.error('Invalid JSON payload:', e);
  }
}

window.addEventListener('load', initWebSocket);
</script>

</body>
</html>
)rawliteral";

#endif
