#!/usr/bin/env python3

import socket
import threading

HOST = "127.0.0.1"
PORT = 8000
CLIENTS = 100

def worker(i):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3)
        s.connect((HOST, PORT))
        request = (
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n\r\n"
        )
        s.sendall(request.encode())
        data = s.recv(4096)
        if not data.startswith(b"HTTP/1.1"):
            print("bad response", i)
        s.close()
    except Exception as e:
        print("failed", i, e)

threads = []

for i in range(CLIENTS):
    t = threading.Thread(target=worker, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

print("done")