#ifndef CONSOLE_HTML_H
#define CONSOLE_HTML_H

#include <Arduino.h>

const char CONSOLE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Console</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#232323;color:#ffffff;text-align:center;padding:20px 10px;}
.wrapper{width:85%;max-width:1000px;margin:0 auto;}
h1{font-size:22px;font-weight:bold;margin-bottom:4px;color:#ffffff;}
h2{font-size:26px;font-weight:bold;margin-bottom:20px;color:#ffffff;}
button,.btn{display:block;width:100%;background:#1fa3ec;color:#ffffff;border:none;border-radius:6px;padding:12px 10px;font-size:18px;font-weight:normal;margin:8px 0;cursor:pointer;text-decoration:none;transition:background 0.2s;}
button:hover,.btn:hover{background:#1887c9;}
.btn-send{background:#28a745;margin-bottom:12px;}
.btn-send:hover{background:#218838;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
#terminal{background:#000000;color:#00ff00;padding:14px;height:450px;overflow-y:auto;font-family:monospace;font-size:15px;line-height:1.4;border-radius:4px;text-align:left;margin-bottom:12px;border:1px solid #444;}
#terminal div{margin-bottom:3px;word-wrap:break-word;}
input[type=text]{width:100%;padding:10px 12px;border:1px solid #ccc;border-radius:4px;font-size:15px;background:#ffffff;color:#000000;margin-bottom:10px;}
a{text-decoration:none;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>

<div id='terminal'></div>
<input type='text' id='cmdInput' placeholder='Enter command' onkeypress='if(event.key==="Enter")sendConsoleCmd()'>
<button class='btn-send' onclick='sendConsoleCmd()'>Send</button>
<a href='/'><button>Main Menu</button></a>

<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>

<script>
const MAX_LINES=200;
let gateway = 'ws://' + window.location.host + '/ws';
let wsConn;

function initWS(){
  wsConn=new WebSocket(gateway);
  wsConn.onopen=()=>{console.log('WS connected');};
  wsConn.onclose=()=>{console.log('WS disconnected');setTimeout(initWS,2000);};
  wsConn.onmessage=(e)=>{
    let term=document.getElementById('terminal');
    let line=document.createElement('div');
    line.innerHTML = e.data;
    term.appendChild(line);
    while(term.children.length>MAX_LINES) term.removeChild(term.firstChild);
    term.scrollTop=term.scrollHeight;
  };
}

function sendCmd(cmd){
  if (wsConn && wsConn.readyState === WebSocket.OPEN) {
      wsConn.send(cmd);
      console.log("Sent via WS:", cmd);
  } else {
      console.error("WS not open!");
  }  
}

function sendConsoleCmd(){
  let input=document.getElementById('cmdInput');
  let val=input.value.trim();
  if(val.length>0){
    sendCmd(val);
    input.value='';
  }
}

window.addEventListener('beforeunload', function() {
  if (wsConn) {
    wsConn.onclose = function() {};
    wsConn.close();
  }
});

initWS();
</script>
</body>
</html>
)rawliteral";

#endif
