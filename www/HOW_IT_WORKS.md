# How the orbit version works

## 1. The server part

When the browser opens `/`, it sends a normal HTTP request such as:

```http
GET / HTTP/1.1
Host: localhost:8000
```

Webserv maps `/` to `site1/index.html` and sends the file back.

The HTML contains:

```html
<link rel="stylesheet" href="/assets/styles.css">
<img src="/assets/crew-1.png">
<img src="/assets/crew-2.png">
<img src="/assets/crew-3.png">
```

Therefore the browser automatically sends separate GET requests for the CSS and the three PNG files.

## 2. The orbit is CSS only

Each photo sits inside a transparent circular container called `.satellite`.

The satellite rotates around the center:

```css
.satellite {
    animation: orbit 15s linear infinite;
}

@keyframes orbit {
    to { transform: rotate(360deg); }
}
```

The photo is placed at the top edge of that rotating circle. As the container rotates, the photo travels around the large center circle.

Three satellites use the same animation but different negative delays:

```html
<div class="satellite" style="--delay:0s">...</div>
<div class="satellite" style="--delay:-5s">...</div>
<div class="satellite" style="--delay:-10s">...</div>
```

That starts them at three different points of the same orbit.

The small portrait also rotates in the opposite direction:

```css
.sat-photo {
    animation: counterOrbit 15s linear infinite;
}

@keyframes counterOrbit {
    to { transform: rotate(-360deg); }
}
```

This cancels the parent rotation, so the faces remain upright while they move around the circle.

## 3. There is no JavaScript test console

The previous `fetch()` request lab was removed completely. There is no `app.js` in this edition.

The cards are ordinary links:

```html
<a href="/listing/banana.html">Open banana</a>
<a href="/listing/">Open directory</a>
<a href="/cgi-bin/hello.py?name=webserv">Run CGI GET</a>
<a href="/old">Follow /old</a>
```

When you click a card, the browser navigates to that URL and Webserv receives a normal request.

## 4. What to say during evaluation

A short explanation:

> The animation is only CSS. Each of the three photos is a normal local PNG served by Webserv. The browser requests the HTML, CSS and PNG files separately. The photos are placed on rotating containers, with different animation delays so they stay spaced around the circle. A reverse rotation keeps the faces upright. There is no JavaScript request simulator anymore; clicking each card simply navigates to another route, so the request being tested comes directly from normal browser navigation.

For POST and DELETE, use curl or another explicit HTTP client. This makes the HTTP method visible and avoids hiding the test inside page JavaScript.
