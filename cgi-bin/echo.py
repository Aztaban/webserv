#!/usr/bin/env python3
import os
import sys
import urllib.parse

method = os.environ.get("REQUEST_METHOD", "")
query = os.environ.get("QUERY_STRING", "")
length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
body = sys.stdin.read(length) if length > 0 else ""

params = urllib.parse.parse_qs(query)
name = params.get("name", ["world"])[0]

print("Content-Type: text/html")
print()
print("<!doctype html>")
print("<html><body>")
print("<h1>Hello from CGI</h1>")
print("<p>Method: {}</p>".format(method))
print("<p>Name: {}</p>".format(name))
print("<p>Query: {}</p>".format(query))
if body:
    print("<pre>{}</pre>".format(body))
print("</body></html>")
