# Webserv — Method Validation Tests & Expected Output
 
These tests verify that each `location` block only allows the HTTP methods declared in its
`methods` directive, and returns `405 Method Not Allowed` for anything else — plus basic
GET / POST / DELETE behavior for locations that support them.
 
Assumes the server is running locally:
```
curl -i http://127.0.0.1:8000/...
```
 
---
 
## 1. `location /` — GET only
 
Config:
```
location / {
    methods GET;
}
```
 
| # | Command | Expected Result |
|---|---------|------------------|
| 1.1 | `curl -i http://127.0.0.1:8000/` | `200 OK` |
| 1.2 | `curl -i -X POST http://127.0.0.1:8000/` | `405 Method Not Allowed` |
| 1.3 | `curl -i -X DELETE http://127.0.0.1:8000/` | `405 Method Not Allowed` |
 
---
 
## 2. `location /upload` — GET + POST
 
Config:
```
location /upload {
    methods GET POST;
}
```
 
| # | Command | Expected Result |
|---|---------|------------------|
| 2.1 | `curl -i http://127.0.0.1:8000/upload` | `200 OK` (index or directory listing, depending on implementation) |
| 2.2 | `curl -i -X POST -F "file=@test.txt" http://127.0.0.1:8000/upload` | Upload succeeds → `201 Created` |
| 2.3 | `curl -i -X DELETE http://127.0.0.1:8000/upload` | `405 Method Not Allowed` |
 
---
 
## 3. `location /uploads` — GET + DELETE
 
Config:
```
location /uploads {
    methods GET DELETE;
    root ./www/site1/uploads;
}
```
 
| # | Command | Expected Result |
|---|---------|------------------|
| 3.1 | `curl -i http://127.0.0.1:8000/uploads` | `200 OK` |
| 3.2 | `curl -i -X DELETE http://127.0.0.1:8000/uploads/test.txt` | If the file exists → deleted, `204 No Content` |
| 3.3 | `curl -i -X POST http://127.0.0.1:8000/uploads` | `405 Method Not Allowed` |
 
---
 
## 4. `location /listing` — GET only
 
Config:
```
location /listing {
    methods GET;
}
```
 
| # | Command | Expected Result |
|---|---------|------------------|
| 4.1 | `curl -i http://127.0.0.1:8000/listing` | `200 OK` |
| 4.2 | `curl -i -X DELETE http://127.0.0.1:8000/listing` | `405 Method Not Allowed` |
 
---
 
## 5. Additional Behavior Tests (from the full flow)
 
### GET — existing file
```
curl -i http://127.0.0.1:8000/index.html
```
Expected:
```
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: <n>
 
<html>...</html>
```
 
### GET — missing file (custom error page)
```
curl -i http://127.0.0.1:8000/doesnotexist
```
Expected:
```
HTTP/1.1 404 Not Found
 
<custom 404.html content>
```
 
### GET — directory with autoindex on
```
curl -i http://127.0.0.1:8000/uploads
```
Expected:
```
HTTP/1.1 200 OK
 
<html>
  <h1>Index of /uploads</h1>
  <a href="test.txt">test.txt</a>
</html>
```
 
### POST — upload
```
curl -i -X POST -F "file=@test.txt" http://127.0.0.1:8000/upload
```
Expected:
```
HTTP/1.1 201 Created
```
And the file should now exist on disk at the configured upload path.
 
### DELETE — existing file
```
curl -i -X DELETE http://127.0.0.1:8000/uploads/test.txt
```
Expected:
```
HTTP/1.1 204 No Content
```
 
### DELETE — file that does not exist
```
curl -i -X DELETE http://127.0.0.1:8000/uploads/ghost.txt
```
Expected:
```
HTTP/1.1 404 Not Found
```
---