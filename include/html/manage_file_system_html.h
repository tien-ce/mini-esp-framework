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
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:Arial,sans-serif;background:#232323;color:#ffffff;text-align:center;padding:20px 10px;}
.wrapper{max-width:580px;margin:0 auto;}
h1{font-size:22px;font-weight:bold;margin-bottom:4px;color:#ffffff;}
h2{font-size:26px;font-weight:bold;margin-bottom:20px;color:#ffffff;}
.section-header{font-size:20px;font-weight:bold;padding:8px 0;border-top:1px solid #555;border-bottom:1px solid #555;margin:15px 0;}
button,.btn{display:block;width:100%;background:#1fa3ec;color:#ffffff;border:none;border-radius:6px;padding:12px 10px;font-size:18px;font-weight:normal;margin:8px 0;cursor:pointer;text-decoration:none;transition:background 0.2s;text-align:center;}
button:hover,.btn:hover{background:#1887c9;}
.btn-green{background:#28a745;}
.btn-green:hover{background:#218838;}
.btn-red{background:#d43f3a;}
.btn-red:hover{background:#b92c28;}
.btn-gray{background:#6c757d;}
.btn-gray:hover{background:#5a6268;}
.btn-sm{display:inline-block;width:auto;padding:6px 12px;font-size:14px;margin:0 2px;border-radius:4px;}
.footer-text{font-size:12px;color:#aaaaaa;margin-top:20px;border-top:1px solid #555;padding-top:10px;}
a{text-decoration:none;}

/* Storage Bar */
.storage-box{background:#333333;border:1px solid #555;border-radius:6px;padding:12px;margin-bottom:15px;text-align:left;}
.storage-title{font-size:14px;font-weight:bold;margin-bottom:6px;display:flex;justify-content:space-between;}
.storage-bar-bg{background:#222222;height:12px;border-radius:6px;overflow:hidden;margin-bottom:4px;border:1px solid #444;}
.storage-bar-fill{background:#1fa3ec;height:100%;width:0%;transition:width 0.3s ease;}
.storage-details{font-size:12px;color:#cccccc;display:flex;justify-content:space-between;}

/* Action Toolbar */
.toolbar{display:flex;gap:8px;margin-bottom:15px;}
.toolbar button{margin:0;flex:1;}

/* File List Table */
.fs-card{background:#333333;border:1px solid #555;border-radius:6px;padding:12px;margin-bottom:15px;text-align:left;}
.file-table{width:100%;border-collapse:collapse;margin-top:8px;}
.file-table th{text-align:left;padding:8px 6px;border-bottom:1px solid #555;font-size:14px;color:#aaaaaa;}
.file-table td{padding:8px 6px;border-bottom:1px solid #444;font-size:14px;vertical-align:middle;}
.file-table tr:hover td{background:#3a3a3a;}
.file-name{cursor:pointer;color:#58a6ff;font-weight:bold;word-break:break-all;}
.file-name:hover{text-decoration:underline;}
.file-size{color:#cccccc;white-space:nowrap;font-size:13px;}
.file-actions{text-align:right;white-space:nowrap;}
.empty-msg{padding:20px;text-align:center;color:#aaaaaa;font-style:italic;}

/* Editor Section */
#editorSection{display:none;}
.editor-header{display:flex;flex-direction:column;gap:8px;margin-bottom:10px;text-align:left;}
.editor-path-row{display:flex;align-items:center;gap:8px;}
.editor-path-row label{font-size:14px;font-weight:bold;white-space:nowrap;}
.editor-path-row input[type=text]{flex:1;padding:8px 10px;border:1px solid #666;border-radius:4px;background:#1e1e1e;color:#ffffff;font-size:14px;font-family:monospace;}
.editor-textarea{width:100%;height:350px;background:#181818;color:#00ff66;font-family:Consolas,Monaco,monospace;font-size:14px;line-height:1.4;padding:12px;border:1px solid #555;border-radius:4px;resize:vertical;white-space:pre;overflow:auto;tab-size:2;}
.editor-toolbar{display:flex;gap:8px;margin-top:10px;}
.editor-toolbar button{flex:1;margin:0;}

/* Status / Alerts */
.status-msg{padding:8px 12px;border-radius:4px;margin-bottom:10px;font-size:13px;display:none;text-align:left;}
.status-success{background:#28a74533;border:1px solid #28a745;color:#28a745;}
.status-error{background:#d43f3a33;border:1px solid #d43f3a;color:#ff6b6b;}
.status-info{background:#1fa3ec33;border:1px solid #1fa3ec;color:#58a6ff;}
</style>
</head>
<body>
<div class='wrapper'>
<h1 id='deviceHeader'>%HEADER_TITLE%</h1>
<h2 id='deviceSubHeader'>%HEADER_SUBTITLE%</h2>

<div class='section-header'>Manage File System</div>

<div id='globalStatus' class='status-msg'></div>

<!-- SECTION 1: FILE LIST VIEW -->
<div id='listViewSection'>
  <!-- Storage Statistics -->
  <div class='storage-box'>
    <div class='storage-title'>
      <span>LittleFS Storage</span>
      <span id='storagePercent'>0%</span>
    </div>
    <div class='storage-bar-bg'>
      <div id='storageBarFill' class='storage-bar-fill'></div>
    </div>
    <div class='storage-details'>
      <span id='storageUsed'>Used: -</span>
      <span id='storageTotal'>Total: -</span>
    </div>
  </div>

  <!-- Action Toolbar -->
  <div class='toolbar'>
    <button class='btn-green' onclick='openNewFileModal()'>+ New File</button>
    <button class='btn-gray' onclick='fetchFileList()'>↻ Refresh</button>
  </div>

  <!-- Files Table Card -->
  <div class='fs-card'>
    <table class='file-table'>
      <thead>
        <tr>
          <th>Name</th>
          <th style='width:80px;'>Size</th>
          <th style='width:120px;text-align:right;'>Actions</th>
        </tr>
      </thead>
      <tbody id='fileListTbody'>
        <tr><td colspan='3' class='empty-msg'>Loading files...</td></tr>
      </tbody>
    </table>
  </div>

  <a href='/tools'><button>Back to Tools</button></a>
  <a href='/'><button class='btn-gray'>Main Menu</button></a>
</div>

<!-- SECTION 2: FILE EDITOR VIEW -->
<div id='editorSection'>
  <div class='fs-card'>
    <div class='editor-header'>
      <div class='editor-path-row'>
        <label for='editorFilePath'>File Path:</label>
        <input type='text' id='editorFilePath' placeholder='/config.txt'>
      </div>
    </div>

    <textarea id='editorContent' class='editor-textarea' placeholder='File content...' spellcheck='false'></textarea>

    <div class='editor-toolbar'>
      <button class='btn-green' onclick='saveCurrentFile()'>💾 Save</button>
      <button class='btn-gray' onclick='closeEditor()'>✕ Cancel</button>
    </div>
  </div>
</div>

<div class='footer-text' id='footerText'>
    Model: %CHIP_MODEL% | MAC: %MAC_ADDR%<br>
    %FOOTER_TEXT%
</div>
</div>

<script>
/**
 * -----------------------------------------------------------------------------
 * API ENDPOINT CONFIGURATION
 * -----------------------------------------------------------------------------
 * 1. LIST_URL (GET):
 *    Returns list of files & storage info.
 *    Response Format (JSON):
 *    {
 *      "totalBytes": 1441792,
 *      "usedBytes": 28672,
 *      "files": [
 *        { "name": "/web_config.txt", "size": 120 },
 *        { "name": "/wifi_config.txt", "size": 85 }
 *      ]
 *    }
 *    (Plain array of objects: [ {"name":"/path", "size": 120}, ... ] is also supported).
 *
 * 2. READ_URL (GET):
 *    Request: GET /api/fs/read?path=/filename
 *    Response: Raw file content (text/plain) or JSON { "path": "...", "content": "..." }
 *
 * 3. SAVE_URL (POST):
 *    Request: POST /api/fs/save
 *    Payload (JSON): { "path": "/filename", "content": "file content here" }
 *    Response: "OK" or JSON { "success": true }
 *
 * 4. DELETE_URL (POST):
 *    Request: POST /api/fs/delete?path=/filename
 *    Response: "OK" or JSON { "success": true }
 * -----------------------------------------------------------------------------
 */
const API_FS_LIST   = '/api/fs/list';
const API_FS_READ   = '/api/fs/read';
const API_FS_SAVE   = '/api/fs/save';
const API_FS_DELETE = '/api/fs/delete';

let isCreatingNew = false;

/**
 * Format bytes into human readable string (e.g. 1.25 KB)
 */
function formatBytes(bytes) {
  if (isNaN(bytes) || bytes === null || bytes === undefined) return '-';
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

/**
 * Show a notification message
 */
function showStatus(msg, type = 'info', duration = 3500) {
  const el = document.getElementById('globalStatus');
  el.textContent = msg;
  el.className = 'status-msg status-' + type;
  el.style.display = 'block';
  if (duration > 0) {
    setTimeout(() => {
      el.style.display = 'none';
    }, duration);
  }
}

/**
 * Fetch and display file system contents
 */
function fetchFileList() {
  const tbody = document.getElementById('fileListTbody');
  tbody.innerHTML = "<tr><td colspan='3' class='empty-msg'>Loading files...</td></tr>";

  fetch(API_FS_LIST)
    .then(res => {
      if (!res.ok) throw new Error('HTTP ' + res.status + ' ' + res.statusText);
      return res.json();
    })
    .then(data => {
      let files = [];
      let totalBytes = 0;
      let usedBytes = 0;

      if (Array.isArray(data)) {
        files = data;
      } else if (data && typeof data === 'object') {
        files = data.files || [];
        totalBytes = data.totalBytes || 0;
        usedBytes = data.usedBytes || 0;
      }

      // Update storage stats
      if (totalBytes > 0) {
        const pct = Math.min(100, Math.round((usedBytes / totalBytes) * 100));
        document.getElementById('storagePercent').textContent = pct + '%';
        document.getElementById('storageBarFill').style.width = pct + '%';
        document.getElementById('storageUsed').textContent = 'Used: ' + formatBytes(usedBytes);
        document.getElementById('storageTotal').textContent = 'Total: ' + formatBytes(totalBytes);
      } else {
        document.getElementById('storagePercent').textContent = '-';
        document.getElementById('storageBarFill').style.width = '0%';
        document.getElementById('storageUsed').textContent = 'Files: ' + files.length;
        document.getElementById('storageTotal').textContent = 'Total: -';
      }

      // Render table
      if (files.length === 0) {
        tbody.innerHTML = "<tr><td colspan='3' class='empty-msg'>No files found in file system.</td></tr>";
        return;
      }

      tbody.innerHTML = '';
      files.forEach(f => {
        const filePath = typeof f === 'string' ? f : f.name;
        const fileSize = typeof f === 'object' && f.size !== undefined ? f.size : null;

        const tr = document.createElement('tr');

        // Name column
        const tdName = document.createElement('td');
        const nameLink = document.createElement('span');
        nameLink.className = 'file-name';
        nameLink.textContent = filePath;
        nameLink.title = 'Click to edit ' + filePath;
        nameLink.onclick = () => openFile(filePath);
        tdName.appendChild(nameLink);

        // Size column
        const tdSize = document.createElement('td');
        tdSize.className = 'file-size';
        tdSize.textContent = formatBytes(fileSize);

        // Actions column
        const tdActions = document.createElement('td');
        tdActions.className = 'file-actions';

        const btnEdit = document.createElement('button');
        btnEdit.className = 'btn btn-sm btn-green';
        btnEdit.textContent = 'Edit';
        btnEdit.onclick = () => openFile(filePath);

        const btnDel = document.createElement('button');
        btnDel.className = 'btn btn-sm btn-red';
        btnDel.textContent = 'Del';
        btnDel.onclick = () => deleteFile(filePath);

        tdActions.appendChild(btnEdit);
        tdActions.appendChild(btnDel);

        tr.appendChild(tdName);
        tr.appendChild(tdSize);
        tr.appendChild(tdActions);
        tbody.appendChild(tr);
      });
    })
    .catch(err => {
      console.error('Error fetching file list:', err);
      tbody.innerHTML = "<tr><td colspan='3' class='empty-msg' style='color:#ff6b6b;'>Failed to load files (" + err.message + ")</td></tr>";
      showStatus('Failed to load file list: ' + err.message, 'error');
    });
}

/**
 * Open file in editor and fetch its content
 */
function openFile(filePath) {
  isCreatingNew = false;
  showStatus('Loading ' + filePath + '...', 'info', 1500);

  const url = API_FS_READ + '?path=' + encodeURIComponent(filePath);

  fetch(url)
    .then(res => {
      if (!res.ok) throw new Error('HTTP ' + res.status + ' ' + res.statusText);
      const contentType = res.headers.get('content-type') || '';
      if (contentType.includes('application/json')) {
        return res.json().then(j => j.content !== undefined ? j.content : JSON.stringify(j, null, 2));
      }
      return res.text();
    })
    .then(content => {
      document.getElementById('editorFilePath').value = filePath;
      document.getElementById('editorFilePath').readOnly = true;
      document.getElementById('editorContent').value = content;

      document.getElementById('listViewSection').style.display = 'none';
      document.getElementById('editorSection').style.display = 'block';
      document.getElementById('editorContent').focus();
    })
    .catch(err => {
      console.error('Error reading file:', err);
      showStatus('Error reading file: ' + err.message, 'error');
    });
}

/**
 * Open editor for creating a new file
 */
function openNewFileModal() {
  isCreatingNew = true;
  document.getElementById('editorFilePath').value = '/';
  document.getElementById('editorFilePath').readOnly = false;
  document.getElementById('editorContent').value = '';

  document.getElementById('listViewSection').style.display = 'none';
  document.getElementById('editorSection').style.display = 'block';
  document.getElementById('editorFilePath').focus();
}

/**
 * Close editor and return to list view
 */
function closeEditor() {
  document.getElementById('editorSection').style.display = 'none';
  document.getElementById('listViewSection').style.display = 'block';
  document.getElementById('editorContent').value = '';
}

/**
 * Save current editor content to backend
 */
function saveCurrentFile() {
  let filePath = document.getElementById('editorFilePath').value.trim();
  const content = document.getElementById('editorContent').value;

  if (!filePath || filePath === '/') {
    alert('Please enter a valid file path (e.g. /my_config.txt)');
    document.getElementById('editorFilePath').focus();
    return;
  }

  if (!filePath.startsWith('/')) {
    filePath = '/' + filePath;
    document.getElementById('editorFilePath').value = filePath;
  }

  showStatus('Saving ' + filePath + '...', 'info', 0);

  const payload = {
    path: filePath,
    content: content
  };

  fetch(API_FS_SAVE, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(payload)
  })
    .then(res => {
      if (!res.ok) throw new Error('HTTP ' + res.status + ' ' + res.statusText);
      return res.text();
    })
    .then(respText => {
      showStatus('File ' + filePath + ' saved successfully!', 'success', 3000);
      closeEditor();
      fetchFileList();
    })
    .catch(err => {
      console.error('Error saving file:', err);
      showStatus('Failed to save file: ' + err.message, 'error');
    });
}

/**
 * Delete a file
 */
function deleteFile(filePath) {
  if (!confirm('Are you sure you want to delete "' + filePath + '"?')) {
    return;
  }

  showStatus('Deleting ' + filePath + '...', 'info', 0);

  const url = API_FS_DELETE + '?path=' + encodeURIComponent(filePath);

  fetch(url, {
    method: 'POST'
  })
    .then(res => {
      if (!res.ok) throw new Error('HTTP ' + res.status + ' ' + res.statusText);
      return res.text();
    })
    .then(() => {
      showStatus('File ' + filePath + ' deleted!', 'success', 3000);
      fetchFileList();
    })
    .catch(err => {
      console.error('Error deleting file:', err);
      showStatus('Failed to delete file: ' + err.message, 'error');
    });
}

/**
 * Support Tab key in editor textarea for code indentation
 */
document.getElementById('editorContent').addEventListener('keydown', function(e) {
  if (e.key === 'Tab') {
    e.preventDefault();
    const start = this.selectionStart;
    const end = this.selectionEnd;
    this.value = this.value.substring(0, start) + '  ' + this.value.substring(end);
    this.selectionStart = this.selectionEnd = start + 2;
  } else if ((e.ctrlKey || e.metaKey) && e.key === 's') {
    e.preventDefault();
    saveCurrentFile();
  }
});

// Load file list on initial page load
window.addEventListener('load', fetchFileList);
</script>
</body>
</html>
)rawliteral";

#endif // MANAGE_FILE_SYSTEM_HTML_H
