#!/bin/bash

BASE_URL="http://127.0.0.1:8000"

UPLOAD_DIR="tests/fixtures/www/uploads"
DELETE_TARGET="$UPLOAD_DIR/test.txt"
UPLOAD_SAMPLE="tests/fixtures/upload_sample.txt"

mkdir -p "$UPLOAD_DIR"

printf "This file is used to test DELETE.\n" > "$DELETE_TARGET"
printf "Hello from upload_sample.txt\nThis file is used for POST upload testing.\n" > "$UPLOAD_SAMPLE"

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

echo "============ [ POST /upload text body ] ============"
curl -i -X POST --data "hello world" "$BASE_URL/upload"
echo

echo "============ [ POST /upload file body ] ============"
curl -i -X POST --data-binary "@$UPLOAD_SAMPLE" "$BASE_URL/upload"
echo

echo "============ [ DELETE /uploads/test.txt ] ============"
curl -i -X DELETE "$BASE_URL/uploads/test.txt"
echo