#!/bin/sh
echo "GET /"
curl -i --http1.1 http://127.0.0.1:8000/

echo "GET static file"
curl -i --http1.1 http://127.0.0.1:8000/static.html

echo "GET autoindex"
curl -i --http1.1 http://127.0.0.1:8000/listing

echo "POST upload"
printf "hello upload\n" > /tmp/webserv_upload_test.txt
curl -i --http1.1 -X POST --data-binary @/tmp/webserv_upload_test.txt http://127.0.0.1:8000/upload

echo "DELETE uploaded/existing file"
curl -i --http1.1 -X DELETE http://127.0.0.1:8000/uploads/keep.txt

echo "CGI GET"
curl -i --http1.1 "http://127.0.0.1:8000/cgi-bin/hello.py?name=webserv"

echo "CGI POST"
curl -i --http1.1 -X POST --data "message=hello" http://127.0.0.1:8000/cgi-bin/hello.py

echo "Redirect"
curl -i --http1.1 http://127.0.0.1:8000/old

echo "413 with small_body_limit.conf"
curl -i --http1.1 -X POST --data "this body is too large" http://127.0.0.1:8000/upload