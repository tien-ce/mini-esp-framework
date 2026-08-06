#ifndef CONFIG_HTML_H
#define CONFIG_HTML_H

#include <Arduino.h>

const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Configuration</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#232323;color:#ffffff;text-align:center;padding:20px 10px;}
.wrapper{max-width:440px;margin:0 auto;}
h1{font-size:22px;font-weight:bold;margin-bottom:4px;color:#ffffff;}
h2{font-size:26px;font-weight:bold;margin-bottom:20px;color:#ffffff;}
.section-header{font-size:20px;font-weight:bold;padding:8px 0;border-top:1px solid #555;border-bottom:1px solid #555;margin:15px 0;}
button,.btn{display:block;width:100%;background:#1fa3ec;color:#ffffff;border:none;border-radius:6px;padding:12px 10px;font-size:18px;font-weight:normal;margin:8px 0;cursor:pointer;text-decoration:none;transition:background 0.2s;}
button:hover,.btn:hover{background:#1887c9;}
button.btn-red{background:#d43f3a;}
button.btn-red:hover{background:#b92c28;}
button.inactive{opacity:0.9;cursor:default;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
a{text-decoration:none;}
.matter-icon{display:inline-block;width:16px;height:16px;vertical-align:middle;margin-right:6px;fill:currentColor;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>

<div class='section-header'>Configuration</div>
<a href='/config-module'><button>Module</button></a>
<button class='inactive'>WiFi</button>
<button class='inactive'>MQTT</button>
<button class='inactive'>Domoticz</button>
<button class='inactive'>Timer</button>
<button class='inactive'>KNX</button>
<button class='inactive'>Auto-Conf</button>
<button class='inactive'><svg class='matter-icon' viewBox='0 0 24 24'><path d='M12 2L2 7v10l10 5 10-5V7L12 2zm0 2.8L19 8v8l-7 3.5L5 16V8l7-3.2z'/></svg>Matter</button>
<button class='inactive'>Logging</button>
<button class='inactive'>Other</button>
<button class='inactive'>Template</button>
<button class='btn-red' onclick="if(confirm('Reset Configuration?')) fetch('/resetConfig').then(()=>location.href='/')">Reset</button>
<button class='inactive'>Backup</button>
<button class='inactive'>Restore</button>
<a href='/'><button>Main Menu</button></a>

<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>
</body>
</html>
)rawliteral";

#endif
