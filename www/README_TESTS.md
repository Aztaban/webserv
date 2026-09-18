# Webserv Orbit Test Files

This edition is intentionally simple on the browser side: there is **no JavaScript test lab** and no `app.js`.
The pages use normal links plus CSS animation, so the visible demo stays focused on Webserv itself.

## Visual idea

The three supplied team photos are local PNG files. They appear as small circular satellites orbiting a larger Webserv/status circle.
The orbit is pure CSS. Loading the page still makes the browser request the HTML, the CSS and all three PNG files from Webserv.

## Files

### site1
- `index.html` — main route menu + CSS orbit animation.
- `assets/styles.css` — all styling and animation; no JavaScript is required.
- `assets/crew-1.png`, `crew-2.png`, `crew-3.png` — local photo/static-file tests.
- `listing/banana.html` — static HTML/CSS/PNG test.
- `listing/crew.html` — static HTML/CSS/three-PNG test.
- `listing/` — intentionally has no `index.html`, so it can test Webserv autoindex.
- `cgi-bin/hello.py` — CGI GET/POST handler; the generated HTML uses the same orbit visuals.
- `errors/400.html`, `errors/404.html` — animated custom error pages.
- `uploads/` — directory for upload/delete tests performed with curl or another HTTP client.

### site2
- `index.html` — separate visual page for the second server/listen port.
- `errors/404.html` — separate custom error page for site2.
- `assets/` — its own CSS + photo assets.

## Simple browser demo

1. Open site1 `/`.
2. Click **Open banana** to test another static HTML file plus CSS/PNG assets.
3. Click **Open directory** to test server-generated autoindex.
4. Click **Run CGI GET** to execute the Python CGI.
5. Click **Follow /old** to test your configured redirect.
6. Click **Summon 404** to show the custom 404 page.
7. Click **Browse uploads** to inspect the uploads route.
8. Open the second configured port (for example `:8001`) and verify that site2 is different.

POST, DELETE and body-size tests are not hidden in front-end JavaScript anymore. Run those explicitly with curl or your preferred HTTP test client so it is clear which HTTP request you are sending.
