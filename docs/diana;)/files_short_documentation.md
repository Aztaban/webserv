## Tuto pisu aby se nestratila protoze delat par veci na jednou a musim se obcas pomoct zorientovat;)

HttpRequest and HttpResponse =
- These two classes are called data structures (DTOs / containers), they don't do anything (only hold data)

## HttpRequest

- we get =

POST /upload HTTP/1.1

Host: localhost

Content-Length: 5

hello

- and we do = 

method = "POST";

target = "/upload";

version = "HTTP/1.1";

headers["Host"] = "localhost";

headers["Content-Length"] = "5";

body = "hello";

## HttpResponse

HTTP/1.1 200 OK

Hello

- as:

statusCode = 200;

reasonPhrase = "OK";

body = "Hello";

---

## In general I do Raw String, HttpRequestParser -> HttpRequest -> HttpRequestHandler -> HttpResponse -> HttpResponseBuilder -> Raw HTTP Response String

# HttpRequestParser

  -> gets
std::string raw =
"POST /upload HTTP/1.1\r\n"
"Host: localhost\r\n"
"Content-Length: 5\r\n"
"\r\n"
"hello";

and it breaks everything apart.


# HttpRequestHandler

  -> HttpRequest -> Business Logic -> HttpResponse
