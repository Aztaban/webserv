"""
Network-level tests.

These tests focus on:

- TCP connection handling
- poll() event loop behavior
- client disconnects
- slow clients
- many simultaneous clients
- request body handling

They do not verify routing, CGI, static files, or configuration logic.
"""
