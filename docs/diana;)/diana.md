# Final touches

tasks								Status
HTTP request parsing				✅ Done
Headers/body parsing				✅ Done
Chunked + Content-Length			✅ Done
Response building					✅ Done
Static files						✅ Done
Directory listing					✅ Done
POST uploads						✅ Done
DELETE								✅ Done
Config-driven HTTP handling			🔄 Next
RouteResult structure				🔄 Next
Route matching						🔄 Next
Allowed methods checking			🔄 Next
Redirect responses					🔄 Next


Raw HTTP request
        ↓
Parser
        ↓
Request handling
        ↓
Responses
        ↓
Files / uploads / DELETE
        ↓
Config integration
        ↓
Routing + methods + redirects


Lada:
.conf file
        ↓
Config parser
        ↓
Server/location objects
        ↓
CGI execution


Both:
        ↓
Integration tests
        ↓
Final debugging

so next steps

1. RouteResult structure	Create the structure that carries the matched ServerConfig and LocationConfig	Existing config classes

The purpose of this part is:

- read our config file
- turn it into ServerConfig objects
- later use those objects to drive routing, uploads, index files, errors, redirects, and CGI

2. Route matching	Match URI to the correct location (longest-prefix match)	RouteResult
3. Config-driven HTTP handling	Make HttpRequestHandler use ServerConfig/LocationConfig instead of hardcoded values	Route matching
4. Redirect responses	Return 301/302 with Location header when route requires it	Config-driven handler
5. Allowed methods checking	Return 405 Method Not Allowed when appropriate	Config-driven handler
6. Tests	Unit + integration tests for all of the above	Everything


tests check

curl -i http://127.0.0.1:8000/
curl -i http://127.0.0.1:8000/index.html
curl -i http://127.0.0.1:8000/listing/
curl -i http://127.0.0.1:8000/uploads/
curl -i -X POST --data-binary @test_upload.txt http://127.0.0.1:8000/upload/test_upload.txt
curl -i -X DELETE http://127.0.0.1:8000/uploads/test_upload.txt


Martin checker
python3 tests/runner.py unit