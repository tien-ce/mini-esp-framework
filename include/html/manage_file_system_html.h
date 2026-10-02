#ifndef MANAGE_FILE_SYSTEM_HTML_H
#define MANAGE_FILE_SYSTEM_HTML_H

#include <Arduino.h>

// Generated from html_test/file_system.html (comment-only lines and indentation stripped to save heap).
const char MANAGE_FILE_SYSTEM_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>File Manager</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:ital,wght@0,400;0,500;0,600;1,400&family=Spline+Sans+Mono:wght@400;500&display=swap">
<style>
:root {
--mask: #164236;
--mask-deep: #0f2f28;
--mask-raised: #1f5244;
--line: #28584a;
--line-strong: #3a6f60;
--trace: #3d7f6a;
--silk: #e8ede6;
--silk-dim: #a3b8ae;
--silk-faint: #88aca0;
--gold: #d4af5a;
--mint: #7ce0a8;
--fault: #f07178;
--sky: #7fc8f8;
--font-ui: "Barlow Semi Condensed", "Segoe UI", system-ui, -apple-system, sans-serif;
--font-code: "Spline Sans Mono", Consolas, "SF Mono", Menlo, monospace;
--topbar-h: 48px;
--head-h: 38px;
--row-h: 26px;
--indent: 16px;
--tree-pad: 14px;
--code-size: 13px;
--code-lh: 22px;
--code-pad-y: 12px;
--code-pad-x: 16px;
color-scheme: dark;
}
* { margin: 0; padding: 0; box-sizing: border-box; }
html, body { height: 100%; }
body {
display: flex;
flex-direction: column;
height: 100vh;
height: 100dvh;
overflow: hidden;
background: var(--mask-deep);
color: var(--silk);
font-family: var(--font-ui);
font-size: 14px;
line-height: 1.4;
-webkit-font-smoothing: antialiased;
}
body.resizing { cursor: ns-resize; user-select: none; }
button, input, textarea { font: inherit; color: inherit; }
button { cursor: pointer; }
[hidden] { display: none !important; }
:focus-visible { outline: 2px solid var(--gold); outline-offset: 2px; }
* { scrollbar-width: thin; scrollbar-color: #2f6354 transparent; }
::-webkit-scrollbar { width: 10px; height: 10px; }
::-webkit-scrollbar-track, ::-webkit-scrollbar-corner { background: transparent; }
::-webkit-scrollbar-thumb { background: #2f6354; border: 2px solid transparent; background-clip: padding-box; border-radius: 6px; }
.topbar {
flex: 0 0 var(--topbar-h);
display: flex;
align-items: center;
gap: 12px;
padding: 0 16px;
background: var(--mask);
border-bottom: 1px solid var(--line);
}
.chip-mark {
position: relative;
flex-shrink: 0;
width: 26px;
height: 26px;
margin: 0 5px;
display: grid;
place-items: center;
border: 1px solid var(--line-strong);
border-radius: 3px;
background: var(--mask-deep);
color: var(--silk-dim);
font-size: 11px;
font-weight: 600;
line-height: 1;
}
.chip-mark::before, .chip-mark::after {
content: '';
position: absolute;
top: 3px;
width: 4px;
height: 17px;
background: repeating-linear-gradient(to bottom, var(--gold) 0 2px, transparent 2px 5px);
}
.chip-mark::before { left: -5px; }
.chip-mark::after { right: -5px; }
.home-link {
flex-shrink: 0;
display: inline-flex;
align-items: center;
gap: 6px;
height: 30px;
margin-left: -6px;
padding: 0 10px 0 8px;
border-radius: 4px;
color: var(--silk-dim);
font-size: 15px;
font-weight: 500;
text-decoration: none;
}
.home-link:hover { background: rgba(232, 237, 230, .08); color: var(--silk); }
.home-link svg { width: 16px; height: 16px; }
.topbar-sep { flex-shrink: 0; width: 1px; height: 22px; background: var(--line-strong); }
.titles { display: flex; align-items: baseline; gap: 10px; min-width: 0; }
.titles h1 { font-size: 17px; font-weight: 600; white-space: nowrap; }
.titles p { font-size: 14px; color: var(--silk-dim); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.link-status {
margin-left: auto;
display: flex;
align-items: center;
gap: 8px;
font-size: 14px;
color: var(--silk-dim);
white-space: nowrap;
}
.led { width: 8px; height: 8px; flex-shrink: 0; border-radius: 50%; background: var(--silk-faint); }
.led.online { background: var(--mint); box-shadow: 0 0 0 3px rgba(124, 224, 168, .16), 0 0 10px rgba(124, 224, 168, .55); }
.led.connecting { background: var(--gold); animation: led-blink .9s ease-in-out infinite alternate; }
.led.offline { background: var(--fault); }
@keyframes led-blink { to { opacity: .25; } }
.icon-btn {
flex-shrink: 0;
width: 28px;
height: 28px;
display: inline-grid;
place-items: center;
border: 0;
border-radius: 4px;
background: transparent;
color: var(--silk-dim);
}
.icon-btn:hover { background: rgba(232, 237, 230, .08); color: var(--silk); }
.icon-btn svg { width: 16px; height: 16px; }
.icon-btn.is-busy svg { animation: spin .8s linear infinite; }
@keyframes spin { to { transform: rotate(360deg); } }
.drawer-toggle { display: none; margin-left: -6px; }
.app { flex: 1; min-height: 0; display: flex; }
.sidebar {
flex-shrink: 0;
width: 272px;
display: flex;
flex-direction: column;
background: var(--mask);
border-right: 1px solid var(--line);
}
.pane-head {
flex: 0 0 var(--head-h);
display: flex;
align-items: center;
gap: 2px;
padding: 0 8px 0 var(--tree-pad);
border-bottom: 1px solid var(--line);
}
.pane-head h2 { flex: 1; font-size: 15px; font-weight: 600; }
.tree-scroll { flex: 1; min-height: 0; overflow: auto; padding: 8px 0 16px; }
.scrim { display: none; }
.tree, .tree ul { list-style: none; }
.node { position: relative; }
.node::before, .node::after {
position: absolute;
z-index: 1;
top: 0;
left: calc(var(--tree-pad) + (var(--d) - 1) * var(--indent) + 4px);
width: 2px;
pointer-events: none;
transition: background-color .18s;
}
.node:not(.root)::before { content: ''; bottom: 0; background: var(--trace); }
.node:not(.root):last-child::before { bottom: auto; height: calc(var(--row-h) / 2 + 1px); }
.node.net-pass::before { background: var(--gold); }
.node.net-end::after { content: ''; height: calc(var(--row-h) / 2 + 1px); background: var(--gold); }
.row {
position: relative;
display: flex;
align-items: center;
gap: 8px;
height: var(--row-h);
padding: 0 6px 0 calc(var(--tree-pad) + var(--d) * var(--indent));
color: var(--silk-dim);
font-size: 15px;
cursor: pointer;
user-select: none;
}
.row:hover { background: rgba(232, 237, 230, .05); color: var(--silk); }
.row:focus-visible { outline-offset: -2px; }
.node:not(.root) > .row::before {
content: '';
position: absolute;
left: calc(var(--tree-pad) + (var(--d) - 1) * var(--indent) + 4px);
top: calc(var(--row-h) / 2 - 1px);
width: calc(var(--indent) - 4px);
height: 2px;
background: var(--trace);
transition: background-color .18s;
}
.node.open > .row::after {
content: '';
position: absolute;
left: calc(var(--tree-pad) + var(--d) * var(--indent) + 4px);
top: calc(var(--row-h) / 2 + 5px);
bottom: 0;
width: 2px;
background: var(--trace);
transition: background-color .18s;
}
.node.net-end > .row::before, .node.net-anc > .row::after { background: var(--gold); }
.node.net-target > .row { background: var(--mask-raised); color: var(--silk); }
.pad {
flex-shrink: 0;
width: 10px;
height: 10px;
border: 2px solid var(--silk-faint);
border-radius: 50%;
transition: background-color .18s, border-color .18s;
}
.pad[data-kind="script"] { background: var(--silk-faint); }
.pad[data-kind="dir"] { border-radius: 2px; border-color: var(--silk-dim); }
.node.open > .row .pad[data-kind="dir"] { background: var(--silk-dim); }
.net-end > .row .pad, .net-anc > .row .pad, .net-target > .row .pad { border-color: var(--gold); }
.net-target > .row .pad[data-kind="script"],
.node.open.net-anc > .row .pad,
.node.open.net-target > .row .pad { background: var(--gold); }
.file-name { flex: 1; min-width: 0; overflow: hidden; white-space: nowrap; text-overflow: ellipsis; }
.row[data-kind="dir"] .file-name { font-weight: 500; }
.root > .row .file-name { font-weight: 600; color: var(--silk); }
.size { flex-shrink: 0; font-size: 13px; color: var(--silk-faint); font-variant-numeric: tabular-nums; }
.row-more-btn {
display: none;
flex-shrink: 0;
width: 24px;
height: 20px;
place-items: center;
border: 0;
border-radius: 3px;
background: transparent;
color: var(--silk-dim);
}
.row-more-btn svg { width: 16px; height: 16px; }
.row-more-btn:hover { background: rgba(232, 237, 230, .1); color: var(--silk); }
.row:hover .row-more-btn, .row:focus-visible .row-more-btn, .row.menu-open .row-more-btn { display: inline-grid; }
.row:hover .size, .row:focus-visible .size, .row.menu-open .size { display: none; }
@media (hover: none) {
.row .row-more-btn { display: inline-grid; }
.row .size { display: inline; }
}
.info-row { cursor: default; color: var(--silk-faint); font-size: 14px; font-style: italic; }
.info-row:hover { background: none; color: var(--silk-faint); }
.info-row.is-error { color: var(--fault); font-style: normal; }
.slot-row { cursor: default; }
.slot-row:hover { background: none; }
.inline-input {
flex: 1;
min-width: 0;
height: 21px;
padding: 0 6px;
border: 1px solid var(--gold);
border-radius: 3px;
background: var(--mask-deep);
color: var(--silk);
font-size: 14.5px;
outline: none;
}
.inline-input.input-error { border-color: var(--fault); }
.inline-input::placeholder { color: var(--silk-faint); }
ul.grow { animation: trace-in .22s ease-out; }
@keyframes trace-in {
from { clip-path: inset(0 0 100% 0); }
to { clip-path: inset(0 0 0 0); }
}
.work { flex: 1; min-width: 0; min-height: 0; display: flex; flex-direction: column; }
.editor-section { flex: 0 0 66%; min-height: 120px; display: flex; flex-direction: column; }
.editor-head {
flex: 0 0 var(--head-h);
display: flex;
background: var(--mask);
box-shadow: inset 0 -1px 0 var(--line);
}
.tabs { flex: 1; min-width: 0; display: flex; overflow-x: auto; scrollbar-width: none; }
.tabs::-webkit-scrollbar { display: none; }
.tab {
flex-shrink: 0;
display: flex;
align-items: center;
gap: 8px;
padding: 0 6px 0 14px;
border-right: 1px solid var(--line);
color: var(--silk-dim);
font-size: 15px;
white-space: nowrap;
cursor: pointer;
}
.tab:hover { color: var(--silk); background: rgba(232, 237, 230, .04); }
.tab.active { background: var(--mask-deep); color: var(--silk); }
.tab:focus-visible { outline-offset: -2px; }
.tab .pad { width: 8px; height: 8px; }
.tab.active .pad { border-color: var(--gold); }
.tab.active .pad[data-kind="script"] { background: var(--gold); }
.tab-dir { font-size: 13px; color: var(--silk-faint); }
.tab-close {
width: 20px;
height: 20px;
display: grid;
place-items: center;
border: 0;
border-radius: 3px;
background: transparent;
color: var(--silk-faint);
opacity: 0;
}
.tab-close svg { width: 12px; height: 12px; }
.tab:hover .tab-close, .tab.active .tab-close, .tab.is-dirty .tab-close { opacity: 1; }
.tab-close:hover { background: rgba(232, 237, 230, .1); color: var(--silk); }
.tab.is-dirty .tab-close::before { content: ''; width: 8px; height: 8px; border-radius: 50%; background: var(--silk-dim); }
.tab.is-dirty .tab-close svg { display: none; }
.tab.is-dirty .tab-close:hover::before { display: none; }
.tab.is-dirty .tab-close:hover svg { display: block; }
.editor-actions { flex-shrink: 0; display: flex; align-items: center; gap: 6px; padding: 0 10px; }
.btn {
display: inline-flex;
align-items: center;
gap: 6px;
height: 28px;
padding: 0 12px;
border: 1px solid var(--line-strong);
border-radius: 4px;
background: transparent;
color: var(--silk);
font-size: 15px;
font-weight: 500;
white-space: nowrap;
transition: background-color .15s, border-color .15s, color .15s;
}
.btn svg { width: 14px; height: 14px; flex-shrink: 0; }
.btn:hover { background: rgba(232, 237, 230, .08); }
.btn:disabled { opacity: .5; cursor: default; }
.btn-save.is-dirty { background: var(--gold); border-color: var(--gold); color: var(--mask-deep); }
.btn-save.is-dirty:hover { background: #e0bf70; }
.btn-run { background: var(--mint); border-color: var(--mint); color: var(--mask-deep); }
.btn-run:hover { background: #98eabd; }
.btn-stop { border-color: rgba(240, 113, 120, .55); color: var(--fault); }
.btn-stop:hover { background: rgba(240, 113, 120, .12); }
.btn-danger { background: var(--fault); border-color: var(--fault); color: #2b0d11; }
.btn-danger:hover { background: #f58b91; }
.editor-box { position: relative; flex: 1; min-height: 0; display: flex; background: var(--mask-deep); }
.gutter {
flex: 0 0 56px;
overflow: hidden;
padding: var(--code-pad-y) 12px calc(var(--code-pad-y) + 40px) 0;
border-right: 1px solid var(--line);
color: #55877a;
font-family: var(--font-code);
font-size: var(--code-size);
line-height: var(--code-lh);
text-align: right;
user-select: none;
}
.gutter-line { height: var(--code-lh); }
.gutter-line.active { color: var(--silk); }
.editor-wrap { position: relative; flex: 1; min-width: 0; overflow: hidden; }
.editor-highlight, .editor-textarea, .char-probe {
font-family: var(--font-code);
font-size: var(--code-size);
line-height: var(--code-lh);
font-variant-ligatures: none;
letter-spacing: 0;
tab-size: 4;
white-space: pre;
}
.editor-highlight, .editor-textarea {
position: absolute;
inset: 0;
width: 100%;
height: 100%;
margin: 0;
padding: var(--code-pad-y) var(--code-pad-x);
border: 0;
outline: none;
}
.editor-highlight {
z-index: 1;
overflow: hidden;
padding-right: calc(var(--code-pad-x) + 24px);
padding-bottom: calc(var(--code-pad-y) + 24px);
pointer-events: none;
background: transparent;
color: #dfe7e1;
}
.editor-highlight code { font: inherit; }
.editor-textarea {
z-index: 2;
overflow: auto;
resize: none;
background: transparent;
color: transparent;
caret-color: var(--gold);
}
.editor-textarea::selection { background: rgba(127, 200, 248, .28); }
.char-probe { position: absolute; visibility: hidden; }
.line-hl, .bracket-hl {
position: absolute;
z-index: 0;
display: none;
height: var(--code-lh);
pointer-events: none;
}
.line-hl { left: 0; right: 0; background: rgba(232, 237, 230, .04); }
.bracket-hl { background: rgba(212, 175, 90, .2); outline: 1px solid rgba(212, 175, 90, .55); border-radius: 2px; }
.tok-kw { color: #e9c877; }
.tok-type { color: #8fd3c1; }
.tok-fn { color: #9dd7ff; }
.tok-str { color: #f4a98a; }
.tok-num { color: #c3a6f5; }
.tok-const { color: #f59fb4; }
.tok-cmt { color: #74998e; }
.empty-state {
position: absolute;
inset: 0;
z-index: 3;
display: none;
overflow: auto;
padding: 32px 40px;
background: var(--mask-deep);
}
.editor-box.is-empty .empty-state { display: block; }
.editor-box.is-empty .gutter, .editor-box.is-empty .editor-wrap { visibility: hidden; }
.empty-state h3 { margin-bottom: 6px; font-size: 22px; font-weight: 600; }
.empty-state p { max-width: 46ch; color: var(--silk-dim); font-size: 16px; line-height: 1.5; }
.empty-state .btn { margin-top: 18px; }
.keys {
display: grid;
grid-template-columns: max-content 1fr;
gap: 8px 20px;
max-width: 440px;
margin-top: 26px;
padding-top: 18px;
border-top: 1px solid var(--line);
color: var(--silk-dim);
font-size: 15px;
}
.keys dt { white-space: nowrap; color: var(--silk-faint); }
kbd {
display: inline-block;
min-width: 22px;
padding: 0 6px;
border: 1px solid var(--line-strong);
border-bottom-width: 2px;
border-radius: 4px;
background: var(--mask);
color: var(--silk);
font: 500 13px/1.5 var(--font-ui);
text-align: center;
}
.statusline {
flex: 0 0 26px;
display: flex;
align-items: center;
justify-content: space-between;
gap: 16px;
padding: 0 14px 0 16px;
background: var(--mask);
border-top: 1px solid var(--line);
color: var(--silk-dim);
font-size: 14px;
white-space: nowrap;
}
#filePathStatus { min-width: 0; overflow: hidden; text-overflow: ellipsis; }
.status-right { flex-shrink: 0; display: flex; gap: 16px; font-variant-numeric: tabular-nums; }
.save-note.ok { color: var(--mint); }
.save-note.err { color: var(--fault); }
.resizer {
position: relative;
flex: 0 0 8px;
background: var(--mask-deep);
border-top: 1px solid var(--line);
border-bottom: 1px solid var(--line);
cursor: ns-resize;
touch-action: none;
}
.resizer::after {
content: '';
position: absolute;
top: 50%;
left: 50%;
width: 36px;
height: 2px;
margin: -1px 0 0 -18px;
border-radius: 1px;
background: var(--trace);
}
.resizer:hover::after, .resizer:focus-visible::after, body.resizing .resizer::after { background: var(--gold); }
.resizer:focus-visible { outline: none; }
.output { flex: 1 1 0; min-height: 72px; display: flex; flex-direction: column; background: var(--mask-deep); }
.output-head {
flex: 0 0 34px;
display: flex;
align-items: center;
gap: 12px;
padding: 0 10px 0 16px;
background: var(--mask);
border-bottom: 1px solid var(--line);
}
.output-head h2 { font-size: 15px; font-weight: 600; }
.line-count { color: var(--silk-faint); font-size: 14px; font-variant-numeric: tabular-nums; }
.text-btn {
margin-left: auto;
height: 24px;
padding: 0 8px;
border: 0;
border-radius: 4px;
background: transparent;
color: var(--silk-dim);
font-size: 14px;
}
.text-btn:hover { background: rgba(232, 237, 230, .08); color: var(--silk); }
#terminal {
flex: 1;
min-height: 0;
overflow-y: auto;
padding: 10px 16px 14px;
color: #c9d6cf;
font-family: var(--font-code);
font-size: 12.5px;
line-height: 1.6;
white-space: pre-wrap;
word-break: break-all;
}
#terminal:empty::before { content: 'Messages from the board show up here.'; color: var(--silk-faint); font-family: var(--font-ui); font-size: 15px; }
.log-run { color: var(--sky); }
.log-err { color: var(--fault); }
.log-ok { color: var(--mint); }
.log-client { color: var(--silk-dim); }
.popover {
position: fixed;
z-index: 100;
min-width: 180px;
padding: 4px;
background: var(--mask-raised);
border: 1px solid var(--line-strong);
border-radius: 6px;
box-shadow: 0 10px 28px rgba(3, 14, 11, .55);
}
.menu-item {
display: flex;
align-items: center;
justify-content: space-between;
gap: 24px;
width: 100%;
height: 30px;
padding: 0 10px;
border: 0;
border-radius: 4px;
background: transparent;
color: var(--silk);
font-size: 15px;
text-align: left;
}
.menu-item:hover, .menu-item:focus-visible { background: rgba(232, 237, 230, .09); outline: none; }
.menu-item:focus-visible { box-shadow: inset 0 0 0 1px var(--gold); }
.menu-item .hint { color: var(--silk-faint); font-size: 13px; }
.menu-item.danger { color: var(--fault); }
.menu-item.danger:hover, .menu-item.danger:focus-visible { background: rgba(240, 113, 120, .14); }
.menu-sep { height: 1px; margin: 4px 6px; background: var(--line-strong); }
.confirm { max-width: 320px; padding: 14px; }
.confirm-msg { margin-bottom: 14px; color: var(--silk-dim); font-size: 15px; line-height: 1.45; }
.confirm-msg strong { color: var(--silk); font-weight: 600; word-break: break-all; }
.confirm-actions { display: flex; justify-content: flex-end; gap: 8px; }
@media (max-width: 760px) {
.drawer-toggle { display: inline-grid; }
.titles p, .chip-mark, .home-link span { display: none; }
.home-link { margin-left: 0; padding: 0 7px; }
.sidebar {
position: fixed;
z-index: 50;
top: var(--topbar-h);
bottom: 0;
left: 0;
width: min(86vw, 320px);
visibility: hidden;
transform: translateX(-100%);
transition: transform .22s ease, visibility 0s linear .22s;
box-shadow: 12px 0 32px rgba(3, 14, 11, .5);
}
body.drawer-open .sidebar { visibility: visible; transform: none; transition: transform .22s ease; }
.scrim {
display: block;
position: fixed;
z-index: 40;
inset: var(--topbar-h) 0 0 0;
background: rgba(3, 14, 11, .55);
opacity: 0;
pointer-events: none;
transition: opacity .22s;
}
body.drawer-open .scrim { opacity: 1; pointer-events: auto; }
.editor-actions { padding: 0 8px; }
.editor-actions .btn { padding: 0 9px; }
.editor-actions .btn span { display: none; }
.tab-dir { display: none; }
.gutter { flex-basis: 44px; padding-right: 8px; }
.empty-state { padding: 24px 16px; }
.statusline #fileTypeStatus { display: none; }
}
@media (prefers-reduced-motion: reduce) {
*, *::before, *::after { animation: none !important; transition: none !important; }
}
</style>
</head>
<body>
<header class="topbar">
<button class="icon-btn drawer-toggle" id="drawerToggle" type="button" aria-label="Files" aria-controls="sidebar" aria-expanded="false">
<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" aria-hidden="true"><path d="M2.5 4h11M2.5 8h11M2.5 12h11"/></svg>
</button>
<a class="home-link" href="/" title="Back to the main menu" aria-label="Home">
<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2.25 7.5 8 2.75l5.75 4.75"/><path d="M3.75 6.5v6.75h3.5V9.75h1.5v3.5h3.5V6.5"/></svg>
<span>Home</span>
</a>
<span class="topbar-sep" aria-hidden="true"></span>
<span class="chip-mark" aria-hidden="true">S3</span>
<div class="titles">
<h1>File manager</h1>
<p>ESP mini framework</p>
</div>
<div class="link-status" role="status" title="Live link for script output and Stop">
<span class="led" id="wsStatusDot"></span>
<span id="wsStatusText">Connecting…</span>
</div>
</header>
<div class="app">
<aside class="sidebar" id="sidebar" aria-label="Files">
<div class="pane-head">
<h2>Files</h2>
<button class="icon-btn" id="btnRefresh" type="button" onclick="refreshFileSystem()" title="Refresh" aria-label="Refresh">
<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M13.25 8A5.25 5.25 0 1 1 11.7 4.3"/><path d="M13.25 2.25v3h-3"/></svg>
</button>
<button class="icon-btn" id="btnNewFile" type="button" onclick="showNewInput('file')" title="New file in /" aria-label="New file in /">
<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 1.75H4.25a1 1 0 0 0-1 1v10.5a1 1 0 0 0 1 1h7.5a1 1 0 0 0 1-1V5.5z"/><path d="M9 1.75V5.5h3.75"/><path d="M8 7.75v4.5M5.75 10h4.5"/></svg>
</button>
<button class="icon-btn" id="btnNewFolder" type="button" onclick="showNewInput('folder')" title="New folder in /" aria-label="New folder in /">
<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M1.75 4a1 1 0 0 1 1-1h3.5l1.5 1.5h5.5a1 1 0 0 1 1 1v6.75a1 1 0 0 1-1 1H2.75a1 1 0 0 1-1-1z"/><path d="M8 6.75v4.5M5.75 9h4.5"/></svg>
</button>
</div>
<div class="tree-scroll" id="treeScroll">
<ul class="tree" id="tree" role="tree" aria-label="Files on the board">
<li class="node root open" id="rootNode" role="none" data-path="" data-dir="1" data-depth="0" style="--d:0">
<div class="row item" id="rootRow" role="treeitem" aria-level="1" aria-expanded="true" tabindex="0" data-kind="dir">
<span class="pad" data-kind="dir"></span>
<span class="file-name">/</span>
</div>
<ul id="fileList" role="none"></ul>
</li>
</ul>
</div>
</aside>
<div class="scrim" id="scrim"></div>
<main class="work" id="work">
<section class="editor-section" id="editorSection" aria-label="Editor">
<div class="editor-head">
<div class="tabs" id="tabsContainer" role="tablist" aria-label="Open files"></div>
<div class="editor-actions">
<button class="btn btn-save" id="btnSave" type="button" onclick="saveActiveFile()" title="Save (Ctrl+S)" aria-label="Save" hidden>
<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round" aria-hidden="true"><path d="M2.75 2.75h8.5l2 2v8.5H2.75z"/><path d="M5.25 2.75v3h5v-3M5 13.25v-4h6v4"/></svg>
<span>Save</span>
</button>
<button class="btn btn-run" id="btnExecute" type="button" onclick="executeFile()" title="Run on the board" aria-label="Run" hidden>
<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><path d="M4.5 2.75v10.5L13 8z"/></svg>
<span>Run</span>
</button>
<button class="btn btn-stop" id="btnStop" type="button" onclick="stopFileTask()" title="Stop the running script" aria-label="Stop" hidden>
<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="4" y="4" width="8" height="8" rx="1"/></svg>
<span>Stop</span>
</button>
</div>
</div>
<div class="editor-box is-empty" id="editorBox">
<div class="gutter" id="gutter" aria-hidden="true"></div>
<div class="editor-wrap" id="editorWrap">
<div class="line-hl" id="lineHL"></div>
<div class="bracket-hl" id="bracketHL1"></div>
<div class="bracket-hl" id="bracketHL2"></div>
<pre class="editor-highlight" id="highlightPre" aria-hidden="true"><code id="highlightCode"></code></pre>
<textarea class="editor-textarea" id="cmdInput" spellcheck="false" autocomplete="off" autocapitalize="off" aria-label="File contents"></textarea>
</div>
<div class="empty-state" id="emptyState">
<h3>No file open</h3>
<p>Open a file from the Files list to edit it. Scripts ending in .ti can also be run on the board from here.</p>
<button class="btn" type="button" onclick="showNewInput('file')">New file</button>
<dl class="keys">
<dt><kbd>Ctrl</kbd> <kbd>S</kbd></dt><dd>Save the open file</dd>
<dt><kbd>Ctrl</kbd> <kbd>/</kbd></dt><dd>Comment or uncomment lines</dd>
<dt><kbd>Tab</kbd> <kbd>Shift</kbd> <kbd>Tab</kbd></dt><dd>Indent or outdent</dd>
<dt><kbd>Alt</kbd> <kbd>↑</kbd> <kbd>↓</kbd></dt><dd>Move lines up or down</dd>
<dt><kbd>Shift</kbd> <kbd>Alt</kbd> <kbd>↓</kbd></dt><dd>Duplicate lines</dd>
<dt><kbd>F2</kbd></dt><dd>Rename the selected file</dd>
</dl>
</div>
</div>
<div class="statusline">
<span id="filePathStatus"></span>
<span class="status-right">
<span class="save-note" id="saveNote" aria-live="polite"></span>
<span id="fileTypeStatus"></span>
<span id="editorStatus"></span>
</span>
</div>
</section>
<div class="resizer" id="resizer" role="separator" aria-orientation="horizontal" aria-label="Resize editor and output" tabindex="0"></div>
<section class="output" id="termCard" aria-label="Output">
<div class="output-head">
<h2>Output</h2>
<span class="line-count" id="termCount">0 lines</span>
<button class="text-btn" type="button" onclick="clearTerminal()">Clear</button>
</div>
<div id="terminal" role="log"></div>
</section>
</main>
</div>
<script>
const SERVER_URL = '';
const fetchHeaders = {};
const WS_URL = `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws_tien`;
const CODE_PAD_Y = 12;
const CODE_PAD_X = 16;
const CODE_LH = 22;
const TERMINAL_MAX_LINES = 500;
const INDENT_UNIT = '    '; // 4 spaces, matches the editor's tab-size.
let wsConn;
let openTabs = [];
let activeTab = null;
let fileCache = {};     // path -> editor text, possibly with unsaved edits
let savedContent = {};  // path -> text last read from or written to the board
let tabView = {};       // path -> { top, left, start, end } restored when switching back
const loadingTabs = new Set();
let openFolders = new Set();
let selectedPath = '';  // '' is the root folder
let selectedFolder = '';
let popState = null;    // { el, row, returnFocus } for the open menu or confirmation
let currentLine = 1;
let charWidth = 7.8;
const $ = (id) => document.getElementById(id);
const cmdInput = $('cmdInput');
const highlightPre = $('highlightPre');
const highlightCode = $('highlightCode');
const gutter = $('gutter');
const editorWrap = $('editorWrap');
const editorBox = $('editorBox');
const lineHL = $('lineHL');
const treeEl = $('tree');
const rootNode = $('rootNode');
const rootRow = $('rootRow');
const fileList = $('fileList');
const tabsContainer = $('tabsContainer');
const btnSave = $('btnSave');
const btnExecute = $('btnExecute');
const btnStop = $('btnStop');
const fileTypeStatus = $('fileTypeStatus');
const filePathStatus = $('filePathStatus');
const editorStatus = $('editorStatus');
const saveNote = $('saveNote');
const terminal = $('terminal');
const termCount = $('termCount');
const ICON = {
more: '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><circle cx="3.5" cy="8" r="1.25"/><circle cx="8" cy="8" r="1.25"/><circle cx="12.5" cy="8" r="1.25"/></svg>',
close: '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" aria-hidden="true"><path d="M4 4l8 8M12 4l-8 8"/></svg>'
};
function escapeHtml(str) { return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }
function toApiPath(path) { return path.startsWith('/') ? path : '/' + path; }
function joinPath(dir, name) { const p = dir ? dir + '/' + name : name; return p.startsWith('/') ? p.substring(1) : p; }
function parentOf(path) { const i = path.lastIndexOf('/'); return i < 0 ? '' : path.substring(0, i); }
function baseName(path) { return path.substring(path.lastIndexOf('/') + 1); }
function isUnder(path, root) { return path === root || path.startsWith(root + '/'); }
function isNarrow() { return window.matchMedia('(max-width: 760px)').matches; }
function reduceMotion() { return window.matchMedia('(prefers-reduced-motion: reduce)').matches; }
function formatSize(bytes) {
if (typeof bytes !== 'number' || bytes < 0) return '';
if (bytes < 1024) return bytes + ' B';
if (bytes < 1048576) return (bytes / 1024).toFixed(bytes < 10240 ? 1 : 0) + ' KB';
return (bytes / 1048576).toFixed(1) + ' MB';
}
function updateLineCount() {
const n = terminal.children.length;
termCount.textContent = n === 1 ? '1 line' : n + ' lines';
}
function appendTerminalLine(text, className) {
const stick = terminal.scrollTop + terminal.clientHeight >= terminal.scrollHeight - 24;
const line = document.createElement('div');
line.textContent = text;
if (className) line.className = className;
terminal.appendChild(line);
while (terminal.children.length > TERMINAL_MAX_LINES) terminal.removeChild(terminal.firstChild);
if (stick) terminal.scrollTop = terminal.scrollHeight;
updateLineCount();
}
function clientLog(msg, type = 'info') {
const cls = type === 'error' ? 'log-err' : type === 'success' ? 'log-ok' : 'log-client';
appendTerminalLine('> [Client] ' + msg, cls);
}
function clearTerminal() {
terminal.innerHTML = '';
updateLineCount();
}
async function fetchFolderContents(dirPath) {
try {
const res = await fetch(`${SERVER_URL}/api/fs/list?dir=${encodeURIComponent(toApiPath(dirPath))}`, { headers: fetchHeaders });
if (res.ok) {
const data = await res.json();
return Array.isArray(data.files) ? data.files : [];
}
clientLog(`Couldn't list ${toApiPath(dirPath)}. Status: ${res.status}`, 'error');
} catch (err) {
clientLog(`Couldn't list ${toApiPath(dirPath)}: ${err.message}`, 'error');
}
return null;
}
async function fetchFileContent(path) {
try {
const res = await fetch(`${SERVER_URL}/api/fs/read?path=${encodeURIComponent(toApiPath(path))}`, { headers: fetchHeaders });
if (res.ok) return await res.text();
clientLog(`Couldn't open ${toApiPath(path)}. Status: ${res.status}`, 'error');
} catch (err) {
clientLog(`Couldn't open ${toApiPath(path)}: ${err.message}`, 'error');
}
return null;
}
async function saveFileContent(path, content, verb = 'Saved') {
try {
const res = await fetch(`${SERVER_URL}/api/fs/save`, {
method: 'POST',
headers: { 'Content-Type': 'application/json', ...fetchHeaders },
body: JSON.stringify({ path: toApiPath(path), content: content })
});
if (res.ok) {
clientLog(`${verb} ${toApiPath(path)}`, 'success');
return true;
}
clientLog(`Couldn't save ${toApiPath(path)}. Status: ${res.status}`, 'error');
} catch (err) {
clientLog(`Couldn't save ${toApiPath(path)}: ${err.message}`, 'error');
}
return false;
}
async function postFs(endpoint, params) {
const query = Object.entries(params).map(([k, v]) => `${k}=${encodeURIComponent(v)}`).join('&');
try {
const res = await fetch(`${SERVER_URL}/api/fs/${endpoint}?${query}`, { method: 'POST', headers: fetchHeaders });
return { ok: res.ok, detail: 'Status: ' + res.status };
} catch (err) {
return { ok: false, detail: err.message };
}
}
function updateWsBadge(status) {
const dot = $('wsStatusDot');
const text = $('wsStatusText');
if (status === 'connected') { dot.className = 'led online'; text.textContent = 'Connected'; }
else if (status === 'connecting') { dot.className = 'led connecting'; text.textContent = 'Connecting…'; }
else { dot.className = 'led offline'; text.textContent = 'Offline, retrying'; }
}
function initWS() {
updateWsBadge('connecting');
try {
wsConn = new WebSocket(WS_URL);
} catch (err) {
updateWsBadge('disconnected');
setTimeout(initWS, 2000);
return;
}
wsConn.onopen = () => updateWsBadge('connected');
wsConn.onclose = () => { updateWsBadge('disconnected'); setTimeout(initWS, 2000); };
wsConn.onerror = () => updateWsBadge('disconnected');
wsConn.onmessage = (e) => {
const data = String(e.data);
let cls = '';
if (data.startsWith('> Stop') || data.startsWith('> [Run')) cls = 'log-run';
else if (data.includes('[ERROR]') || data.includes('error') || data.includes('Error')) cls = 'log-err';
else if (data.includes('[INFO]') || data.includes('successfully')) cls = 'log-ok';
appendTerminalLine(data, cls);
};
}
function makeNode(depth, extraClass) {
const li = document.createElement('li');
li.className = 'node' + (extraClass ? ' ' + extraClass : '');
li.style.setProperty('--d', depth);
li.dataset.depth = depth;
li.setAttribute('role', 'none');
return li;
}
function makeInfoNode(text, depth, isError) {
const li = makeNode(depth, 'info');
li.innerHTML = `<div class="row info-row${isError ? ' is-error' : ''}">${escapeHtml(text)}</div>`;
return li;
}
async function renderNodeChildren(files, parentPath, container, depth) {
container.innerHTML = '';
const slot = makeNode(depth, 'slot');
slot.hidden = true;
container.appendChild(slot);
if (files === null) { container.appendChild(makeInfoNode("Couldn't load this folder", depth, true)); return; }
if (files.length === 0) { container.appendChild(makeInfoNode('Empty folder', depth)); return; }
files.sort((a, b) => (a.is_dir === b.is_dir) ? a.name.localeCompare(b.name) : (a.is_dir ? -1 : 1));
const reopen = [];
for (const f of files) {
const path = joinPath(parentPath, f.name);
const kind = f.is_dir ? 'dir' : (f.name.endsWith('.ti') ? 'script' : 'file');
const li = makeNode(depth);
li.dataset.path = path;
li.dataset.name = f.name;
li.dataset.dir = f.is_dir ? '1' : '0';
li.innerHTML =
`<div class="row item" role="treeitem" tabindex="-1" aria-level="${depth + 1}"${f.is_dir ? ' aria-expanded="false"' : ''} data-kind="${kind}">` +
`<span class="pad" data-kind="${kind}"></span>` +
`<span class="file-name">${escapeHtml(f.name)}</span>` +
(f.is_dir ? '' : `<span class="size">${formatSize(f.size)}</span>`) +
`<button class="row-more-btn" type="button" tabindex="-1" title="More actions" aria-label="More actions for ${escapeHtml(f.name)}">${ICON.more}</button>` +
`</div>`;
if (f.is_dir) {
const ul = document.createElement('ul');
ul.setAttribute('role', 'none');
ul.hidden = true;
li.appendChild(ul);
if (openFolders.has(path)) reopen.push(li);
}
container.appendChild(li);
}
updateNet();
for (const li of reopen) {
if (li.isConnected) await expandFolder(li, false);
}
}
async function expandFolder(li, animate = true) {
const ul = li.querySelector(':scope > ul');
if (!ul || li === rootNode) return;
openFolders.add(li.dataset.path);
li.classList.add('open');
li.querySelector(':scope > .row').setAttribute('aria-expanded', 'true');
ul.hidden = false;
if (animate && !reduceMotion()) {
ul.classList.remove('grow');
void ul.offsetWidth; // Restart the reveal animation.
ul.classList.add('grow');
}
if (ul.dataset.loaded) return;
if (!li._loading) {
li._loading = (async () => {
const depth = Number(li.dataset.depth) + 1;
ul.innerHTML = '';
ul.appendChild(makeInfoNode('Loading…', depth));
const children = await fetchFolderContents(li.dataset.path);
if (children) ul.dataset.loaded = 'true';
await renderNodeChildren(children, li.dataset.path, ul, depth);
})().finally(() => { li._loading = null; });
}
await li._loading;
}
function collapseFolder(li) {
const ul = li.querySelector(':scope > ul');
if (!ul || li === rootNode) return;
openFolders.delete(li.dataset.path);
li.classList.remove('open');
li.querySelector(':scope > .row').setAttribute('aria-expanded', 'false');
ul.hidden = true;
}
function toggleFolder(li) {
if (li.classList.contains('open')) collapseFolder(li);
else expandFolder(li);
}
function findNode(path) {
if (!path) return rootNode;
return treeEl.querySelector(`li.node[data-path="${CSS.escape(path)}"]`);
}
function containerFor(dirPath) {
if (!dirPath) return fileList;
const li = findNode(dirPath);
return li ? li.querySelector(':scope > ul') : null;
}
async function refreshContainer(ul) {
if (!ul || !ul.isConnected) return;
const folderLi = ul === fileList ? rootNode : ul.parentElement;
const path = folderLi === rootNode ? '' : folderLi.dataset.path;
const depth = Number(folderLi.dataset.depth) + 1;
const children = await fetchFolderContents(path);
if (!ul.isConnected) return;
if (children) ul.dataset.loaded = 'true';
await renderNodeChildren(children, path, ul, depth);
updateNet();
}
async function fetchFileList() {
const btn = $('btnRefresh');
btn.classList.add('is-busy');
fileList.innerHTML = '';
fileList.appendChild(makeInfoNode('Loading…', 1));
const files = await fetchFolderContents('');
await renderNodeChildren(files, '', fileList, 1);
btn.classList.remove('is-busy');
if (!findNode(selectedPath)) selectPath('', false);
else updateNet();
}
function refreshFileSystem() {
openFolders.clear();
fetchFileList();
}
function setRovingRow(row) {
if (!row) return;
const prev = treeEl.querySelector('.row.item[tabindex="0"]');
if (prev && prev !== row) prev.tabIndex = -1;
row.tabIndex = 0;
}
function focusRow(row) {
if (!row) return;
setRovingRow(row);
row.focus();
row.scrollIntoView({ block: 'nearest' });
}
function updateNewItemTitles() {
const where = '/' + selectedFolder;
for (const [id, label] of [['btnNewFile', 'New file in '], ['btnNewFolder', 'New folder in ']]) {
$(id).title = label + where;
$(id).setAttribute('aria-label', label + where);
}
}
function selectPath(path, isFile) {
selectedPath = path;
selectedFolder = isFile ? parentOf(path) : path;
updateNet();
updateNewItemTitles();
}
function updateNet() {
treeEl.querySelectorAll('.net-pass, .net-end, .net-anc, .net-target')
.forEach((el) => el.classList.remove('net-pass', 'net-end', 'net-anc', 'net-target'));
const target = findNode(selectedPath);
if (!target) return;
target.classList.add('net-target');
let visible = target;
while (visible !== rootNode && visible.querySelector(':scope > .row').offsetParent === null) {
visible = visible.parentElement.closest('li.node');
}
setRovingRow(visible.querySelector(':scope > .row'));
let node = target;
while (node !== rootNode) {
node.classList.add('net-end');
for (let sib = node.previousElementSibling; sib; sib = sib.previousElementSibling) sib.classList.add('net-pass');
node = node.parentElement.closest('li.node');
if (!node) return;
node.classList.add('net-anc');
}
}
function activateRow(row) {
const li = row.parentElement;
if (li === rootNode) { selectPath('', false); return; }
if (li.dataset.dir === '1') {
selectPath(li.dataset.path, false);
toggleFolder(li);
} else {
selectPath(li.dataset.path, true);
openFile(li.dataset.path);
if (isNarrow()) setDrawer(false);
}
}
function visibleRows() {
return Array.from(treeEl.querySelectorAll('.row.item')).filter((r) => r.offsetParent !== null);
}
function onTreeKeydown(e) {
const row = e.target.closest && e.target.closest('.row.item');
if (!row || e.target !== row) return;
const li = row.parentElement;
const isDir = li.dataset.dir === '1';
const isOpen = li.classList.contains('open');
const rows = visibleRows();
const i = rows.indexOf(row);
switch (e.key) {
case 'ArrowDown': focusRow(rows[i + 1]); break;
case 'ArrowUp': if (i > 0) focusRow(rows[i - 1]); break;
case 'Home': focusRow(rows[0]); break;
case 'End': focusRow(rows[rows.length - 1]); break;
case 'ArrowRight':
if (!isDir) return;
if (!isOpen) expandFolder(li);
else focusRow(li.querySelector(':scope > ul > li > .row.item'));
break;
case 'ArrowLeft':
if (isDir && isOpen && li !== rootNode) collapseFolder(li);
else {
const parent = li.parentElement.closest('li.node');
if (parent) focusRow(parent.querySelector(':scope > .row'));
}
break;
case 'Enter':
case ' ':
activateRow(row);
break;
case 'F2': startInlineRename(li); break;
case 'Delete': if (li !== rootNode) confirmDelete(li, row.getBoundingClientRect()); break;
case 'ContextMenu': openRowMenu(row, row.getBoundingClientRect(), 'end', true); break;
case 'F10':
if (!e.shiftKey) return;
openRowMenu(row, row.getBoundingClientRect(), 'end', true);
break;
default: return;
}
e.preventDefault();
}
treeEl.addEventListener('click', (e) => {
const row = e.target.closest('.row.item');
if (!row || e.target.closest('input')) return;
const more = e.target.closest('.row-more-btn');
if (more) {
if (popState && popState.row === row) { closePopover(false); return; }
openRowMenu(row, more.getBoundingClientRect(), 'end', false);
return;
}
activateRow(row);
});
treeEl.addEventListener('contextmenu', (e) => {
const row = e.target.closest('.row.item');
if (!row || e.target.closest('input')) return;
e.preventDefault();
openRowMenu(row, { left: e.clientX, right: e.clientX, top: e.clientY, bottom: e.clientY }, 'start', false);
});
treeEl.addEventListener('keydown', onTreeKeydown);
$('treeScroll').addEventListener('scroll', () => closePopover(false));
function openPopover(el, rect, align, ownerRow) {
const returnFocus = ownerRow || document.activeElement;
document.body.appendChild(el);
const w = el.offsetWidth;
const h = el.offsetHeight;
let left = align === 'end' ? rect.right - w : rect.left;
left = Math.max(8, Math.min(left, window.innerWidth - w - 8));
let top = rect.bottom + 4;
if (top + h > window.innerHeight - 8) top = Math.max(8, rect.top - h - 4);
el.style.left = left + 'px';
el.style.top = top + 'px';
popState = { el, row: ownerRow, returnFocus };
}
function closePopover(restoreFocus) {
if (!popState) return;
const { el, row, returnFocus } = popState;
popState = null;
el.remove();
if (row) row.classList.remove('menu-open');
if (restoreFocus && returnFocus && returnFocus.isConnected) returnFocus.focus();
}
document.addEventListener('pointerdown', (e) => {
if (!popState || popState.el.contains(e.target)) return;
const more = e.target.closest && e.target.closest('.row-more-btn');
if (more && popState.row && popState.row.contains(more)) return;
closePopover(false);
}, true);
function openRowMenu(row, rect, align, fromKeyboard) {
closePopover(false);
const li = row.parentElement;
const path = li.dataset.path;
const isDir = li.dataset.dir === '1';
if (!rect.top && !rect.bottom && !rect.left) rect = row.getBoundingClientRect();
selectPath(path, !isDir);
const items = [];
if (isDir) {
items.push({ label: 'New file', run: () => showNewInput('file') });
items.push({ label: 'New folder', run: () => showNewInput('folder') });
} else if (path.endsWith('.ti')) {
items.push({ label: 'Run', run: () => runScript(path) });
}
if (li !== rootNode) {
if (items.length) items.push(null);
items.push({ label: 'Rename', hint: 'F2', run: () => startInlineRename(li) });
items.push({ label: 'Delete', hint: 'Del', danger: true, run: () => confirmDelete(li, rect) });
}
const menu = document.createElement('div');
menu.className = 'popover menu';
menu.setAttribute('role', 'menu');
for (const item of items) {
if (!item) {
const sep = document.createElement('div');
sep.className = 'menu-sep';
sep.setAttribute('role', 'separator');
menu.appendChild(sep);
continue;
}
const b = document.createElement('button');
b.type = 'button';
b.className = 'menu-item' + (item.danger ? ' danger' : '');
b.setAttribute('role', 'menuitem');
b.tabIndex = -1;
b.innerHTML = `<span>${item.label}</span>${item.hint ? `<span class="hint">${item.hint}</span>` : ''}`;
b.onclick = () => { closePopover(false); item.run(); };
menu.appendChild(b);
}
menu.addEventListener('keydown', (e) => {
const buttons = Array.from(menu.querySelectorAll('.menu-item'));
const i = buttons.indexOf(document.activeElement);
if (e.key === 'ArrowDown') { e.preventDefault(); buttons[(i + 1) % buttons.length].focus(); }
else if (e.key === 'ArrowUp') { e.preventDefault(); buttons[(i - 1 + buttons.length) % buttons.length].focus(); }
else if (e.key === 'Tab') { e.preventDefault(); closePopover(true); }
});
openPopover(menu, rect, align, row);
row.classList.add('menu-open');
if (fromKeyboard) {
const first = menu.querySelector('.menu-item');
if (first) first.focus();
}
}
function showConfirm({ rect, align = 'end', message, confirmLabel, cancelLabel = 'Cancel', onConfirm }) {
const returnTo = popState ? popState.returnFocus : document.activeElement;
closePopover(false);
const pop = document.createElement('div');
pop.className = 'popover confirm';
pop.setAttribute('role', 'alertdialog');
pop.setAttribute('aria-describedby', 'confirmMsg');
pop.innerHTML =
`<p class="confirm-msg" id="confirmMsg">${message}</p>` +
`<div class="confirm-actions"><button class="btn" type="button" data-act="cancel">${cancelLabel}</button>` +
`<button class="btn btn-danger" type="button" data-act="ok">${confirmLabel}</button></div>`;
const cancelBtn = pop.querySelector('[data-act="cancel"]');
const okBtn = pop.querySelector('[data-act="ok"]');
cancelBtn.onclick = () => closePopover(true);
okBtn.onclick = () => { closePopover(true); onConfirm(); };
pop.addEventListener('keydown', (e) => {
if (e.key !== 'Tab') return;
e.preventDefault();
(document.activeElement === okBtn ? cancelBtn : okBtn).focus();
});
openPopover(pop, rect, align, null);
popState.returnFocus = returnTo;
cancelBtn.focus();
}
function validateName(name, ul, self) {
if (!name) return 'Enter a name.';
if (name.includes('/')) return "Names can't contain '/'.";
if (name === '.' || name === '..') return 'Pick a different name.';
const clash = ul && Array.from(ul.children).some((n) => n !== self && n.dataset.name === name);
if (clash) return `${name} already exists in this folder.`;
return null;
}
async function showNewInput(type) {
closePopover(false);
if (isNarrow()) setDrawer(true);
let ul = containerFor(selectedFolder);
if (!ul) { selectPath('', false); ul = fileList; }
const folderLi = ul === fileList ? rootNode : ul.parentElement;
if (folderLi !== rootNode) await expandFolder(folderLi);
const slot = ul.querySelector(':scope > li.slot');
if (!slot) return;
const dirPath = folderLi === rootNode ? '' : folderLi.dataset.path;
const isFolder = type === 'folder';
slot.innerHTML =
`<div class="row slot-row"><span class="pad" data-kind="${isFolder ? 'dir' : 'file'}"></span>` +
`<input class="inline-input" spellcheck="false" autocomplete="off" aria-label="${isFolder ? 'New folder name' : 'New file name'}" placeholder="${isFolder ? 'Folder name' : 'File name, e.g. main.ti'}"></div>`;
slot.hidden = false;
const input = slot.querySelector('input');
input.focus();
slot.scrollIntoView({ block: 'nearest' });
let settled = false;
const cancel = () => {
if (settled) return;
settled = true;
slot.hidden = true;
slot.innerHTML = '';
};
input.addEventListener('input', () => input.classList.remove('input-error'));
input.addEventListener('blur', cancel);
input.addEventListener('keydown', async (e) => {
e.stopPropagation();
if (e.key === 'Escape') {
e.preventDefault();
cancel();
focusRow(folderLi.querySelector(':scope > .row'));
return;
}
if (e.key !== 'Enter') return;
e.preventDefault();
const name = input.value.trim();
if (!name) { cancel(); return; }
const problem = validateName(name, ul, null);
if (problem) {
input.classList.add('input-error');
clientLog(problem, 'error');
return;
}
settled = true;
input.disabled = true;
const newPath = joinPath(dirPath, name);
let ok;
if (isFolder) {
const r = await postFs('mkdir', { path: toApiPath(newPath) });
ok = r.ok;
if (ok) clientLog('Created folder ' + toApiPath(newPath), 'success');
else clientLog(`Couldn't create ${toApiPath(newPath)}. ${r.detail}`, 'error');
} else {
ok = await saveFileContent(newPath, '', 'Created');
}
await refreshContainer(ul);
if (!ok) return;
if (isFolder) {
selectPath(newPath, false);
} else {
fileCache[newPath] = '';
savedContent[newPath] = '';
selectPath(newPath, true);
openFile(newPath);
if (isNarrow()) setDrawer(false);
}
});
}
function startInlineRename(li) {
if (li === rootNode) return;
const row = li.querySelector(':scope > .row');
const nameSpan = row.querySelector('.file-name');
if (!nameSpan) return;
const oldPath = li.dataset.path;
const name = li.dataset.name;
const parentPath = parentOf(oldPath);
const input = document.createElement('input');
input.className = 'inline-input';
input.value = name;
input.spellcheck = false;
input.setAttribute('aria-label', 'New name for ' + name);
nameSpan.replaceWith(input);
input.focus();
const dotIdx = name.lastIndexOf('.');
if (dotIdx > 0) input.setSelectionRange(0, dotIdx); else input.select();
let settled = false;
const finish = async (commit, refocus) => {
if (settled) return;
const newName = input.value.trim();
if (commit && newName !== name) {
const problem = validateName(newName, li.parentElement, li);
if (problem) {
input.classList.add('input-error');
clientLog(problem, 'error');
return;
}
}
settled = true;
if (!commit || newName === name) {
input.replaceWith(nameSpan);
if (refocus) focusRow(row);
return;
}
input.disabled = true;
const newPath = joinPath(parentPath, newName);
const { ok, detail } = await postFs('rename', { path: toApiPath(oldPath), new_path: toApiPath(newPath) });
if (ok) {
clientLog(`Renamed ${toApiPath(oldPath)} to ${toApiPath(newPath)}`, 'success');
remapPaths(oldPath, newPath);
} else {
clientLog(`Couldn't rename ${toApiPath(oldPath)}. ${detail}`, 'error');
}
await refreshContainer(containerFor(parentPath));
const node = findNode(ok ? newPath : oldPath);
if (node) focusRow(node.querySelector(':scope > .row'));
};
input.addEventListener('keydown', (ev) => {
ev.stopPropagation();
if (ev.key === 'Enter') { ev.preventDefault(); finish(true, true); }
else if (ev.key === 'Escape') { ev.preventDefault(); finish(false, true); }
});
input.addEventListener('input', () => input.classList.remove('input-error'));
input.addEventListener('click', (ev) => ev.stopPropagation());
input.addEventListener('blur', () => finish(false, false));
}
function remapPaths(oldPath, newPath) {
stashActive();
const mapPath = (p) => (isUnder(p, oldPath) ? newPath + p.substring(oldPath.length) : p);
const moveKey = (obj, from, to) => { if (from in obj) { obj[to] = obj[from]; delete obj[from]; } };
openTabs.filter((t) => isUnder(t, oldPath) && loadingTabs.has(t)).forEach((t) => closeTab(t, true));
openTabs = openTabs.map((t) => {
const n = mapPath(t);
if (n !== t) { moveKey(fileCache, t, n); moveKey(savedContent, t, n); moveKey(tabView, t, n); }
return n;
});
if (activeTab) activeTab = mapPath(activeTab);
openFolders = new Set(Array.from(openFolders, mapPath));
selectedPath = mapPath(selectedPath);
selectedFolder = mapPath(selectedFolder);
renderTabs();
updateUIForFileType();
updateNewItemTitles();
}
function confirmDelete(li, rect) {
const path = li.dataset.path;
const parentPath = parentOf(path);
showConfirm({
rect,
message: `Delete <strong>${escapeHtml(li.dataset.name)}</strong>? This can't be undone.`,
confirmLabel: 'Delete',
onConfirm: async () => {
const { ok, detail } = await postFs('delete', { path: toApiPath(path) });
if (ok) {
clientLog('Deleted ' + toApiPath(path), 'success');
openTabs.filter((t) => isUnder(t, path)).forEach((t) => closeTab(t, true));
openFolders = new Set(Array.from(openFolders).filter((p) => !isUnder(p, path)));
if (isUnder(selectedPath, path)) selectPath(parentPath, false);
} else {
clientLog(`Couldn't delete ${toApiPath(path)}. ${detail}`, 'error');
}
await refreshContainer(containerFor(parentPath));
}
});
}
function stashActive() {
if (!activeTab || !openTabs.includes(activeTab) || loadingTabs.has(activeTab)) return;
fileCache[activeTab] = cmdInput.value;
tabView[activeTab] = {
top: cmdInput.scrollTop,
left: cmdInput.scrollLeft,
start: cmdInput.selectionStart,
end: cmdInput.selectionEnd
};
}
function isDirty(path) {
if (!openTabs.includes(path) || loadingTabs.has(path) || savedContent[path] === undefined) return false;
const current = path === activeTab ? cmdInput.value : fileCache[path];
return current !== savedContent[path];
}
async function openFile(path) {
if (openTabs.includes(path)) { switchTab(path); return; }
stashActive();
openTabs.push(path);
if (fileCache[path] !== undefined) {
if (savedContent[path] === undefined) savedContent[path] = fileCache[path];
switchTab(path);
return;
}
loadingTabs.add(path);
switchTab(path);
const content = await fetchFileContent(path);
if (!loadingTabs.delete(path)) return; // Tab was closed while loading.
if (content === null) { closeTab(path, true); return; }
fileCache[path] = content;
savedContent[path] = content;
if (activeTab === path) switchTab(path, true);
else renderTabs();
}
function switchTab(path, force = false) {
if (path === activeTab && !force) return;
if (path !== activeTab) stashActive();
activeTab = path;
const view = tabView[path];
if (loadingTabs.has(path)) {
cmdInput.value = 'Loading…';
cmdInput.disabled = true;
} else {
cmdInput.value = fileCache[path] ?? '';
cmdInput.disabled = false;
if (view) cmdInput.setSelectionRange(view.start, view.end);
else cmdInput.setSelectionRange(0, 0);
}
updateUIForFileType();
renderTabs();
renderEditor();
cmdInput.scrollTop = view ? view.top : 0;
cmdInput.scrollLeft = view ? view.left : 0;
syncScroll();
updateCursorPos();
if (selectedPath !== path && findNode(path)) selectPath(path, true);
}
function closeTab(path, force = false, anchorRect = null) {
if (!force && isDirty(path)) {
const tab = tabsContainer.querySelector(`.tab[data-path="${CSS.escape(path)}"]`);
showConfirm({
rect: anchorRect || (tab ? tab.getBoundingClientRect() : tabsContainer.getBoundingClientRect()),
align: 'start',
message: `<strong>${escapeHtml(baseName(path))}</strong> has unsaved changes.`,
confirmLabel: 'Close without saving',
cancelLabel: 'Keep editing',
onConfirm: () => closeTab(path, true)
});
return;
}
const idx = openTabs.indexOf(path);
if (idx < 0) return;
openTabs.splice(idx, 1);
loadingTabs.delete(path);
delete fileCache[path];
delete savedContent[path];
delete tabView[path];
if (activeTab !== path) { renderTabs(); updateDirtyUI(); return; }
activeTab = null;
const next = openTabs[Math.min(idx, openTabs.length - 1)];
if (next) { switchTab(next); return; }
cmdInput.value = '';
cmdInput.disabled = false;
updateUIForFileType();
renderTabs();
renderEditor();
updateCursorPos();
}
function renderTabs() {
tabsContainer.innerHTML = '';
for (const path of openTabs) {
const active = path === activeTab;
const name = baseName(path);
const dir = parentOf(path);
const tab = document.createElement('div');
tab.className = 'tab' + (active ? ' active' : '') + (isDirty(path) ? ' is-dirty' : '');
tab.dataset.path = path;
tab.title = toApiPath(path);
tab.setAttribute('role', 'tab');
tab.setAttribute('aria-selected', active ? 'true' : 'false');
tab.tabIndex = active ? 0 : -1;
tab.innerHTML =
`<span class="pad" data-kind="${path.endsWith('.ti') ? 'script' : 'file'}"></span>` +
`<span class="tab-name">${escapeHtml(name)}</span>` +
(dir ? `<span class="tab-dir">${escapeHtml(dir)}</span>` : '') +
`<button class="tab-close" type="button" tabindex="-1" title="Close" aria-label="Close ${escapeHtml(name)}">${ICON.close}</button>`;
tabsContainer.appendChild(tab);
}
const act = tabsContainer.querySelector('.tab.active');
if (act) act.scrollIntoView({ block: 'nearest', inline: 'nearest' });
}
tabsContainer.addEventListener('click', (e) => {
const tab = e.target.closest('.tab');
if (!tab) return;
if (e.target.closest('.tab-close')) { closeTab(tab.dataset.path, false, tab.getBoundingClientRect()); return; }
switchTab(tab.dataset.path);
});
tabsContainer.addEventListener('mousedown', (e) => { if (e.button === 1) e.preventDefault(); });
tabsContainer.addEventListener('auxclick', (e) => {
const tab = e.target.closest('.tab');
if (e.button === 1 && tab) { e.preventDefault(); closeTab(tab.dataset.path, false, tab.getBoundingClientRect()); }
});
tabsContainer.addEventListener('keydown', (e) => {
const tab = e.target.closest('.tab');
if (!tab) return;
const tabs = Array.from(tabsContainer.querySelectorAll('.tab'));
const i = tabs.indexOf(tab);
let target = null;
if (e.key === 'ArrowRight') target = tabs[(i + 1) % tabs.length];
else if (e.key === 'ArrowLeft') target = tabs[(i - 1 + tabs.length) % tabs.length];
else if (e.key === 'Delete') { e.preventDefault(); closeTab(tab.dataset.path, false, tab.getBoundingClientRect()); return; }
else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); cmdInput.focus(); return; }
if (!target) return;
e.preventDefault();
switchTab(target.dataset.path);
const act = tabsContainer.querySelector('.tab.active');
if (act) act.focus();
});
function updateUIForFileType() {
const has = !!activeTab;
const script = has && activeTab.endsWith('.ti');
editorBox.classList.toggle('is-empty', !has);
filePathStatus.textContent = has ? toApiPath(activeTab) : '';
fileTypeStatus.textContent = has ? (script ? 'TI script' : 'Plain text') : '';
btnSave.hidden = !has;
btnExecute.hidden = !script;
btnStop.hidden = !script;
updateDirtyUI();
}
function updateDirtyUI() {
const dirty = !!activeTab && isDirty(activeTab);
btnSave.classList.toggle('is-dirty', dirty);
btnSave.title = dirty ? 'Save changes (Ctrl+S)' : 'Save (Ctrl+S)';
btnExecute.title = dirty ? 'Save, then run on the board' : 'Run on the board';
tabsContainer.querySelectorAll('.tab').forEach((t) => t.classList.toggle('is-dirty', isDirty(t.dataset.path)));
}
let saveNoteTimer = 0;
let saving = false;
function showSaveNote(text, kind) {
clearTimeout(saveNoteTimer);
saveNote.textContent = text;
saveNote.className = 'save-note' + (kind ? ' ' + kind : '');
if (kind) saveNoteTimer = setTimeout(() => { saveNote.textContent = ''; }, 2400);
}
async function saveFile(path) {
const content = path === activeTab ? cmdInput.value : fileCache[path];
if (content === undefined) return false;
fileCache[path] = content;
const ok = await saveFileContent(path, content);
if (ok && openTabs.includes(path)) savedContent[path] = content;
updateDirtyUI();
return ok;
}
async function saveActiveFile() {
if (!activeTab || loadingTabs.has(activeTab) || saving) return;
saving = true;
btnSave.disabled = true;
showSaveNote('Saving…');
const ok = await saveFile(activeTab);
saving = false;
btnSave.disabled = false;
showSaveNote(ok ? 'Saved' : "Couldn't save", ok ? 'ok' : 'err');
}
async function runScript(path) {
if (!path || !path.endsWith('.ti')) return;
if (isDirty(path)) {
showSaveNote('Saving…');
const ok = await saveFile(path);
showSaveNote(ok ? 'Saved' : "Couldn't save", ok ? 'ok' : 'err');
if (!ok) { clientLog(`Didn't run ${toApiPath(path)} because it couldn't be saved.`, 'error'); return; }
}
const cmd = 'tien ' + toApiPath(path);
try {
const res = await fetch(`${SERVER_URL}/cmd?msg=${encodeURIComponent(cmd)}`, { headers: fetchHeaders });
if (!res.ok) clientLog(`Couldn't run ${toApiPath(path)}. Status: ${res.status}`, 'error');
} catch (err) {
clientLog(`Couldn't run ${toApiPath(path)}: ${err.message}`, 'error');
}
}
function executeFile() {
runScript(activeTab);
}
function stopFileTask() {
if (!activeTab || !activeTab.endsWith('.ti')) return;
if (wsConn && wsConn.readyState === WebSocket.OPEN) {
wsConn.send(JSON.stringify({ action: 'stop', name: toApiPath(activeTab) }));
} else {
clientLog("Couldn't stop the script: the live link to the board is down.", 'error');
}
}
function highlightTI(code) {
const tokenRegex = /(\/\/[^\n]*|\/\*[\s\S]*?\*\/)|("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(\b0x[0-9a-fA-F]+\b|\b\d+(?:\.\d+)?\b)|(\b(?:int|float|string|bool|void|char|struct)\b)|(\b(?:while|for|if|else|return|break|continue|switch|case|default|do)\b)|(\b(?:true|false|null)\b)|(\b[a-zA-Z_]\w*(?=\s*\())/g;
let html = '';
let lastIndex = 0;
let match;
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
const curLine = code.substring(0, cmdInput.selectionStart).split('\n').length;
let nums = '';
for (let i = 1; i <= lineCount; i++) {
nums += `<div class="gutter-line${i === curLine ? ' active' : ''}">${i}</div>`;
}
gutter.innerHTML = nums;
}
function renderEditor() {
if (!activeTab) { highlightCode.innerHTML = ''; updateGutter(); return; }
const code = cmdInput.value;
if (activeTab.endsWith('.ti')) highlightCode.innerHTML = highlightTI(code);
else highlightCode.innerHTML = escapeHtml(code) + (code.endsWith('\n') || code.length === 0 ? ' ' : '');
updateGutter();
syncScroll();
}
function updateLineHighlight() {
if (!activeTab || cmdInput.selectionStart !== cmdInput.selectionEnd) { lineHL.style.display = 'none'; return; }
lineHL.style.top = (CODE_PAD_Y + (currentLine - 1) * CODE_LH - cmdInput.scrollTop) + 'px';
lineHL.style.display = 'block';
}
function updateCursorPos() {
if (!activeTab) {
editorStatus.textContent = '';
updateLineHighlight();
updateBracketHighlight();
return;
}
const code = cmdInput.value;
const linesBefore = code.substring(0, cmdInput.selectionStart).split('\n');
currentLine = linesBefore.length;
const curCol = linesBefore[linesBefore.length - 1].length + 1;
editorStatus.textContent = `Ln ${currentLine}, Col ${curCol}`;
const gutterLines = gutter.children;
for (let i = 0; i < gutterLines.length; i++) gutterLines[i].classList.toggle('active', i + 1 === currentLine);
updateLineHighlight();
updateBracketHighlight();
}
const BRACKET_OPEN_TO_CLOSE = { '(': ')', '[': ']', '{': '}' };
const BRACKET_CLOSE_TO_OPEN = { ')': '(', ']': '[', '}': '{' };
function measureCharWidth() {
const probe = document.createElement('span');
probe.className = 'char-probe';
probe.textContent = '0'.repeat(100);
editorWrap.appendChild(probe);
const w = probe.getBoundingClientRect().width / 100;
probe.remove();
if (w) charWidth = w;
}
function findMatchingBracket(text, pos) {
for (const idx of [pos, pos - 1]) {
const ch = text[idx];
if (ch === undefined) continue;
if (BRACKET_OPEN_TO_CLOSE[ch]) {
const closeCh = BRACKET_OPEN_TO_CLOSE[ch];
let depth = 0;
for (let i = idx; i < text.length; i++) {
if (text[i] === ch) depth++;
else if (text[i] === closeCh && --depth === 0) return { open: idx, close: i };
}
return null;
}
if (BRACKET_CLOSE_TO_OPEN[ch]) {
const openCh = BRACKET_CLOSE_TO_OPEN[ch];
let depth = 0;
for (let i = idx; i >= 0; i--) {
if (text[i] === ch) depth++;
else if (text[i] === openCh && --depth === 0) return { open: i, close: idx };
}
return null;
}
}
return null;
}
function positionBracketMarker(el, text, idx) {
const lines = text.substring(0, idx).split('\n');
const line = lines.length - 1;
const col = lines[lines.length - 1].length;
el.style.top = (CODE_PAD_Y + line * CODE_LH - cmdInput.scrollTop) + 'px';
el.style.left = (CODE_PAD_X + col * charWidth - cmdInput.scrollLeft) + 'px';
el.style.width = charWidth + 'px';
el.style.display = 'block';
}
function updateBracketHighlight() {
const el1 = $('bracketHL1');
const el2 = $('bracketHL2');
const match = (activeTab && cmdInput.selectionStart === cmdInput.selectionEnd)
? findMatchingBracket(cmdInput.value, cmdInput.selectionStart) : null;
if (!match) {
el1.style.display = 'none';
el2.style.display = 'none';
return;
}
positionBracketMarker(el1, cmdInput.value, match.open);
positionBracketMarker(el2, cmdInput.value, match.close);
}
function syncScroll() {
highlightPre.scrollTop = cmdInput.scrollTop;
highlightPre.scrollLeft = cmdInput.scrollLeft;
gutter.scrollTop = cmdInput.scrollTop;
updateLineHighlight();
updateBracketHighlight();
}
cmdInput.addEventListener('input', () => { renderEditor(); updateCursorPos(); updateDirtyUI(); });
cmdInput.addEventListener('scroll', syncScroll);
cmdInput.addEventListener('keyup', updateCursorPos);
cmdInput.addEventListener('click', updateCursorPos);
cmdInput.addEventListener('select', updateCursorPos);
function replaceRange(el, rangeStart, rangeEnd, newText, selStart, selEnd) {
el.focus();
el.setSelectionRange(rangeStart, rangeEnd);
const inserted = document.execCommand && document.execCommand('insertText', false, newText);
if (!inserted) {
el.setRangeText(newText, rangeStart, rangeEnd, 'end');
}
el.setSelectionRange(selStart, selEnd);
renderEditor();
updateCursorPos();
updateDirtyUI();
}
function getLineBounds(text, start, end) {
const lineStart = text.lastIndexOf('\n', start - 1) + 1;
let lineEnd = text.indexOf('\n', end);
if (lineEnd === -1) lineEnd = text.length;
return { lineStart, lineEnd };
}
function indentLines(dedent) {
const el = cmdInput;
const text = el.value;
const start = el.selectionStart, end = el.selectionEnd;
const hadSelection = start !== end;
const { lineStart, lineEnd } = getLineBounds(text, start, end);
const lines = text.substring(lineStart, lineEnd).split('\n');
let firstLineDelta = 0;
const newLines = lines.map((line, idx) => {
if (dedent) {
const m = line.match(/^(\t| {1,4})/);
const removed = m ? m[0].length : 0;
if (idx === 0) firstLineDelta = -removed;
return line.slice(removed);
}
if (idx === 0) firstLineDelta = INDENT_UNIT.length;
return INDENT_UNIT + line;
});
const newBlock = newLines.join('\n');
const caret = Math.max(lineStart, start + firstLineDelta);
const selStart = hadSelection ? lineStart : caret;
const selEnd = hadSelection ? lineStart + newBlock.length : caret;
replaceRange(el, lineStart, lineEnd, newBlock, selStart, selEnd);
}
function toggleLineComment() {
const el = cmdInput;
const text = el.value;
const start = el.selectionStart, end = el.selectionEnd;
const hadSelection = start !== end;
const { lineStart, lineEnd } = getLineBounds(text, start, end);
const lines = text.substring(lineStart, lineEnd).split('\n');
const nonEmpty = lines.filter((l) => l.trim().length > 0);
const allCommented = nonEmpty.length > 0 && nonEmpty.every((l) => l.trim().startsWith('//'));
let firstLineDelta = 0;
const newLines = lines.map((line, idx) => {
if (allCommented) {
const m = line.match(/^(\s*)\/\/ ?/);
if (!m) return line;
if (idx === 0) firstLineDelta = -(m[0].length - m[1].length);
return line.slice(0, m[1].length) + line.slice(m[0].length);
}
const indent = (line.match(/^\s*/) || [''])[0];
if (idx === 0) firstLineDelta = 3; // length of "// "
return indent + '// ' + line.slice(indent.length);
});
const newBlock = newLines.join('\n');
const caret = Math.max(lineStart, start + firstLineDelta);
const selStart = hadSelection ? lineStart : caret;
const selEnd = hadSelection ? lineStart + newBlock.length : caret;
replaceRange(el, lineStart, lineEnd, newBlock, selStart, selEnd);
}
function duplicateLines() {
const el = cmdInput;
const text = el.value;
const { lineStart, lineEnd } = getLineBounds(text, el.selectionStart, el.selectionEnd);
const block = text.substring(lineStart, lineEnd);
replaceRange(el, lineEnd, lineEnd, '\n' + block, lineEnd + 1, lineEnd + 1 + block.length);
}
function moveLines(direction) {
const el = cmdInput;
const text = el.value;
const { lineStart, lineEnd } = getLineBounds(text, el.selectionStart, el.selectionEnd);
const block = text.substring(lineStart, lineEnd);
if (direction < 0) {
if (lineStart === 0) return; // Already the first line.
const prevLineStart = text.lastIndexOf('\n', lineStart - 2) + 1;
const prevLine = text.substring(prevLineStart, lineStart - 1);
const newRegion = block + '\n' + prevLine;
replaceRange(el, prevLineStart, lineEnd, newRegion, prevLineStart, prevLineStart + block.length);
} else {
if (lineEnd === text.length) return; // Already the last line.
let nextLineEnd = text.indexOf('\n', lineEnd + 1);
if (nextLineEnd === -1) nextLineEnd = text.length;
const nextLine = text.substring(lineEnd + 1, nextLineEnd);
const newRegion = nextLine + '\n' + block;
const newSelStart = lineStart + nextLine.length + 1;
replaceRange(el, lineStart, nextLineEnd, newRegion, newSelStart, newSelStart + block.length);
}
}
cmdInput.addEventListener('keydown', function (e) {
if (!activeTab) return;
const start = this.selectionStart;
const end = this.selectionEnd;
const text = this.value;
if (e.key === 'Tab') {
e.preventDefault();
if (e.shiftKey) { indentLines(true); return; }
if (start !== end) { indentLines(false); return; }
replaceRange(this, start, end, INDENT_UNIT, start + INDENT_UNIT.length, start + INDENT_UNIT.length);
return;
}
if (e.key === '/' && (e.ctrlKey || e.metaKey)) {
e.preventDefault();
toggleLineComment();
return;
}
if (e.altKey && e.shiftKey && (e.key === 'ArrowDown' || e.key === 'Down')) {
e.preventDefault();
duplicateLines();
return;
}
if (e.altKey && !e.shiftKey && (e.key === 'ArrowUp' || e.key === 'Up')) {
e.preventDefault();
moveLines(-1);
return;
}
if (e.altKey && !e.shiftKey && (e.key === 'ArrowDown' || e.key === 'Down')) {
e.preventDefault();
moveLines(1);
return;
}
if (e.key === 'Enter') {
e.preventDefault();
const lineStart = text.lastIndexOf('\n', start - 1) + 1;
const currentLineText = text.substring(lineStart, start);
const indent = (currentLineText.match(/^[ \t]*/) || [''])[0];
const charBefore = text[start - 1];
const charAfter = text[start];
let insertText, cursorOffset;
if (charBefore === '{' && charAfter === '}') {
const innerIndent = indent + '  ';
insertText = '\n' + innerIndent + '\n' + indent;
cursorOffset = 1 + innerIndent.length;
} else if (currentLineText.trimEnd().endsWith('{')) {
insertText = '\n' + indent + '  ';
cursorOffset = insertText.length;
} else {
insertText = '\n' + indent;
cursorOffset = insertText.length;
}
replaceRange(this, start, end, insertText, start + cursorOffset, start + cursorOffset);
return;
}
const autoClosePairs = { '{': '}', '(': ')', '[': ']', '"': '"', "'": "'" };
if (autoClosePairs[e.key]) {
e.preventDefault();
const open = e.key;
const close = autoClosePairs[open];
if (start !== end) {
const selected = text.substring(start, end);
replaceRange(this, start, end, open + selected + close, start + 1, start + 1 + selected.length);
} else {
replaceRange(this, start, end, open + close, start + 1, start + 1);
}
}
});
const resizer = $('resizer');
const editorSection = $('editorSection');
const work = $('work');
let isResizing = false;
function setEditorHeight(px) {
const total = work.clientHeight;
const clamped = Math.max(120, Math.min(px, total - resizer.offsetHeight - 80));
editorSection.style.flex = `0 0 ${(clamped / total) * 100}%`;
syncScroll();
}
resizer.addEventListener('pointerdown', (e) => {
isResizing = true;
resizer.setPointerCapture(e.pointerId);
document.body.classList.add('resizing');
e.preventDefault();
});
resizer.addEventListener('pointermove', (e) => {
if (!isResizing) return;
setEditorHeight(e.clientY - work.getBoundingClientRect().top - resizer.offsetHeight / 2);
});
const stopResize = () => {
isResizing = false;
document.body.classList.remove('resizing');
};
resizer.addEventListener('pointerup', stopResize);
resizer.addEventListener('pointercancel', stopResize);
resizer.addEventListener('keydown', (e) => {
if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return;
e.preventDefault();
setEditorHeight(editorSection.offsetHeight + (e.key === 'ArrowUp' ? -24 : 24));
});
function setDrawer(open) {
document.body.classList.toggle('drawer-open', open);
$('drawerToggle').setAttribute('aria-expanded', open ? 'true' : 'false');
}
$('drawerToggle').addEventListener('click', () => {
const open = !document.body.classList.contains('drawer-open');
setDrawer(open);
if (open) (treeEl.querySelector('.row.item[tabindex="0"]') || rootRow).focus();
});
$('scrim').addEventListener('click', () => setDrawer(false));
document.addEventListener('keydown', (e) => {
if ((e.ctrlKey || e.metaKey) && !e.altKey && e.key.toLowerCase() === 's') {
e.preventDefault();
saveActiveFile();
return;
}
if (e.key !== 'Escape') return;
if (popState) { e.preventDefault(); closePopover(true); }
else if (document.body.classList.contains('drawer-open')) { setDrawer(false); $('drawerToggle').focus(); }
});
window.addEventListener('resize', () => { closePopover(false); syncScroll(); });
window.addEventListener('beforeunload', (e) => {
stashActive();
if (openTabs.some(isDirty)) { e.preventDefault(); e.returnValue = ''; }
});
updateNewItemTitles();
updateUIForFileType();
measureCharWidth();
if (document.fonts && document.fonts.ready) {
document.fonts.ready.then(() => { measureCharWidth(); updateCursorPos(); });
}
initWS();
fetchFileList();
</script>
</body>
</html>
)rawliteral";

#endif // MANAGE_FILE_SYSTEM_HTML_H
