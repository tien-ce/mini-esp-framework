#ifndef CONFIG_MODULE_HTML_H
#define CONFIG_MODULE_HTML_H

#include <Arduino.h>

const char CONFIG_MODULE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Configuration Module</title>
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
.form-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:6px;}
.form-row label{font-size:14px;font-weight:bold;color:#ffffff;width:35%;}
.form-row label.gpio-red{color:#ff5555;}
.form-row input[type=text], .form-row select{width:62%;padding:5px 8px;border:1px solid #777;border-radius:3px;background:#e0e0e0;color:#000000;font-size:13px;}
hr.box-divider{border:0;border-top:1px solid #555;margin:10px 0;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>

<form id='moduleForm' action='/saveModule' method='POST'>
<fieldset class='param-box'>
<legend>Template parameters</legend>

<div class='form-row'>
  <label for='name'>Name</label>
  <input type='text' id='name' name='name' value='Generic'>
</div>

<div class='form-row'>
  <label for='basedOn'>Based on</label>
  <select id='basedOn' name='basedOn'>
    <option value='18' selected>Generic (18)</option>
    <option value='1'>Sonoff Basic (1)</option>
  </select>
</div>

<hr class='box-divider'>

%GPIO_TABLE_ROWS%

<button type='submit' class='btn-green'>Save</button>
</fieldset>
</form>

<a href='/config'><button>Configuration</button></a>

<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>
</body>
</html>
)rawliteral";

#endif
