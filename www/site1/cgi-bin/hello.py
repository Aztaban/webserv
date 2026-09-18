#!/usr/bin/env python3
import html
import os
import sys
import urllib.parse


def params():
    data = {}
    query = os.environ.get("QUERY_STRING", "")
    data.update({k: v[-1] for k, v in urllib.parse.parse_qs(query).items()})
    if os.environ.get("REQUEST_METHOD", "GET").upper() == "POST":
        try:
            length = int(os.environ.get("CONTENT_LENGTH", "0") or 0)
        except ValueError:
            length = 0
        raw = sys.stdin.read(length) if length else sys.stdin.read()
        if "application/x-www-form-urlencoded" in os.environ.get("CONTENT_TYPE", ""):
            data.update({k: v[-1] for k, v in urllib.parse.parse_qs(raw).items()})
        elif raw:
            data["body_preview"] = raw[:180]
    return data


p = params()
name = html.escape(p.get("name", "webserv explorer"))
method = html.escape(os.environ.get("REQUEST_METHOD", "GET"))
query = html.escape(os.environ.get("QUERY_STRING", ""))
ctype = html.escape(os.environ.get("CONTENT_TYPE", ""))
clength = html.escape(os.environ.get("CONTENT_LENGTH", ""))
script = html.escape(os.environ.get("SCRIPT_NAME", ""))
path_info = html.escape(os.environ.get("PATH_INFO", ""))

print("Content-Type: text/html; charset=utf-8")
print("Status: 200 OK")
print()
print(f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CGI works</title>
<link rel="stylesheet" href="/assets/styles.css">
<style>
.cgi-data{{margin-top:20px;padding:16px 18px;border:1px solid rgba(255,255,255,.1);border-radius:16px;background:rgba(6,10,20,.7);color:#a8b3cf;font:13px/1.65 ui-monospace,monospace;white-space:pre-wrap}}
.cgi-data b{{color:#62e6ff}}
</style>
</head>
<body>
<div class="grid-bg"></div>
<main class="wrap">
<section class="page-hero">
<div>
<span class="eyebrow"><span class="dot"></span> CGI execution success</span>
<h1>Python says<br><span class="gradient">hello, {name}.</span></h1>
<p class="lead">This HTML came from the CGI process, not from a static .html file. Request method: <strong>{method}</strong>.</p>
<div class="cgi-data"><b>QUERY_STRING</b> = {query}\n<b>CONTENT_TYPE</b> = {ctype}\n<b>CONTENT_LENGTH</b> = {clength}\n<b>SCRIPT_NAME</b> = {script}\n<b>PATH_INFO</b> = {path_info}</div>
<div class="page-actions"><a class="btn primary" href="/">Back home</a><a class="btn" href="/listing/crew.html">Static PNG test</a><a class="btn" href="/definitely-not-a-real-space-potato">404 next</a></div>
</div>
<div class="page-orbit">
<div class="orbit-ring-two"></div>
<div class="core-circle"><div class="big-emoji">🐍</div><span class="core-code">CGI 200</span></div>
<div class="satellite" style="--delay:0s"><div class="sat-photo"><img src="/assets/crew-1.png" alt="Crew member 1"></div></div>
<div class="satellite" style="--delay:-5s"><div class="sat-photo"><img src="/assets/crew-2.png" alt="Crew member 2"></div></div>
<div class="satellite" style="--delay:-10s"><div class="sat-photo"><img src="/assets/crew-3.png" alt="Crew member 3"></div></div>
</div>
</section>
</main>
</body>
</html>''')
