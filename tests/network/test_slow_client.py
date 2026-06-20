#!/usr/bin/env python3

import socket
import threading
import time

HOST = "127.0.0.1"
PORT = 8000
CLIENTS = 10

def normal_client(index):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((HOST, PORT))
        request = (
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n\r\n"
        )
        s.sendall(request.encode())
        data = s.recv(4096)

        if not data.startswith(b"HTTP/1.1"):
            print("[FAIL] normal client", index, "bad response")
        else:
            print("[OK] normal client", index)

        s.close()

    except Exception as e:
        print("[FAIL] normal client", index, e)

def slow_client():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(20)
        s.connect((HOST, PORT))
        request = (
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n\r\n"
        )
        print("[INFO] slow client connected")
        for ch in request:
            s.send(ch.encode())
            time.sleep(0.2)

        data = s.recv(4096)
        if data.startswith(b"HTTP/1.1"):
            print("[OK] slow got response")
        else:
            print("[FAIL] slow bad response")
        s.close()

    except Exception as e:
        print("[INFO] slow client ended:", e)

def main():
    slow_thread = threading.Thread(target=slow_client)
    slow_thread.start()

    # Give slow client time to connect and start sending slowly.
    time.sleep(1)

    threads = []
    for i in range(CLIENTS):
        t = threading.Thread(target=normal_client, args=(i,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    slow_thread.join()

    print("done")

if __name__ == "__main__":
    main()