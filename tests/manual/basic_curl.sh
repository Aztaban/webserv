#!/bin/bash

BASE_URL="http://127.0.0.1:8000"

echo "============ [ GET / ] ============"
curl -i "$BASE_URL/"
echo

echo "============ [ GET /about.html ] ============"
curl -i "$BASE_URL/about.html"
echo

echo "============ [ GET /missing.html ] ============"
curl -i "$BASE_URL/missing.html"
echo

echo "============ [ Unsupported method: PUT / ] ============"
curl -i -X PUT "$BASE_URL/"
echo

echo "============ [ POST /upload ] ============"
curl -i -X POST --data "hello world" "$BASE_URL/upload"
echo

echo "============ [ DELETE / ] ============"
curl -i -X DELETE "$BASE_URL/uploads/test/txt"
echo