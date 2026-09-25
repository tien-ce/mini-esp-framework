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
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  background: #14161d;
  color: #e2e4e9;
  text-align: center;
  padding: 16px 10px 24px;
  min-height: 100vh;
}
.wrapper {
  width: 94%;
  max-width: 1080px;
  margin: 0 auto;
  text-align: left;
}
h1 {
  font-size: 20px;
  font-weight: 700;
  margin-bottom: 4px;
  color: #f0f2f5;
  text-align: center;
  letter-spacing: 0.3px;
}
h2 {
  font-size: 23px;
  font-weight: 700;
  margin-bottom: 16px;
  color: #00d2ff;
  text-align: center;
  letter-spacing: 0.5px;
}

.toolbar {
  display: flex;
  gap: 12px;
  align-items: center;
  margin-bottom: 12px;
  flex-wrap: wrap;
  background: #1c1f28;
  padding: 10px 14px;
  border-radius: 6px;
  border: 1px solid #2b303c;
}
.toolbar label {
  font-size: 13.5px;
  color: #9ba3b4;
  font-weight: 600;
}
.toolbar input[type='text'] {
  flex: 1;
  min-width: 180px;
  padding: 8px 12px;
  background: #12141a;
  color: #f0f2f5;
  border: 1px solid #363c4a;
  border-radius: 4px;
  font-size: 13.5px;
  font-family: Consolas, Menlo, monospace;
  outline: none;
  transition: border-color 0.2s;
}
.toolbar input[type='text']:focus {
  border-color: #528bff;
}

/* Editor Container */
.editor-container {
  background: #181a20;
  border: 1px solid #2b303c;
  border-radius: 8px;
  margin-bottom: 14px;
  box-shadow: 0 6px 20px rgba(0, 0, 0, 0.5);
  display: flex;
  flex-direction: column;
  overflow: hidden;
}


.toolbar { display: none; }
.editor-tab-bar {
  display: flex;
  align-items: center;
  background: #12141a;
  border-bottom: 1px solid #282c37;
  user-select: none;
  height: 36px;
}
.tabs-container {
  display: flex;
  overflow-x: auto;
  height: 100%;
}
.tabs-container::-webkit-scrollbar { display: none; }
.editor-tab {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 0 14px;
  font-size: 12.5px;
  font-family: Consolas, Menlo, monospace;
  color: #abb2bf;
  background: #12141a;
  border-right: 1px solid #282c37;
  border-top: 2px solid transparent;
  height: 100%;
  cursor: pointer;
  white-space: nowrap;
}
.editor-tab.active {
  background: #181a20;
  border-top: 2px solid #528bff;
  color: #fff;
}
.editor-tab:not(.active):hover {
  background: #1c1f28;
}

.tab-icon {
  color: #528bff;
  font-weight: bold;
}
.tab-name {
  user-select: none;
}
.tab-close {
  margin-left: 6px;
  color: #6e7687;
  font-weight: bold;
  font-size: 11px;
  border-radius: 50%;
  width: 16px;
  height: 16px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  transition: 0.2s;
  padding-bottom: 1px;
}
.tab-close:hover {
  background: #f85149;
  color: #fff;
}

.tab-add {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  color: #abb2bf;
  cursor: pointer;
  font-size: 18px;
  font-weight: bold;
}
.tab-add:hover {
  color: #fff;
  background: #242933;
}
.editor-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-left: auto;
  padding-right: 10px;
}

.btn-editor-action {
  background: #242933;
  color: #abb2bf;
  border: 1px solid #363c4a;
  border-radius: 4px;
  padding: 4px 10px;
  font-size: 11.5px;
  cursor: pointer;
  transition: all 0.2s;
}
.btn-editor-action:hover {
  background: #2f3644;
  color: #fff;
  border-color: #528bff;
}

.ws-status {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 11.5px;
  color: #8b949e;
  margin-right: 6px;
}
.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}
.status-dot.online { background: #3fb950; box-shadow: 0 0 6px rgba(63, 185, 80, 0.6); }
.status-dot.connecting { background: #d29922; }
.status-dot.offline { background: #f85149; }

/* Editor Box */
.editor-box {
  display: flex;
  background: #181a20;
  height: 380px;
  position: relative;
  overflow: hidden;
}
.gutter {
  width: 52px;
  background: #14161c;
  color: #4b5263;
  font-family: Consolas, "SF Mono", Menlo, Monaco, "Courier New", monospace;
  font-size: 13.5px;
  line-height: 22px;
  padding: 12px 6px;
  text-align: right;
  user-select: none;
  border-right: 1px solid #242833;
  overflow: hidden;
  flex-shrink: 0;
  box-sizing: border-box;
}
.gutter-line {
  height: 22px;
  line-height: 22px;
  padding-right: 4px;
}
.gutter-line.active {
  color: #528bff;
  font-weight: bold;
}
.editor-wrap {
  position: relative;
  flex: 1;
  height: 100%;
  overflow: hidden;
}
.editor-highlight, .editor-textarea {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  margin: 0;
  padding: 12px 14px;
  font-family: Consolas, "SF Mono", Menlo, Monaco, "Courier New", monospace;
  font-size: 13.5px;
  line-height: 22px;
  letter-spacing: 0px;
  white-space: pre;
  word-wrap: normal;
  overflow-wrap: normal;
  tab-size: 4;
  -moz-tab-size: 4;
  border: none;
  outline: none;
  box-sizing: border-box;
}
.editor-highlight {
  background: transparent;
  color: #abb2bf;
  pointer-events: none;
  z-index: 1;
  overflow: hidden;
}
.editor-textarea {
  background: transparent;
  color: transparent;
  caret-color: #528bff;
  resize: none;
  z-index: 2;
  overflow: auto;
}
.editor-textarea::selection {
  background: rgba(82, 139, 255, 0.35);
}
.editor-textarea::-webkit-scrollbar, #terminal::-webkit-scrollbar {
  width: 10px;
  height: 10px;
}
.editor-textarea::-webkit-scrollbar-track, #terminal::-webkit-scrollbar-track {
  background: #14161c;
}
.editor-textarea::-webkit-scrollbar-thumb, #terminal::-webkit-scrollbar-thumb {
  background: #2b303c;
  border-radius: 5px;
}
.editor-textarea::-webkit-scrollbar-thumb:hover, #terminal::-webkit-scrollbar-thumb:hover {
  background: #3f4758;
}

/* Status Bar */
.editor-statusbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  background: #12141a;
  border-top: 1px solid #242833;
  padding: 4px 12px;
  font-size: 11.5px;
  color: #6e7687;
  user-select: none;
  height: 26px;
}
.statusbar-left, .statusbar-right {
  display: flex;
  gap: 12px;
  align-items: center;
}

/* Syntax Highlighting (Modern Dark Palette) */
.tok-kw { color: #c678dd; font-weight: 600; }
.tok-type { color: #4ec9b0; font-weight: 600; }
.tok-fn { color: #61afef; }
.tok-str { color: #98c379; }
.tok-num { color: #e5c07b; }
.tok-const { color: #d19a66; }
.tok-cmt { color: #7f848e; font-style: italic; }

/* Buttons */
.btn-group {
  display: flex;
  gap: 10px;
  margin-bottom: 14px;
}
.btn {
  border: none;
  border-radius: 6px;
  padding: 10px 16px;
  font-size: 15px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 6px;
}
.btn:hover {
  filter: brightness(1.15);
  transform: translateY(-1px);
}
.btn:active {
  transform: translateY(0);
}
.btn-send {
  background: linear-gradient(135deg, #2ea043, #238636);
  color: #ffffff;
  flex: 2;
  box-shadow: 0 3px 8px rgba(35, 134, 54, 0.4);
}
.btn-stop {
  background: linear-gradient(135deg, #da3633, #b62324);
  color: #ffffff;
  flex: 1.4;
  box-shadow: 0 3px 8px rgba(218, 54, 51, 0.4);
}
.btn-clear {
  background: linear-gradient(135deg, #373e47, #2d333b);
  color: #e2e4e9;
  flex: 1;
  border: 1px solid #444c56;
}

/* Terminal Card */
.term-card {
  background: #14161d;
  border: 1px solid #2b303c;
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 14px;
  box-shadow: 0 4px 14px rgba(0, 0, 0, 0.4);
}
.term-header {
  font-size: 12.5px;
  color: #8b949e;
  padding: 8px 14px;
  background: #1c1f28;
  border-bottom: 1px solid #282c37;
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-weight: 600;
}
#terminal {
  background: #0d1117;
  color: #c9d1d9;
  padding: 12px 14px;
  height: 210px;
  overflow-y: auto;
  font-family: Consolas, "SF Mono", Menlo, monospace;
  font-size: 13px;
  line-height: 1.5;
  white-space: pre-wrap;
  word-break: break-all;
  text-align: left;
}
#terminal div {
  margin-bottom: 2px;
  word-wrap: break-word;
}
.term-log-run { color: #58a6ff; font-weight: 500; }
.term-log-err { color: #f85149; font-weight: 600; }
.term-log-succ { color: #56d364; }

/* Navigation & Footer */
.nav-buttons {
  display: flex;
  gap: 10px;
  margin-top: 10px;
}
.nav-buttons a {
  flex: 1;
  text-decoration: none;
}
.nav-buttons .btn-nav {
  width: 100%;
  background: #21262d;
  color: #c9d1d9;
  border: 1px solid #30363d;
  padding: 9px 14px;
  font-size: 14px;
}
.nav-buttons .btn-nav:hover {
  background: #30363d;
  color: #fff;
}

.footer-text {
  font-size: 12px;
  color: #6e7687;
  margin-top: 22px;
  border-top: 1px solid #282c37;
  padding-top: 12px;
  text-align: center;
  line-height: 1.6;
}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>Tien Script Console</h2>

<div class='toolbar'>
    <label for='scriptName'>Task Name:</label>
    <input type='text' id='scriptName' placeholder='e.g. script_1' value='script_1'>
</div>

<div class='editor-container'>
    
    <div class='editor-tab-bar'>
        <div class='tabs-container' id='tabsContainer'>
        </div>
        <div class='tab-add' onclick='addNewTab()' title='New File'>+</div>
        <div class='editor-actions'>
            <span class='ws-status' id='wsStatus'>
                <span class='status-dot' id='wsStatusDot'></span>
                <span id='wsStatusText'>Connecting...</span>
            </span>
            <button class='btn-editor-action' id='btnCopyCode' onclick='copyEditorCode()' title='Copy Code to Clipboard'>Copy</button>
            <button class='btn-editor-action' onclick='clearEditorCode()' title='Clear Code'>Clear</button>
        </div>
    </div>

    <div class='editor-box'>
        <div class='gutter' id='gutter'><div class='gutter-line active'>1</div></div>
        <div class='editor-wrap'>
            <pre class='editor-highlight' id='highlightPre' aria-hidden='true'><code id='highlightCode'></code></pre>
            <textarea class='editor-textarea' id='cmdInput' spellcheck='false' autocomplete='off' autocapitalize='off' placeholder='Enter Tien script expression or code... (Ctrl+Enter to execute)'></textarea>
        </div>
    </div>
    <div class='editor-statusbar'>
        <div class='statusbar-left'>
            <span id='editorStatus'>Ln 1, Col 1</span>
            <span>Spaces: 4</span>
            <span>TI Script</span>
        </div>
        <div class='statusbar-right'>
            <span>UTF-8</span>
            <span>Ctrl+Enter to Execute</span>
        </div>
    </div>
</div>

<div class='btn-group'>
    <button class='btn btn-send' onclick='sendConsoleCmd()' title='Ctrl+Enter to run script'>&#9654; Execute</button>
    <button class='btn btn-stop' onclick='stopConsoleTask()' title='Stop running script task'>&#9632; Stop Task</button>
    <button class='btn btn-clear' onclick='clearTerminal()' title='Clear console diagnostics'>&#10005; Clear Output</button>
</div>

<div class='term-card'>
    <div class='term-header'>
        <span>Console Output &amp; Diagnostics</span>
        <span id='termCount' style='font-size:11.5px;color:#8b949e;'>0 lines</span>
    </div>
    <div id='terminal'></div>
</div>

<div class='nav-buttons'>
    <a href='/tools'><button class='btn btn-nav'>Tools Menu</button></a>
    <a href='/'><button class='btn btn-nav'>Main Menu</button></a>
</div>

<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>

<script>
const MAX_LINES = 500;
let gateway = 'ws://' + window.location.host + '/ws_tien';
let wsConn;

const cmdInput = document.getElementById('cmdInput');
const highlightPre = document.getElementById('highlightPre');
const highlightCode = document.getElementById('highlightCode');
const gutter = document.getElementById('gutter');



let scripts = { 'script_1.ti': '' };
let activeTab = 'script_1.ti';
let tabCounter = 1;


function renderTabs() {
  const container = document.getElementById('tabsContainer');
  if(!container) return;
  container.innerHTML = '';
  for (let name in scripts) {
      const div = document.createElement('div');
      div.className = 'editor-tab' + (name === activeTab ? ' active' : '');
      
      div.onclick = () => switchTab(name);
      div.ondblclick = (e) => {
          e.stopPropagation();
          renameTab(name);
      };
      
      const icon = document.createElement('span');
      icon.className = 'tab-icon';
      icon.innerHTML = '&lt;/&gt;';
      
      const nameSpan = document.createElement('span');
      nameSpan.className = 'tab-name';
      nameSpan.textContent = name;
      nameSpan.title = 'Double click to rename';
      
      const closeBtn = document.createElement('span');
      closeBtn.className = 'tab-close';
      closeBtn.innerHTML = '&#10005;';
      closeBtn.title = 'Close tab';
      closeBtn.onclick = (e) => {
          e.stopPropagation();
          closeTab(name);
      };
      
      div.appendChild(icon);
      div.appendChild(nameSpan);
      div.appendChild(closeBtn);
      container.appendChild(div);
  }
}

function closeTab(name) {
  if (Object.keys(scripts).length <= 1) {
    alert('Cannot close the last tab. Rename it instead or create a new one first.');
    return;
  }
  
  delete scripts[name];
  if (activeTab === name) {
     activeTab = Object.keys(scripts)[0];
     cmdInput.value = scripts[activeTab];
  }
  renderTabs();
  renderEditor();
  updateCursorPos();
}

function renameTab(oldName) {
  let newName = prompt('Rename file (must end with .ti):', oldName);
  if (!newName || newName === oldName) return;
  
  newName = newName.trim();
  if (!newName.endsWith('.ti')) newName += '.ti';
  
  if (scripts[newName] !== undefined) {
     alert('A file with this name already exists!');
     return;
  }
  
  scripts[newName] = scripts[oldName];
  delete scripts[oldName];
  
  if (activeTab === oldName) {
     activeTab = newName;
  }
  renderTabs();
}


function switchTab(name) {
  if (activeTab === name) return;
  scripts[activeTab] = cmdInput.value;
  activeTab = name;
  cmdInput.value = scripts[name];
  
  renderTabs();
  renderEditor();
  updateCursorPos();
}

function addNewTab() {
  scripts[activeTab] = cmdInput.value;
  tabCounter++;
  let newName = 'script_' + tabCounter + '.ti';
  while (scripts[newName] !== undefined) {
      tabCounter++;
      newName = 'script_' + tabCounter + '.ti';
  }
  scripts[newName] = '';
  switchTab(newName);
}


function updateWsBadge(status) {
  const dot = document.getElementById('wsStatusDot');
  const text = document.getElementById('wsStatusText');
  if (!dot || !text) return;
  if (status === 'connected') {
    dot.className = 'status-dot online';
    text.textContent = 'Connected';
  } else if (status === 'connecting') {
    dot.className = 'status-dot connecting';
    text.textContent = 'Connecting...';
  } else {
    dot.className = 'status-dot offline';
    text.textContent = 'Disconnected';
  }
}

function initWS(){
  updateWsBadge('connecting');
  try {
    wsConn = new WebSocket(gateway);
  } catch (err) {
    console.error('WS init error', err);
    updateWsBadge('disconnected');
    setTimeout(initWS, 2000);
    return;
  }

  wsConn.onopen = () => {
    console.log('Tien WS connected');
    updateWsBadge('connected');
  };

  wsConn.onclose = () => {
    console.log('Tien WS disconnected');
    updateWsBadge('disconnected');
    setTimeout(initWS, 2000);
  };

  wsConn.onerror = (err) => {
    console.error('Tien WS error', err);
    updateWsBadge('disconnected');
  };

  wsConn.onmessage = (e) => {
    const term = document.getElementById('terminal');
    const line = document.createElement('div');
    line.textContent = e.data;
    if (e.data.startsWith('> Stop') || e.data.startsWith('> [Run')) {
      line.className = 'term-log-run';
    } else if (e.data.includes('[ERROR]') || e.data.includes('[Fatal Error]') || e.data.includes('error') || e.data.includes('Error')) {
      line.className = 'term-log-err';
    } else if (e.data.includes('[INFO]') || e.data.includes('successfully') || e.data.includes('Connected')) {
      line.className = 'term-log-succ';
    }
    term.appendChild(line);
    while (term.children.length > MAX_LINES) {
      term.removeChild(term.firstChild);
    }
    term.scrollTop = term.scrollHeight;

    const countEl = document.getElementById('termCount');
    if (countEl) {
      countEl.textContent = term.children.length + ' lines';
    }
  };
}

function escapeHtml(str) {
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function highlightTI(code) {
  // Tokenizer regex for TI language (C/C++ like):
  // 1: Comments (// ... or /* ... */)
  // 2: Strings ("..." or '...')
  // 3: Numbers (Hex 0x... or decimal/float)
  // 4: Types (int, float, string, bool, void, char, struct)
  // 5: Keywords (while, for, if, else, return, break, continue, switch, case, default, do)
  // 6: Constants (true, false, null)
  // 7: Built-in functions or Function calls
  const tokenRegex = /(\/\/[^\n]*|\/\*[\s\S]*?\*\/)|("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(\b0x[0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b)|(\b(?:int|float|string|bool|void|char|struct)\b)|(\b(?:while|for|if|else|return|break|continue|switch|case|default|do)\b)|(\b(?:true|false|null)\b)|(\b(?:print|delay|autonics_read|autonics_tk_get_pv|autonics_tk_get_sv|autonics_tk_set_slave_address|get_json|get_json_as_string|get_json_as_int|get_json_as_float|get_json_as_bool|http_get|http_post|is_none|file_read|file_write|file_exists|file_remove|relay_get_state|relay_set_state)\b|\b[a-zA-Z_]\w*(?=\s*\())/g;

  let html = '';
  let lastIndex = 0;
  let match;
  while ((match = tokenRegex.exec(code)) !== null) {
    html += escapeHtml(code.substring(lastIndex, match.index));
    if (match[1]) {
      html += '<span class="tok-cmt">' + escapeHtml(match[1]) + '</span>';
    } else if (match[2]) {
      html += '<span class="tok-str">' + escapeHtml(match[2]) + '</span>';
    } else if (match[3]) {
      html += '<span class="tok-num">' + escapeHtml(match[3]) + '</span>';
    } else if (match[4]) {
      html += '<span class="tok-type">' + escapeHtml(match[4]) + '</span>';
    } else if (match[5]) {
      html += '<span class="tok-kw">' + escapeHtml(match[5]) + '</span>';
    } else if (match[6]) {
      html += '<span class="tok-const">' + escapeHtml(match[6]) + '</span>';
    } else if (match[7]) {
      html += '<span class="tok-fn">' + escapeHtml(match[7]) + '</span>';
    }
    lastIndex = tokenRegex.lastIndex;
  }
  html += escapeHtml(code.substring(lastIndex));
  if (code.endsWith('\n') || code.length === 0) {
    html += ' ';
  }
  return html;
}

function updateGutter() {
  const code = cmdInput.value;
  const lineCount = Math.max(1, code.split('\n').length);
  const curPos = cmdInput.selectionStart;
  const curLine = code.substring(0, curPos).split('\n').length;

  let nums = '';
  for (let i = 1; i <= lineCount; i++) {
    nums += `<div class="gutter-line${i === curLine ? ' active' : ''}">${i}</div>`;
  }
  gutter.innerHTML = nums;
}

function updateCursorPos() {
  const code = cmdInput.value;
  const pos = cmdInput.selectionStart;
  const linesBefore = code.substring(0, pos).split('\n');
  const curLine = linesBefore.length;
  const curCol = linesBefore[linesBefore.length - 1].length + 1;

  const statusEl = document.getElementById('editorStatus');
  if (statusEl) {
    statusEl.textContent = `Ln ${curLine}, Col ${curCol}`;
  }

  const gutterLines = gutter.children;
  for (let i = 0; i < gutterLines.length; i++) {
    if (i + 1 === curLine) {
      gutterLines[i].classList.add('active');
    } else {
      gutterLines[i].classList.remove('active');
    }
  }
}

function syncScroll() {
  highlightPre.scrollTop = cmdInput.scrollTop;
  highlightPre.scrollLeft = cmdInput.scrollLeft;
  gutter.scrollTop = cmdInput.scrollTop;
}

function renderEditor() {
  const code = cmdInput.value;
  highlightCode.innerHTML = highlightTI(code);
  updateGutter();
  syncScroll();
}

/**
 * Scans code forward up to pos to find the matching opening brace '{'
 * and returns its line indentation string. Handles strings & comments safely.
 */
function getMatchingIndentForBrace(text, pos) {
  const stack = [];
  let i = 0;
  const limit = Math.min(pos, text.length);

  while (i < limit) {
    const ch = text[i];
    const next = i + 1 < limit ? text[i + 1] : '';

    // Line comment: //
    if (ch === '/' && next === '/') {
      i += 2;
      while (i < limit && text[i] !== '\n') i++;
      continue;
    }
    // Block comment: /* ... */
    if (ch === '/' && next === '*') {
      i += 2;
      while (i < limit && !(text[i] === '*' && i + 1 < limit && text[i + 1] === '/')) {
        i++;
      }
      i += 2;
      continue;
    }
    // String literal: "..."
    if (ch === '"') {
      i++;
      while (i < limit && text[i] !== '"') {
        if (text[i] === '\\') i++;
        i++;
      }
      i++;
      continue;
    }
    // Char literal: '...'
    if (ch === "'") {
      i++;
      while (i < limit && text[i] !== "'") {
        if (text[i] === '\\') i++;
        i++;
      }
      i++;
      continue;
    }

    // Braces tracking
    if (ch === '{') {
      const lineStart = text.lastIndexOf('\n', i - 1) + 1;
      const lineText = text.substring(lineStart, i);
      const indentMatch = lineText.match(/^[ \t]*/);
      const indent = indentMatch ? indentMatch[0] : '';
      stack.push(indent);
    } else if (ch === '}') {
      if (stack.length > 0) {
        stack.pop();
      }
    }
    i++;
  }

  if (stack.length > 0) {
    return stack[stack.length - 1];
  }
  return null;
}

// Auto-closing pairs definition
const PAIRS = { '(': ')', '[': ']', '{': '}', '"': '"', "'": "'" };
const CLOSE_CHARS = [')', ']', '}', '"', "'"];

cmdInput.addEventListener('input', function() {
  renderEditor();
  updateCursorPos();
});

cmdInput.addEventListener('scroll', syncScroll);
cmdInput.addEventListener('keyup', updateCursorPos);
cmdInput.addEventListener('click', updateCursorPos);
cmdInput.addEventListener('select', updateCursorPos);

// Key bindings
cmdInput.addEventListener('keydown', function(e) {
  const start = this.selectionStart;
  const end = this.selectionEnd;
  const text = this.value;

  // 1. Tab and Shift+Tab (Indent / Unindent)
  if (e.key === 'Tab') {
    e.preventDefault();
    if (e.shiftKey) {
      // Unindent 4 spaces or 1 tab
      const firstLineStart = text.lastIndexOf('\n', start - 1) + 1;
      let lastLineEnd = text.indexOf('\n', end);
      if (lastLineEnd === -1) lastLineEnd = text.length;

      const lines = text.substring(firstLineStart, lastLineEnd).split('\n');
      let removedFirst = 0;
      let totalRemoved = 0;

      const unindented = lines.map((line, idx) => {
        let removeCount = 0;
        if (line.startsWith('    ')) {
          removeCount = 4;
        } else if (line.startsWith('\t')) {
          removeCount = 1;
        } else {
          const m = line.match(/^ +/);
          if (m) removeCount = Math.min(m[0].length, 4);
        }
        if (idx === 0) removedFirst = removeCount;
        totalRemoved += removeCount;
        return line.substring(removeCount);
      });

      this.setRangeText(unindented.join('\n'), firstLineStart, lastLineEnd, 'preserve');
      this.selectionStart = Math.max(firstLineStart, start - removedFirst);
      this.selectionEnd = Math.max(this.selectionStart, end - totalRemoved);
      renderEditor();
      updateCursorPos();
      return;
    } else {
      // Indent: check if multiple lines selected
      if (start !== end && text.substring(start, end).includes('\n')) {
        const firstLineStart = text.lastIndexOf('\n', start - 1) + 1;
        let lastLineEnd = text.indexOf('\n', end);
        if (lastLineEnd === -1) lastLineEnd = text.length;

        const lines = text.substring(firstLineStart, lastLineEnd).split('\n');
        const indented = lines.map(line => '    ' + line);
        const totalAdded = lines.length * 4;

        this.setRangeText(indented.join('\n'), firstLineStart, lastLineEnd, 'preserve');
        this.selectionStart = start + 4;
        this.selectionEnd = end + totalAdded;
        renderEditor();
        updateCursorPos();
        return;
      } else {
        // Single line tab: insert 4 spaces
        this.setRangeText('    ', start, end, 'end');
        this.selectionStart = this.selectionEnd = start + 4;
        renderEditor();
        updateCursorPos();
        return;
      }
    }
  }

  
  // 8. Toggle Line Comment (Ctrl + /)
  if (e.key === '/' && (e.ctrlKey || e.metaKey)) {
    e.preventDefault();
    const firstLineStart = text.lastIndexOf('\n', start - 1) + 1;
    let lastLineEnd = text.indexOf('\n', end);
    if (lastLineEnd === -1) lastLineEnd = text.length;

    const lines = text.substring(firstLineStart, lastLineEnd).split('\n');
    const allCommented = lines.every(line => /^\s*\/\//.test(line) || line.trim() === '');
    
    if (allCommented) {
      const uncommented = lines.map(line => {
        const match = line.match(/^(\s*)\/\/( ?)/);
        if (match) {
          return line.substring(0, match[1].length) + line.substring(match[0].length);
        }
        return line;
      });
      this.setRangeText(uncommented.join('\n'), firstLineStart, lastLineEnd, 'preserve');
    } else {
      const commented = lines.map(line => '// ' + line);
      this.setRangeText(commented.join('\n'), firstLineStart, lastLineEnd, 'preserve');
    }
    
    renderEditor();
    updateCursorPos();
    return;
  }

  // 2. Execute on Ctrl+Enter or Cmd+Enter
  if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
    e.preventDefault();
    sendConsoleCmd();
    return;
  }

  // 3. Enter key: handle Enter between {} and auto-indent
  if (e.key === 'Enter') {
    e.preventDefault();

    // Special Case: Cursor between { and }
    if (start === end && start > 0 && start < text.length && text[start - 1] === '{' && text[start] === '}') {
      const lineStart = text.lastIndexOf('\n', start - 2) + 1;
      const currentLine = text.substring(lineStart, start - 1);
      const indentMatch = currentLine.match(/^[ \t]*/);
      const baseIndent = indentMatch ? indentMatch[0] : '';
      const innerIndent = baseIndent + '    ';

      const insertText = '\n' + innerIndent + '\n' + baseIndent;
      this.setRangeText(insertText, start, start, 'end');
      const newCursor = start + 1 + innerIndent.length;
      this.selectionStart = this.selectionEnd = newCursor;
      renderEditor();
      updateCursorPos();
      return;
    }

    // Standard Enter with auto-indent
    const lineStart = text.lastIndexOf('\n', start - 1) + 1;
    const currentLine = text.substring(lineStart, start);
    const indentMatch = currentLine.match(/^[ \t]*/);
    let indent = indentMatch ? indentMatch[0] : '';
    if (currentLine.trim().endsWith('{')) {
      indent += '    ';
    }

    this.setRangeText('\n' + indent, start, end, 'end');
    const newCursor = start + 1 + indent.length;
    this.selectionStart = this.selectionEnd = newCursor;
    renderEditor();
    updateCursorPos();
    return;
  }

  // 4. Backspace: delete both chars if cursor is between an empty pair
  if (e.key === 'Backspace') {
    if (start === end && start > 0 && start < text.length) {
      const left = text[start - 1];
      const right = text[start];
      if (PAIRS[left] && PAIRS[left] === right) {
        e.preventDefault();
        this.setRangeText('', start - 1, start + 1, 'end');
        this.selectionStart = this.selectionEnd = start - 1;
        renderEditor();
        updateCursorPos();
        return;
      }
    }
  }

  // 5. Dedent on '}' key
  if (e.key === '}') {
    // Skip over if next char is already '}'
    if (start === end && start < text.length && text[start] === '}') {
      e.preventDefault();
      this.selectionStart = this.selectionEnd = start + 1;
      updateCursorPos();
      return;
    }

    // Check if line before cursor consists solely of whitespace
    const lineStart = text.lastIndexOf('\n', start - 1) + 1;
    const linePrefix = text.substring(lineStart, start);

    if (/^[ \t]*$/.test(linePrefix)) {
      e.preventDefault();
      const matchingIndent = getMatchingIndentForBrace(text, start);
      let targetIndent = '';

      if (matchingIndent !== null) {
        targetIndent = matchingIndent;
      } else {
        // Fallback: unindent 1 level (4 spaces or 1 tab)
        if (linePrefix.endsWith('    ')) {
          targetIndent = linePrefix.slice(0, -4);
        } else if (linePrefix.endsWith('\t')) {
          targetIndent = linePrefix.slice(0, -1);
        } else {
          targetIndent = linePrefix.slice(0, Math.max(0, linePrefix.length - 4));
        }
      }

      const replacement = targetIndent + '}';
      this.setRangeText(replacement, lineStart, end, 'end');
      const newCursor = lineStart + replacement.length;
      this.selectionStart = this.selectionEnd = newCursor;
      renderEditor();
      updateCursorPos();
      return;
    }
  }

  // 6. Overtype / Skip closing chars: ')', ']', '"', '\''
  if (CLOSE_CHARS.includes(e.key) && e.key !== '}') {
    if (start === end && start < text.length && text[start] === e.key) {
      e.preventDefault();
      this.selectionStart = this.selectionEnd = start + 1;
      updateCursorPos();
      return;
    }
  }

  // 7. Auto-closing pairs insertion & Surround selection
  if (PAIRS[e.key]) {
    // Selection surround
    if (start !== end) {
      e.preventDefault();
      const selected = text.substring(start, end);
      const replacement = e.key + selected + PAIRS[e.key];
      this.setRangeText(replacement, start, end, 'select');
      this.selectionStart = start + 1;
      this.selectionEnd = end + 1;
      renderEditor();
      updateCursorPos();
      return;
    }

    // Skip over quotes if standing directly in front of matching quote
    if ((e.key === '"' || e.key === "'") && start < text.length && text[start] === e.key) {
      e.preventDefault();
      this.selectionStart = this.selectionEnd = start + 1;
      updateCursorPos();
      return;
    }

    // Insert pair and place cursor in between
    e.preventDefault();
    const pair = e.key + PAIRS[e.key];
    this.setRangeText(pair, start, start, 'end');
    this.selectionStart = this.selectionEnd = start + 1;
    renderEditor();
    updateCursorPos();
    return;
  }
});

function copyEditorCode() {
  const code = cmdInput.value;
  if (!code) return;
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(code).then(() => {
      const btn = document.getElementById('btnCopyCode');
      if (btn) {
        const orig = btn.textContent;
        btn.textContent = 'Copied!';
        setTimeout(() => { btn.textContent = orig; }, 1500);
      }
    }).catch(() => {
      fallbackCopy();
    });
  } else {
    fallbackCopy();
  }
}

function fallbackCopy() {
  cmdInput.select();
  document.execCommand('copy');
  const btn = document.getElementById('btnCopyCode');
  if (btn) {
    const orig = btn.textContent;
    btn.textContent = 'Copied!';
    setTimeout(() => { btn.textContent = orig; }, 1500);
  }
}


function clearEditorCode() {
  if (cmdInput.value.length > 0 && confirm('Clear code from editor?')) {
    cmdInput.value = '';
    scripts[activeTab] = '';
    renderEditor();
    updateCursorPos();
    cmdInput.focus();
  }
}

function sendPayload(payload){
  if (wsConn && wsConn.readyState === WebSocket.OPEN) {
    wsConn.send(JSON.stringify(payload));
  } else {
    console.error("Tien WS not open!");
  }  
}

function sendConsoleCmd(){
  scripts[activeTab] = cmdInput.value;
  let name = activeTab.replace('.ti', '');
  let val = scripts[activeTab];
  if(name.length === 0){
    alert('Invalid tab name');
    return;
  }
  if(val.trim().length > 0){
    sendPayload({action:'run', name:name, code:val});
  }
}

function stopConsoleTask(){
  let name = activeTab.replace('.ti', '');
  sendPayload({action:'stop', name:name});
}

function clearTerminal(){
  document.getElementById('terminal').innerHTML = '';
  const countEl = document.getElementById('termCount');
  if (countEl) countEl.textContent = '0 lines';
}

window.addEventListener('beforeunload', function() {
  if (wsConn) {
    wsConn.onclose = function() {};
    wsConn.close();
  }
});

renderTabs();
renderEditor();
updateCursorPos();
initWS();
</script>
</body>
</html>
)rawliteral";

#endif // TIEN_CONSOLE_HTML_H
