#ifndef CONFIG_MQTT_HTML_H
#define CONFIG_MQTT_HTML_H

#include <Arduino.h>

const char CONFIG_MQTT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Configure MQTT</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#232323;color:#ffffff;text-align:center;padding:20px 10px;}
.wrapper{max-width:440px;margin:0 auto;}
h1{font-size:22px;font-weight:bold;margin-bottom:4px;color:#ffffff;}
h2{font-size:26px;font-weight:bold;margin-bottom:20px;color:#ffffff;}
button,.btn{display:block;width:100%;background:#1fa3ec;color:#ffffff;border:none;border-radius:6px;padding:12px 10px;font-size:18px;font-weight:normal;margin:8px 0;cursor:pointer;text-decoration:none;transition:background 0.2s;}
button:hover,.btn:hover{background:#1887c9;}
.btn-green{background:#28a745;}
.btn-green:hover{background:#218838;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
a{text-decoration:none;}
fieldset.param-box{border:1px solid #666;border-radius:4px;padding:12px;margin-bottom:15px;background:#333333;text-align:left;}
legend{font-size:15px;font-weight:bold;color:#ffffff;padding:0 6px;}
.form-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px;}
.form-row label{font-size:14px;font-weight:bold;color:#ffffff;width:38%;}
.form-row input[type=text], .form-row input[type=number], .form-row input[type=password]{width:58%;padding:6px 8px;border:1px solid #777;border-radius:3px;background:#e0e0e0;color:#000000;font-size:13px;}
hr.box-divider{border:0;border-top:1px solid #555;margin:12px 0;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>

<form id='mqttForm' action='/saveMqtt' method='POST'>
<fieldset class='param-box'>
<legend>MQTT parameters</legend>

<div class='form-row'>
  <label for='server'>Host / Server</label>
  <input type='text' id='server' name='server' value='%MQTT_SERVER%' placeholder='192.168.1.100'>
</div>

<div class='form-row'>
  <label for='port'>Port</label>
  <input type='number' id='port' name='port' value='%MQTT_PORT%' placeholder='1883'>
</div>

<div class='form-row'>
  <label for='user'>Client / User</label>
  <input type='text' id='user' name='user' value='%MQTT_USER%'>
</div>

<div class='form-row'>
  <label for='pass'>Password</label>
  <input type='password' id='pass' name='pass' value='%MQTT_PASSWORD%'>
</div>

<hr class='box-divider'>

<div class='form-row'>
  <label for='topic'>Telemetry Topic</label>
  <input type='text' id='topic' name='topic' value='%MQTT_TOPIC%' placeholder='tele/%topic%/STATE'>
</div>

<div class='form-row'>
  <label for='rpc_topic'>RPC Topic</label>
  <input type='text' id='rpc_topic' name='rpc_topic' value='%MQTT_RPC_TOPIC%' placeholder='cmnd/%topic%'>
</div>

<div class='form-row'>
  <label for='interval'>Interval (s)</label>
  <input type='number' id='interval' name='interval' value='%MQTT_INTERVAL%' placeholder='10'>
</div>

<button type='submit' class='btn-green' id='saveBtn'>Save</button>
</fieldset>
</form>

<a href='/config'><button>Configuration</button></a>

<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>

<script>
document.getElementById('mqttForm').addEventListener('submit', function(e) {
  e.preventDefault();
  var btn = document.getElementById('saveBtn');
  btn.disabled = true;
  btn.textContent = 'Saving & Restarting...';
  var formData = new URLSearchParams(new FormData(this));
  fetch(this.action, { method: 'POST', body: formData })
    .then(function() {
      setTimeout(function() { window.location.href = '/'; }, 3000);
    })
    .catch(function() {
      setTimeout(function() { window.location.href = '/'; }, 3000);
    });
});
</script>
</body>
</html>
)rawliteral";

#endif // CONFIG_MQTT_HTML_H
