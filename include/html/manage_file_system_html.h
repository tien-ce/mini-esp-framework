#ifndef MANAGE_FILE_SYSTEM_HTML_H
#define MANAGE_FILE_SYSTEM_HTML_H

#include <Arduino.h>

const char MANAGE_FILE_SYSTEM_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Manage File System</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  background: #14161d;
  color: #e2e4e9;
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}
.header {
  text-align: center;
  padding: 16px 10px;
  background: #1c1f28;
  border-bottom: 1px solid #2b303c;
}
.header h1 { font-size: 20px; color: #f0f2f5; }
.header h2 { font-size: 16px; color: #00d2ff; margin-top: 6px; }

.app-container {
  display: flex;
  flex: 1;
  height: calc(100vh - 70px);
}

/* Sidebar File Explorer */
.sidebar {
  width: 250px;
  background: #181a20;
  border-right: 1px solid #282c37;
  display: flex;
  flex-direction: column;
}
.sidebar-header {
  padding: 12px 14px;
  font-size: 13px;
  font-weight: 600;
  color: #8b949e;
  border-bottom: 1px solid #282c37;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.btn-new-file {
  background: transparent;
  color: #528bff;
  border: none;
  cursor: pointer;
  font-size: 18px;
  font-weight: bold;
}
.btn-new-file:hover { color: #fff; }

.file-list {
  flex: 1;
  overflow-y: auto;
  list-style: none;
  padding: 10px 0;
}
.file-item {
  padding: 8px 14px;
  font-size: 13.5px;
  color: #abb2bf;
  cursor: pointer;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.file-item-left {
  display: flex;
  align-items: center;
  gap: 8px;
  flex: 1;
  overflow: hidden;
}
.file-name {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  flex: 1;
}
.file-actions {
  display: none;
  gap: 8px;
  font-size: 14px;
}
.file-item:hover .file-actions { display: flex; }
.file-action-btn { color: #8b949e; cursor: pointer; transition: 0.2s; }
.file-action-btn:hover { color: #f85149; }

.file-item:hover { background: #21252f; color: #fff; }
.file-item.active { background: #2c313c; color: #528bff; font-weight: 600; border-left: 3px solid #528bff; padding-left: 11px; }

.inline-input {
  width: 100%;
  background: #12141a;
  border: 1px solid #528bff;
  color: #fff;
  padding: 4px 8px;
  font-family: inherit;
  font-size: 13px;
  outline: none;
  border-radius: 4px;
}

/* Main Editor Area */
.main-editor {
  flex: 1;
  display: flex;
  flex-direction: column;
  background: #14161d;
  padding: 14px;
  overflow: hidden;
}
.editor-container {
  flex: 1;
  background: #181a20;
  border: 1px solid #2b303c;
  border-radius: 8px;
  margin-bottom: 14px;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  box-shadow: 0 4px 14px rgba(0,0,0,0.3);
}

/* Tab Bar */
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
.editor-tab:not(.active):hover { background: #1c1f28; }
.tab-icon { color: #528bff; font-weight: bold; }
.tab-name { user-select: none; }
.tab-close {
  margin-left: 6px;
  color: #6e7687;
  font-size: 12px;
  font-weight: bold;
  border-radius: 50%;
  width: 16px;
  height: 16px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  padding-bottom: 1px;
}
.tab-close:hover { background: #f85149; color: #fff; }

.editor-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-left: auto;
  padding-right: 10px;
}
.ws-status {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 11.5px;
  color: #8b949e;
  margin-right: 6px;
}
.status-dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
.status-dot.online { background: #3fb950; box-shadow: 0 0 6px rgba(63, 185, 80, 0.6); }
.status-dot.connecting { background: #d29922; }
.status-dot.offline { background: #f85149; }

/* Editor Box */
.editor-box {
  display: flex;
  flex: 1;
  background: #181a20;
  position: relative;
  overflow: hidden;
}
.gutter {
  width: 52px;
  background: #14161c;
  color: #4b5263;
  font-family: Consolas, "SF Mono", Menlo, monospace;
  font-size: 13.5px;
  line-height: 22px;
  padding: 12px 6px;
  text-align: right;
  user-select: none;
  border-right: 1px solid #242833;
  overflow: hidden;
  flex-shrink: 0;
}
.gutter-line { height: 22px; line-height: 22px; padding-right: 4px; }
.gutter-line.active { color: #528bff; font-weight: bold; }
.editor-wrap {
  position: relative;
  flex: 1;
  height: 100%;
  overflow: hidden;
}
.editor-highlight, .editor-textarea {
  position: absolute;
  top: 0; left: 0; width: 100%; height: 100%;
  margin: 0; padding: 12px 14px;
  font-family: Consolas, "SF Mono", Menlo, monospace;
  font-size: 13.5px;
  line-height: 22px;
  white-space: pre;
  tab-size: 4;
  border: none; outline: none;
}
.editor-highlight { background: transparent; color: #abb2bf; pointer-events: none; z-index: 1; overflow: hidden; }
.editor-textarea { background: transparent; color: transparent; caret-color: #528bff; resize: none; z-index: 2; overflow: auto; }
.editor-textarea::selection { background: rgba(82, 139, 255, 0.35); }
.editor-textarea::-webkit-scrollbar, #terminal::-webkit-scrollbar { width: 10px; height: 10px; }
.editor-textarea::-webkit-scrollbar-track, #terminal::-webkit-scrollbar-track { background: #14161c; }
.editor-textarea::-webkit-scrollbar-thumb, #terminal::-webkit-scrollbar-thumb { background: #2b303c; border-radius: 5px; }
.editor-textarea::-webkit-scrollbar-thumb:hover, #terminal::-webkit-scrollbar-thumb:hover { background: #3f4758; }

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

/* Terminal Card */
.term-card {
  display: flex;
  flex-direction: column;
  background: #14161d;
  border: 1px solid #2b303c;
  border-radius: 8px;
  overflow: hidden;
  margin-bottom: 14px;
  box-shadow: 0 4px 14px rgba(0, 0, 0, 0.4);
  flex-shrink: 0;
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
#terminal div { margin-bottom: 2px; word-wrap: break-word; }
.term-log-run { color: #58a6ff; font-weight: 500; }
.term-log-err { color: #f85149; font-weight: 600; }
.term-log-succ { color: #56d364; }

/* Syntax Highlighting Palette */
.tok-kw { color: #c678dd; font-weight: 600; }
.tok-type { color: #4ec9b0; font-weight: 600; }
.tok-fn { color: #61afef; }
.tok-str { color: #98c379; }
.tok-num { color: #e5c07b; }
.tok-const { color: #d19a66; }
.tok-cmt { color: #7f848e; font-style: italic; }

/* Buttons */
.btn-group { display: flex; gap: 10px; flex-wrap: wrap; margin-bottom: 14px; }
.btn {
  border: none; border-radius: 6px;
  padding: 10px 16px; font-size: 14px; font-weight: 600;
  cursor: pointer; transition: all 0.2s; flex: 1;
  display: inline-flex; align-items: center; justify-content: center; gap: 6px;
}
.btn:hover { filter: brightness(1.15); }
.btn-save { background: linear-gradient(135deg, #2ea043, #238636); color: #fff; }
.btn-execute { background: linear-gradient(135deg, #58a6ff, #1f6feb); color: #fff; display: none; }
.btn-stop { background: linear-gradient(135deg, #da3633, #b62324); color: #fff; display: none; }
.btn-close-tab { background: linear-gradient(135deg, #373e47, #2d333b); color: #fff; border: 1px solid #444c56; }
</style>
</head>
<body>
<div class="header">
  <h1>ESP32S3 ESP mini framework</h1>
  <h2>Manage File System</h2>
</div>
<div class="app-container">
  <div class="sidebar">
    <div class="sidebar-header">
      <span>FILES</span>
      <button class="btn-new-file" onclick="addNewFileInline()" title="New File">+</button>
    </div>
    <ul class="file-list" id="fileList"></ul>
  </div>

  <div class="main-editor">
    <div class="editor-container">
      <div class="editor-tab-bar">
        <div class="tabs-container" id="tabsContainer"></div>
        <div class="editor-actions">
            <span class="ws-status" id="wsStatus">
                <span class="status-dot" id="wsStatusDot"></span>
                <span id="wsStatusText">Connecting...</span>
            </span>
        </div>
      </div>
      <div class="editor-box">
        <div class="gutter" id="gutter"></div>
        <div class="editor-wrap">
          <pre class="editor-highlight" id="highlightPre"><code id="highlightCode"></code></pre>
          <textarea class="editor-textarea" id="cmdInput" spellcheck="false" autocomplete="off"></textarea>
        </div>
      </div>
      <div class="editor-statusbar">
        <div id="editorStatus">Ln 1, Col 1</div>
        <div id="fileTypeStatus">Plain Text</div>
      </div>
    </div>
    
    <div class="btn-group">
      <button class="btn btn-save" onclick="saveActiveFile()">&#128190; Save</button>
      <button class="btn btn-execute" id="btnExecute" onclick="executeFile()">&#9654; Execute</button>
      <button class="btn btn-stop" id="btnStop" onclick="stopFileTask()">&#9632; Stop Task</button>
      <button class="btn btn-close-tab" onclick="clearTerminal()">&#128465; Clear Output</button>
    </div>

    <div class="term-card" id="termCard">
        <div class="term-header">
            <span>Console Output &amp; Diagnostics</span>
            <div>
              <span id="termCount" style="font-size:11.5px;color:#8b949e;margin-right:10px;">0 lines</span>
              <button style="background:transparent;border:none;color:#8b949e;cursor:pointer;" onclick="clearTerminal()">Clear</button>
            </div>
        </div>
        <div id="terminal"></div>
    </div>
  </div>
</div>

<script>
// Server Configurations
const SERVER_URL = '';
// Basic Auth credentials provided by user

const fetchHeaders = {};
const WS_URL = 'ws://' + window.location.host + '/ws_tien';

let wsConn;
let fileListArr = [];
let openTabs = [];
let activeTab = null;
let fileCache = {}; 

const cmdInput = document.getElementById('cmdInput');
const highlightPre = document.getElementById('highlightPre');
const highlightCode = document.getElementById('highlightCode');
const gutter = document.getElementById('gutter');
const fileList = document.getElementById('fileList');
const tabsContainer = document.getElementById('tabsContainer');
const fileTypeStatus = document.getElementById('fileTypeStatus');
const termCard = document.getElementById('termCard');
const btnExecute = document.getElementById('btnExecute');
const btnStop = document.getElementById('btnStop');
const terminal = document.getElementById('terminal');

// --- File System Operations ---
function clientLog(msg, type = 'info') {
  const line = document.createElement('div');
  line.textContent = '> [Client] ' + msg;
  if (type === 'error') line.className = 'term-log-err';
  else if (type === 'success') line.className = 'term-log-succ';
  else line.style.color = '#8b949e';
  terminal.appendChild(line);
  while (terminal.children.length > 500) terminal.removeChild(terminal.firstChild);
  terminal.scrollTop = terminal.scrollHeight;
  const countEl = document.getElementById('termCount');
  if (countEl) countEl.textContent = terminal.children.length + ' lines';
}

async function fetchFileList() {
  try {
    const response = await fetch(`${SERVER_URL}/api/fs/list`, { headers: fetchHeaders });
    if (response.ok) {
      const data = await response.json();
      fileListArr = data.files ? data.files.map(f => f.name.replace(/^\//, '')) : [];
    }
  } catch (err) {
    console.warn("Failed to fetch file list", err);
  }
  renderSidebar();
}

async function fetchFileContent(path) {
  try {
    let apiPath = path.startsWith('/') ? path : '/' + path;
    const response = await fetch(`${SERVER_URL}/api/fs/read?path=${encodeURIComponent(apiPath)}`, { headers: fetchHeaders });
    if (response.ok) {
      return await response.text();
    } else {
      console.warn("File read returned status:", response.status);
    }
  } catch (err) {
    console.warn("Failed to read file.", err);
  }
  return '';
}

async function saveFileContent(path, content) {
  try {
    let apiPath = path.startsWith('/') ? path : '/' + path;
    const response = await fetch(`${SERVER_URL}/api/fs/save`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path: apiPath, content: content })
    });
    if (!response.ok) {
        clientLog('Failed to save file. Status: ' + response.status, 'error');
    } else {
        clientLog('Saved file: ' + path, 'success');
    }
  } catch (err) { clientLog('Failed to save file network error.', 'error'); }
}

async function apiDeleteFile(filename) {
  try {
    let apiPath = filename.startsWith('/') ? filename : '/' + filename;
    const res = await fetch(`${SERVER_URL}/api/fs/delete?path=${encodeURIComponent(apiPath)}`, { method: 'POST', headers: fetchHeaders });
    if (res.ok) {
      clientLog('Deleted file: ' + filename, 'success');
      fileListArr = fileListArr.filter(f => f !== filename);
      if (activeTab === filename) closeActiveTab();
      else renderSidebar();
    } else {
      clientLog('Failed to delete file: ' + filename, 'error');
    }
  } catch(e) { clientLog('Failed to delete', 'error'); }
}

async function apiRenameFile(oldName, newName) {
  try {
    let oldPath = oldName.startsWith('/') ? oldName : '/' + oldName;
    let newPath = newName.startsWith('/') ? newName : '/' + newName;
    const res = await fetch(`${SERVER_URL}/api/fs/rename?path=${encodeURIComponent(oldPath)}&new_path=${encodeURIComponent(newPath)}`, { method: 'POST', headers: fetchHeaders });
    if (res.ok) {
      clientLog('Renamed file: ' + oldName + ' to ' + newName, 'success');
      fileListArr[fileListArr.indexOf(oldName)] = newName;
      if (fileCache[oldName] !== undefined) {
        fileCache[newName] = fileCache[oldName];
        delete fileCache[oldName];
      }
      if (openTabs.includes(oldName)) {
        openTabs[openTabs.indexOf(oldName)] = newName;
      }
      if (activeTab === oldName) activeTab = newName;
      renderSidebar();
      renderTabs();
      updateUIForFileType();
    } else {
      clientLog('Failed to rename file: ' + oldName, 'error');
    }
  } catch(e) { clientLog('Failed to rename', 'error'); }
}

// --- WebSocket Operations ---
function updateWsBadge(status) {
  const dot = document.getElementById('wsStatusDot');
  const text = document.getElementById('wsStatusText');
  if (!dot || !text) return;
  if (status === 'connected') { dot.className = 'status-dot online'; text.textContent = 'Connected'; }
  else if (status === 'connecting') { dot.className = 'status-dot connecting'; text.textContent = 'Connecting...'; }
  else { dot.className = 'status-dot offline'; text.textContent = 'Disconnected'; }
}

function initWS(){
  updateWsBadge('connecting');
  try { 
    wsConn = new WebSocket(WS_URL); 
  } catch (err) { updateWsBadge('disconnected'); setTimeout(initWS, 2000); return; }

  wsConn.onopen = () => updateWsBadge('connected');
  wsConn.onclose = () => { updateWsBadge('disconnected'); setTimeout(initWS, 2000); };
  wsConn.onerror = (err) => updateWsBadge('disconnected');
  
  wsConn.onmessage = (e) => {
    const line = document.createElement('div');
    line.textContent = e.data;
    if (e.data.startsWith('> Stop') || e.data.startsWith('> [Run')) line.className = 'term-log-run';
    else if (e.data.includes('[ERROR]') || e.data.includes('[Fatal Error]') || e.data.includes('error') || e.data.includes('Error')) line.className = 'term-log-err';
    else if (e.data.includes('[INFO]') || e.data.includes('successfully') || e.data.includes('Connected')) line.className = 'term-log-succ';
    
    terminal.appendChild(line);
    while (terminal.children.length > 500) terminal.removeChild(terminal.firstChild);
    terminal.scrollTop = terminal.scrollHeight;
    
    const countEl = document.getElementById('termCount');
    if (countEl) countEl.textContent = terminal.children.length + ' lines';
  };
}

function sendPayload(payload) {
  if (wsConn && wsConn.readyState === WebSocket.OPEN) {
    wsConn.send(JSON.stringify(payload));
  } else {
    console.warn("WebSocket not connected");
  }  
}

// --- UI Actions ---
function executeFile() {
  if (!activeTab || !activeTab.endsWith('.ti')) return;
  let apiPath = activeTab.startsWith('/') ? activeTab : '/' + activeTab;
  let cmd = 'tien ' + apiPath;
  fetch(`${SERVER_URL}/cmd?msg=${encodeURIComponent(cmd)}`, { headers: fetchHeaders })
    .catch(e => console.error("Execute failed", e));
}

function stopFileTask() {
  if (!activeTab || !activeTab.endsWith('.ti')) return;
  let apiPath = activeTab.startsWith('/') ? activeTab : '/' + activeTab;
  sendPayload({ action: 'stop', name: apiPath });
}

function clearTerminal() {
  terminal.innerHTML = '';
  document.getElementById('termCount').textContent = '0 lines';
}

function startRenameInline(oldName, nameSpanElement) {
  nameSpanElement.innerHTML = '';
  const input = document.createElement('input');
  input.type = 'text';
  input.value = oldName;
  input.className = 'inline-input';
  input.onclick = (e) => e.stopPropagation();
  input.onkeydown = async (e) => {
    if (e.key === 'Enter') {
      const newName = input.value.trim();
      if (newName && newName !== oldName && !fileListArr.includes(newName)) {
        await apiRenameFile(oldName, newName);
      } else {
        renderSidebar(); // cancel if invalid
      }
    } else if (e.key === 'Escape') {
      renderSidebar();
    }
  };
  input.onblur = () => renderSidebar();
  nameSpanElement.appendChild(input);
  input.focus();
}

function renderSidebar() {
  fileList.innerHTML = '';
  for (let filename of fileListArr) {
    const li = document.createElement('li');
    li.className = 'file-item' + (filename === activeTab ? ' active' : '');
    
    const leftDiv = document.createElement('div');
    leftDiv.className = 'file-item-left';
    leftDiv.innerHTML = `&#128196;`;
    
    const nameSpan = document.createElement('span');
    nameSpan.className = 'file-name';
    nameSpan.textContent = filename;
    // Double click to rename
    nameSpan.ondblclick = (e) => { e.stopPropagation(); startRenameInline(filename, nameSpan); };
    leftDiv.appendChild(nameSpan);
    
    // Actions block
    const actionsDiv = document.createElement('div');
    actionsDiv.className = 'file-actions';
    
    const delBtn = document.createElement('span');
    delBtn.className = 'file-action-btn';
    delBtn.innerHTML = '&#128465;'; // Trash icon
    delBtn.title = "Delete File";
    delBtn.onclick = (e) => { e.stopPropagation(); apiDeleteFile(filename); };
    
    actionsDiv.appendChild(delBtn);
    
    li.appendChild(leftDiv);
    li.appendChild(actionsDiv);
    
    li.onclick = () => openFile(filename);
    fileList.appendChild(li);
  }
}

function renderTabs() {
  tabsContainer.innerHTML = '';
  openTabs.forEach(name => {
    const div = document.createElement('div');
    div.className = 'editor-tab' + (name === activeTab ? ' active' : '');
    div.onclick = () => switchTab(name);
    
    const isTi = name.endsWith('.ti');
    div.innerHTML = `<span class="tab-icon">${isTi ? '&lt;/&gt;' : '&#128196;'}</span><span class="tab-name">${escapeHtml(name)}</span>`;
    
    const closeBtn = document.createElement('span');
    closeBtn.className = 'tab-close';
    closeBtn.innerHTML = '&#10005;';
    closeBtn.onclick = (e) => { e.stopPropagation(); closeTab(name); };
    div.appendChild(closeBtn);
    tabsContainer.appendChild(div);
  });
}

function updateUIForFileType() {
  if (!activeTab) {
    btnExecute.style.display = 'none';
    btnStop.style.display = 'none';
    return;
  }
  if (activeTab.endsWith('.ti')) {
    fileTypeStatus.textContent = 'TI Script';
    btnExecute.style.display = 'inline-flex';
    btnStop.style.display = 'inline-flex';
  } else {
    fileTypeStatus.textContent = 'Plain Text';
    btnExecute.style.display = 'none';
    btnStop.style.display = 'none';
  }
}

async function openFile(name) {
  if (activeTab) fileCache[activeTab] = cmdInput.value;
  if (!openTabs.includes(name)) {
    openTabs.push(name);
    if (fileCache[name] === undefined) {
      cmdInput.value = "Loading...";
      fileCache[name] = await fetchFileContent(name);
    }
  }
  switchTab(name);
}

function switchTab(name) {
  if (activeTab && activeTab !== name && openTabs.includes(activeTab)) {
    fileCache[activeTab] = cmdInput.value;
  }
  activeTab = name;
  cmdInput.value = fileCache[name] || '';
  
  updateUIForFileType();
  renderSidebar();
  renderTabs();
  renderEditor();
  updateCursorPos();
}

function closeTab(name) {
  if (activeTab === name) fileCache[activeTab] = cmdInput.value;
  openTabs = openTabs.filter(t => t !== name);
  if (openTabs.length > 0) {
    if (activeTab === name) switchTab(openTabs[openTabs.length - 1]);
  } else {
    activeTab = null;
    cmdInput.value = '';
    updateUIForFileType();
  }
  renderTabs();
  renderSidebar();
  renderEditor();
}

function closeActiveTab() {
  if (activeTab) closeTab(activeTab);
}

function addNewFileInline() {
  const li = document.createElement('li');
  li.className = 'file-item';
  const input = document.createElement('input');
  input.type = 'text';
  input.className = 'inline-input';
  input.placeholder = 'filename.ti';
  
  input.onkeydown = async (e) => {
    if (e.key === 'Enter') {
      let val = input.value.trim();
      if (val) {
        if (!fileListArr.includes(val)) {
          clientLog('Created new file: ' + val, 'success');
          fileListArr.push(val);
          fileCache[val] = '';
          await saveFileContent(val, '');
        }
        if (li.parentNode) li.parentNode.removeChild(li);
        renderSidebar();
        openFile(val);
      } else {
        if (li.parentNode) li.parentNode.removeChild(li);
        renderSidebar();
      }
    } else if (e.key === 'Escape') {
      if (li.parentNode) li.parentNode.removeChild(li);
      renderSidebar();
    }
  };
  
  input.onblur = () => {
    if (li.parentNode) li.parentNode.removeChild(li);
    renderSidebar();
  };
  
  li.appendChild(input);
  fileList.appendChild(li);
  input.focus();
}

async function saveActiveFile() {
  if (!activeTab) return;
  fileCache[activeTab] = cmdInput.value;
  await saveFileContent(activeTab, cmdInput.value);
}

// --- Editor Rendering ---
function escapeHtml(str) { return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }

function highlightTI(code) {
  const tokenRegex = /(\/\/[^\n]*|\/\*[\s\S]*?\*\/)|("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(\b0x[0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b)|(\b(?:int|float|string|bool|void|char|struct)\b)|(\b(?:while|for|if|else|return|break|continue|switch|case|default|do)\b)|(\b(?:true|false|null)\b)|(\b(?:print|delay|autonics_read|autonics_tk_get_pv|autonics_tk_get_sv|autonics_tk_set_slave_address|get_json|get_json_as_string|get_json_as_int|get_json_as_float|get_json_as_bool|http_get|http_post|is_none|file_read|file_write|file_exists|file_remove|relay_get_state|relay_set_state|web_ui_update)\b|\b[a-zA-Z_]\w*(?=\s*\())/g;
  let html = ''; let lastIndex = 0; let match;
  while ((match = tokenRegex.exec(code)) !== null) {
    html += escapeHtml(code.substring(lastIndex, match.index));
    if (match[1]) html += '<span class="tok-cmt">' + escapeHtml(match[1]) + '</span>';
    else if (match[2]) html += '<span class="tok-str">' + escapeHtml(match[2]) + '</span>';
    else if (match[3]) html += '<span class="tok-num">' + escapeHtml(match[3]) + '</span>';
    else if (match[4]) html += '<span class="tok-type">' + escapeHtml(match[4]) + '</span>';
    else if (match[5]) html += '<span class="tok-kw">' + escapeHtml(match[5]) + '</span>';
    else if (match[6]) html += '<span class="tok-const">' + escapeHtml(match[6]) + '</span>';
    else if (match[7]) html += '<span class="tok-fn">' + escapeHtml(match[7]) + '</span>';
    lastIndex = tokenRegex.lastIndex;
  }
  html += escapeHtml(code.substring(lastIndex));
  if (code.endsWith('\n') || code.length === 0) html += ' ';
  return html;
}

function updateGutter() {
  if (!activeTab) { gutter.innerHTML = ''; return; }
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
  if (!activeTab) { document.getElementById('editorStatus').textContent = ''; return; }
  const code = cmdInput.value;
  const pos = cmdInput.selectionStart;
  const linesBefore = code.substring(0, pos).split('\n');
  const curLine = linesBefore.length;
  const curCol = linesBefore[linesBefore.length - 1].length + 1;
  document.getElementById('editorStatus').textContent = `Ln ${curLine}, Col ${curCol}`;

  const gutterLines = gutter.children;
  for (let i = 0; i < gutterLines.length; i++) {
    if (i + 1 === curLine) gutterLines[i].classList.add('active');
    else gutterLines[i].classList.remove('active');
  }
}

function syncScroll() {
  highlightPre.scrollTop = cmdInput.scrollTop;
  highlightPre.scrollLeft = cmdInput.scrollLeft;
  gutter.scrollTop = cmdInput.scrollTop;
}

function renderEditor() {
  if (!activeTab) { highlightCode.innerHTML = ''; updateGutter(); return; }
  const code = cmdInput.value;
  if (activeTab.endsWith('.ti')) {
    highlightCode.innerHTML = highlightTI(code);
  } else {
    highlightCode.innerHTML = escapeHtml(code) + (code.endsWith('\n') || code.length === 0 ? ' ' : '');
  }
  updateGutter();
  syncScroll();
}

cmdInput.addEventListener('input', () => { renderEditor(); updateCursorPos(); });
cmdInput.addEventListener('scroll', syncScroll);
cmdInput.addEventListener('keyup', updateCursorPos);
cmdInput.addEventListener('click', updateCursorPos);
cmdInput.addEventListener('select', updateCursorPos);

// Setup Keyboard shortcuts
cmdInput.addEventListener('keydown', function(e) {
  if (!activeTab) return;
  const start = this.selectionStart;
  const end = this.selectionEnd;
  const text = this.value;

  if (e.key === 'Tab') {
    e.preventDefault();
    if (start === end) {
      if (e.shiftKey) {
        const lineStart = text.lastIndexOf('\n', start - 1) + 1;
        const lineText = text.substring(lineStart, start);
        const spaceMatch = lineText.match(/^ {1,4}/);
        if (spaceMatch) {
          const spacesToRem = spaceMatch[0].length;
          this.setRangeText('', lineStart, lineStart + spacesToRem);
          this.selectionStart = this.selectionEnd = start - spacesToRem;
        }
      } else {
        this.setRangeText('    ', start, end, 'end');
        this.selectionStart = this.selectionEnd = start + 4;
      }
    } else {
      const firstLineStart = text.lastIndexOf('\n', start - 1) + 1;
      let lastLineEnd = text.indexOf('\n', end);
      if (lastLineEnd === -1) lastLineEnd = text.length;
      const lines = text.substring(firstLineStart, lastLineEnd).split('\n');
      let newLines = []; let charsAdded = 0; let firstLineCharsAdded = 0;
      if (e.shiftKey) {
        newLines = lines.map((line, idx) => {
          const spaceMatch = line.match(/^ {1,4}/);
          const rem = spaceMatch ? spaceMatch[0].length : 0;
          if (idx === 0) firstLineCharsAdded = -rem;
          charsAdded -= rem;
          return line.substring(rem);
        });
      } else {
        newLines = lines.map((line, idx) => {
          if (idx === 0) firstLineCharsAdded = 4;
          charsAdded += 4;
          return '    ' + line;
        });
      }
      this.setRangeText(newLines.join('\n'), firstLineStart, lastLineEnd, 'preserve');
      this.selectionStart = start + firstLineCharsAdded;
      this.selectionEnd = end + charsAdded;
    }
    renderEditor(); updateCursorPos(); return;
  }
  
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
        return match ? line.substring(0, match[1].length) + line.substring(match[0].length) : line;
      });
      this.setRangeText(uncommented.join('\n'), firstLineStart, lastLineEnd, 'preserve');
    } else {
      const commented = lines.map(line => '// ' + line);
      this.setRangeText(commented.join('\n'), firstLineStart, lastLineEnd, 'preserve');
    }
    renderEditor(); updateCursorPos(); return;
  }
  if (e.key === 's' && (e.ctrlKey || e.metaKey)) {
    e.preventDefault();
    saveActiveFile();
  }
  if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
    e.preventDefault();
    if(activeTab.endsWith('.ti')) executeFile();
  }
});

// Init
initWS();
fetchFileList();
</script>
</body>
</html>

)rawliteral";

#endif // MANAGE_FILE_SYSTEM_HTML_H
