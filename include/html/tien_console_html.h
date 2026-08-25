#ifndef TIEN_CONSOLE_HTML_H
#define TIEN_CONSOLE_HTML_H

#include <Arduino.h>

const char TIEN_CONSOLE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Tien Script Console</title>
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
.btn-clear{background:#6c757d;margin-bottom:12px;}
.btn-clear:hover{background:#5a6268;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
#terminal{background:#000000;color:#00e5ff;padding:14px;height:450px;overflow-y:auto;font-family:monospace;font-size:15px;line-height:1.4;border-radius:4px;text-align:left;margin-bottom:12px;border:1px solid #444;white-space:pre-wrap;}
#terminal div{margin-bottom:3px;word-wrap:break-word;}
textarea{width:100%;height:80px;padding:10px 12px;border:1px solid #ccc;border-radius:4px;font-size:15px;font-family:monospace;background:#ffffff;color:#000000;margin-bottom:10px;resize:vertical;}
a{text-decoration:none;}
.btn-group{display:flex;gap:10px;}
.btn-group button{flex:1;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>Tien Script Console</h2>

<div id='terminal'></div>
<textarea id='cmdInput' placeholder='Enter Tien script expression or code... (Ctrl+Enter to execute)'></textarea>
<div class='btn-group'>
    <button class='btn-send' onclick='sendConsoleCmd()'>Execute</button>
    <button class='btn-clear' onclick='clearTerminal()'>Clear Output</button>
</div>
<a href='/tools'><button>Tools Menu</button></a>
<a href='/'><button>Main Menu</button></a>

<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>

<script>
const MAX_LINES=300;
let gateway = 'ws://' + window.location.host + '/ws_tien';
let wsConn;

function initWS(){
  wsConn=new WebSocket(gateway);
  wsConn.onopen=()=>{console.log('Tien WS connected');};
  wsConn.onclose=()=>{console.log('Tien WS disconnected');setTimeout(initWS,2000);};
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
      console.log("Sent script via WS:", cmd);
  } else {
      console.error("Tien WS not open!");
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

function clearTerminal(){
  document.getElementById('terminal').innerHTML='';
}

document.getElementById('cmdInput').addEventListener('keydown', function(e) {
  if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
    e.preventDefault();
    sendConsoleCmd();
  }
});

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

#endif // TIEN_CONSOLE_HTML_H
