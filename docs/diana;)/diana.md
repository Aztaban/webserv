# Webserv Progress;))

## Week 1 — Core HTTP Output Layer (currentos states;))

| Day | Topic | co mam | co chybi / next step | a co bude "doufam"|
|-----|------|----------------------|------------------------------|-------------------|
| 1 | Parser cleanup + stability | Full HTTP parser (request line, headers, body, validation) | Add reset(), improve edge-case handling, better incomplete detection | Stable parser (~90% done) |
| 2 | HttpResponse redesign | HttpResponse struct + handler usage | Later refactor into stronger response pipeline | Mostly done |
| 3 | Response builder v1 | HttpResponseBuilder fully implemented | Extend flexibility later | DONE |
| 4 | Error response system | error404 / error405 inside handler | Centralize later | Partially done |
| 5 | Static file serving v1 | NOT YET | Read file from disk into response body | GET /file.html works |
| 6 | MIME types system | NOT YET | Map extensions to Content-Type | Browser renders files correctly |
| 7 | Integration test day | Full pipeline test exists | Upgrade with file serving | MVP pipeline done |

---

## Week 2 — Real Server Features

| Day | Topic | coz znamena? | Result |
|-----|------|-------------------------------|--------|
| 8 | Directory listing (autoindex) | Generate HTML listing for folders | `/folder/` works |
| 9 | Path safety | Prevent `../` traversal attacks | Secure file access |
| 10 | POST upload v1 | Save request body to file | Upload works |
| 11 | Upload improvements | Filename handling, overwrite rules | Stable upload system |
| 12 | Chunked transfer intro | Detect Transfer-Encoding | HTTP/1.1 upgrade |
| 13 | Chunked decoding | Rebuild chunked body | Full compliance |
| 14 | Integration + stress test | Multi-client + event loop testing | MVP server stable |

---

## takze zatim

- HTTP parser , si
- HttpRequest model , si
- HttpResponse model , si
- Request handler , si
- Response builder , si
- Full pipeline test , si

so mame a working **mini HTTP engine**

ale mame hardcoded responses
- no filesystem
- no real server behavior yet

Next steps turns it into:

- real file server
- real web behavior
- browser-compatible responses

---

## so budu next step implementovat Static file serving**

GET /index.html → read file from disk → return content

soo implement:

- file reading
- path mapping
- safe filesystem access

