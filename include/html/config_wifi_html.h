#ifndef CONFIG_WIFI_HTML_H
#define CONFIG_WIFI_HTML_H

#include <Arduino.h>

const char CONFIG_WIFI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Configure WiFi</title>
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
.form-row input[type=text], .form-row input[type=password]{width:58%;padding:6px 8px;border:1px solid #777;border-radius:3px;background:#e0e0e0;color:#000000;font-size:13px;}
hr.box-divider{border:0;border-top:1px solid #555;margin:12px 0;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>

<form id='wifiForm' action='/saveWifi' method='POST'>
<fieldset class='param-box'>
<legend>WiFi parameters</legend>

<div class='form-row'>
  <label for='client_id'>Host / Client ID</label>
  <input type='text' id='client_id' name='client_id' value='%WIFI_CLIENT_ID%' placeholder='ESP32_DEVICE'>
</div>

<div class='form-row'>
  <label for='ssid'>AP1 SSID</label>
  <input type='text' id='ssid' name='ssid' value='%WIFI_SSID%' required>
</div>

<div class='form-row'>
  <label for='pass'>AP1 Password</label>
  <input type='password' id='pass' name='pass' value='%WIFI_PASSWORD%'>
</div>

<hr class='box-divider'>

<div class='form-row'>
  <label for='ip'>Static IP</label>
  <input type='text' id='ip' name='ip' value='%STATIC_IP%' placeholder='192.168.1.100'>
</div>

<div class='form-row'>
  <label for='gw'>Gateway</label>
  <input type='text' id='gw' name='gw' value='%STATIC_GATEWAY%' placeholder='192.168.1.1'>
</div>

<div class='form-row'>
  <label for='sn'>Subnetmask</label>
  <input type='text' id='sn' name='sn' value='%STATIC_SUBNET%' placeholder='255.255.255.0'>
</div>

<div class='form-row'>
  <label for='dns'>DNS Server</label>
  <input type='text' id='dns' name='dns' value='%STATIC_DNS1%' placeholder='8.8.8.8'>
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
document.getElementById('wifiForm').addEventListener('submit', function(e) {
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

#endif // CONFIG_WIFI_HTML_H
