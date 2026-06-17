table of reference 

Week 1 — Core HTTP Output Layer (Parser → Response → File Serving)
Day	Topic	What you will build	Result of the day
1	Parser cleanup + stability	Refactor parser, add reset(), fix edge cases, small test requests	Stable parser ready for production use
2	HttpResponse redesign	Switch to unified HttpResponse → HttpResponseBuilder flow	Clean response object pipeline
3	Response builder v1	Build full HTTP response string from object (headers + body + status line)	Can generate valid HTTP response manually
4	Error response system	Implement 400/404/405/500/505 helper generator	Standard error responses everywhere
5	Static file serving v1	Open file from disk, read content into response body	GET /file.html works
6	MIME types system	Map .html .css .js .png .jpg → Content-Type	Browser renders files correctly
7	Integration test day	Parser → Response → file serving full flow	First end-to-end working server response

Week 2 — Real Server Features (Uploads, directories, robustness)
Day	Topic	What you will build	Result of the day
8	Directory listing (autoindex)	Read folder content, generate HTML listing	GET /folder/ works
9	Path safety + validation	Prevent ../ access, normalize paths	Secure file access
10	POST upload v1	Save request body into file	Upload file to server
11	Upload improvements	Handle overwrite, filename extraction, limits	Stable upload system
12	Chunked transfer intro	Detect Transfer-Encoding: chunked	Basic chunk support
13	Chunked decoding	Reconstruct body from chunks	Full HTTP/1.1 compliance basics
14	Integration + stress test	Full system test with Martin (socket + loop)	Stable MVP server