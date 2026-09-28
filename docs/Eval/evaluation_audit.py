#!/usr/bin/env python3
"""Isolated evaluation fixtures and probes. Run from the repository root."""
import concurrent.futures
import http.client
import json
import os
from pathlib import Path
import signal
import shutil
import socket
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]

def setup():
    base = Path(tempfile.mkdtemp(prefix='webserv-evaluation-'))
    for name in ('site/uploads', 'site/listing', 'site/private/nested', 'site/listing/nested', 'other', 'custom-uploads', 'cgi'):
        (base / name).mkdir(parents=True)
    for name, body in {'site/index.html':'PRIMARY INDEX\n', 'site/home.html':'CONFIGURED INDEX\n',
                       'site/empty.html':'', 'site/listing/a.txt':'LISTING FILE\n',
                       'site/private/nested/secret.txt':'PRIVATE LISTING\n', 'site/listing/nested/a.txt':'NESTED FILE\n', 'other/index.html':'SECOND SITE\n',
                       'custom404.html':'CUSTOM ERROR MARKER\n', 'cgi/relative.txt':'RELATIVE OK'}.items():
        (base / name).write_text(body)
    scripts = {
        'echo.py': 'import os,sys\nprint("Content-Type: text/plain\\n")\nprint("method="+os.environ.get("REQUEST_METHOD",""))\nprint("query="+os.environ.get("QUERY_STRING",""))\nprint("type="+os.environ.get("CONTENT_TYPE",""))\nprint(sys.stdin.read(int(os.environ.get("CONTENT_LENGTH","0"))))\n',
        'relative.py': 'print("Content-Type: text/plain\\n")\nprint(open("relative.txt").read())\n',
        'error.py': 'raise RuntimeError("intentional evaluation error")\n',
        'loop.py': 'import time\nwhile True: time.sleep(1)\n',
    }
    # Close every duplicate of stdout's pipe, so only child lifetime remains.
    scripts['close_sleep.py'] = 'import os,time\nfor fd in range(1,256):\n try: os.close(fd)\n except OSError: pass\ntime.sleep(8)\n'
    for name, body in scripts.items():
        (base / 'cgi' / name).write_text(body)
    config = '''server {
 listen 127.0.0.1:18880;
 server_name primary.test;
 root BASE/site;
 index home.html;
 client_max_body_size 64;
 error_page 404 BASE/custom404.html;
 location / { methods GET; root BASE/site; index home.html; autoindex off; }
 location /listing { methods GET; root BASE/site/listing; autoindex on; }
 location /private { methods GET; root BASE/site/private; autoindex off; }
 location /other { methods GET; root BASE/other; index index.html; }
 location /upload { methods GET POST; root BASE/site; upload on; upload_path BASE/custom-uploads; }
 location /disabled { methods POST; root BASE/site; upload off; }
 location /uploads { methods GET DELETE; root BASE/custom-uploads; autoindex on; }
 location /old { methods GET; return 301 /; }
 location /cgi-bin { methods GET POST; root BASE/cgi; cgi_extension .py /usr/bin/python3; }
}
server {
 listen 127.0.0.1:18880;
 server_name alternate.test;
 root BASE/other;
 location / { methods GET; root BASE/other; }
}
server {
 listen 127.0.0.1:18881;
 root BASE/other;
 location / { methods GET; root BASE/other; }
}
server {
 listen 127.0.0.2:18880;
 root BASE/other;
 location / { methods GET; root BASE/other; }
}
'''.replace('BASE', str(base))
    (base/'audit.conf').write_text(config)
    (base/'collision.conf').write_text(config.split('server {')[0] + 'server {' + config.split('server {')[1])
    return base

def request(path='/', method='GET', body=None, host='127.0.0.1', port=18880, headers=None, timeout=3):
    c = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        c.request(method, path, body=body, headers=headers or {})
        r = c.getresponse()
        return r.status, dict(r.getheaders()), r.read().decode(errors='replace')
    finally:
        c.close()

def run(base):
    results = []
    def check(name, fn):
        try:
            ok, detail = fn()
            results.append({'test': name, 'pass': bool(ok), 'detail': detail})
        except Exception as e:
            results.append({'test': name, 'pass': False, 'detail': repr(e)})
        print(json.dumps(results[-1]), flush=True)
    def probe(path, code, marker=None, **kwargs):
        r = request(path, **kwargs)
        return r[0] == code and (marker is None or marker in r[2]), str(r)
    for addr in [('127.0.0.1',18880), ('127.0.0.1',18881), ('127.0.0.2',18880)]:
        with socket.socket() as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(addr)  # refuse to test somebody else's server
    log = open(base/'server.log', 'w')
    p = subprocess.Popen([str(ROOT/'webserv'), str(base/'audit.conf')], cwd=ROOT, stdout=log, stderr=log, start_new_session=True)
    try:
        for _ in range(100):
            try:
                request(); break
            except OSError:
                if p.poll() is not None: raise RuntimeError('server failed to start')
                time.sleep(.05)
        check('configured index home.html', lambda: probe('/', 200, 'CONFIGURED INDEX'))
        check('different route root', lambda: probe('/other/', 200, 'SECOND SITE'))
        check('second port', lambda: probe('/',200,'SECOND SITE',port=18881))
        check('second loopback address', lambda: probe('/',200,'SECOND SITE',host='127.0.0.2'))
        check('virtual host', lambda: probe('/',200,'SECOND SITE',headers={'Host':'alternate.test'}))
        check('configured custom 404', lambda: probe('/missing',404,'CUSTOM ERROR MARKER'))
        check('listing enabled', lambda: probe('/listing/',200,'a.txt'))
        check('listing nested enabled', lambda: probe('/listing/nested/',200,'a.txt'))
        check('listing disabled', lambda: (request('/private/nested/')[0] in (403,404), str(request('/private/nested/'))))
        check('redirect', lambda: (request('/old')[:2][0] == 301 and request('/old')[1].get('Location')=='/', str(request('/old'))))
        check('DELETE forbidden', lambda: probe('/index.html',405,method='DELETE'))
        check('unknown method gets 501', lambda: probe('/',501,method='UNKNOWN'))
        check('upload 64 bytes accepted', lambda: probe('/upload/demo.txt',201,method='POST',body='x'*64))
        check('upload_path honored', lambda: ((base/'custom-uploads/demo.txt').exists(), 'custom-uploads='+str((base/'custom-uploads/demo.txt').exists())+' hardcoded site/uploads='+str((base/'site/uploads/demo.txt').exists())))
        check('retrieve actual upload', lambda: probe('/uploads/demo.txt',200,'x'*64))
        check('delete actual upload', lambda: probe('/uploads/demo.txt',204,method='DELETE'))
        def raw_delete():
            (base/'custom-uploads/raw.txt').write_text('delete me')
            with socket.create_connection(('127.0.0.1',18880),timeout=3) as sock:
                sock.sendall(b'DELETE /uploads/raw.txt HTTP/1.1\r\nHost: primary.test\r\n\r\n')
                raw=b''
                while True:
                    part=sock.recv(4096)
                    if not part: break
                    raw+=part
            head,body=raw.split(b'\r\n\r\n',1)
            return b'204' in head and not body and b'Content-Length:' not in head,repr(raw)
        check('204 has no body or Content-Length',raw_delete)
        check('deleted file gone', lambda: probe('/uploads/demo.txt',404))
        check('65 byte body rejected', lambda: probe('/upload/too-big.txt',413,method='POST',body='x'*65))
        check('upload off honored with POST allowed', lambda: (request('/disabled/off.txt',method='POST',body='disabled')[0] in (403,405), 'file_created='+str((base/'site/uploads/off.txt').exists())))
        check('CGI GET', lambda: probe('/cgi-bin/echo.py?name=Diana',200,'query=name=Diana'))
        check('CGI POST', lambda: probe('/cgi-bin/echo.py',200,'hello=world',method='POST',body='hello=world',headers={'Content-Type':'text/plain'}))
        check('CGI relative file', lambda: probe('/cgi-bin/relative.py',200,'RELATIVE OK'))
        check('CGI error returns 500', lambda: probe('/cgi-bin/error.py',500))
        def collision():
            r = subprocess.run([str(ROOT/'webserv'),str(base/'collision.conf')],cwd=ROOT,capture_output=True,text=True,timeout=3)
            return r.returncode != 0 and request('/empty.html')[0]==200, 'exit='+str(r.returncode)+' '+r.stderr
        check('overlapping process rejected',collision)
        def metrics():
            status = Path('/proc/%s/status'%p.pid).read_text()
            return {'rss_kb':int(status.split('VmRSS:')[1].split()[0]),'fds':len(list(Path('/proc/%s/fd'%p.pid).iterdir()))}
        siege = shutil.which('siege') or '/tmp/webserv-siege-tool/unpacked/usr/bin/siege'
        siege_args = [siege]
        if siege.startswith('/tmp/'):
            siege_args += ['-R', '/tmp/webserv-siege-tool/unpacked/etc/siege/siegerc']
        if Path(siege).exists():
            samples=[]
            for i in range(3):
                before=metrics()
                r=subprocess.run(siege_args+['-b','-c','20','-r','100','http://127.0.0.1:18880/empty.html'],capture_output=True,text=True,timeout=45)
                (base/('siege-%s.log'%i)).write_text(r.stdout+r.stderr)
                time.sleep(.1)
                samples.append({'before':before,'after':metrics(),'exit':r.returncode})
            (base/'stress-metrics.json').write_text(json.dumps(samples,indent=2))
            print('STRESS '+json.dumps(samples),flush=True)
        else:
            print('SKIP: Siege unavailable; install siege for stress coverage.', flush=True)
        def idle_timeout():
            with socket.create_connection(('127.0.0.1',18880)) as s:
                s.settimeout(34)
                s.sendall(b'GET / HTTP/1.1\r\nHost:')
                start=time.monotonic()
                data=s.recv(128)
                return data==b'', 'elapsed=%.1fs data=%r'%(time.monotonic()-start,data)
        # Run the two 30-second timeout probes together.
        def loop_timeout():
            try:
                r=request('/cgi-bin/loop.py',timeout=34)
                return r[0]>=500,str(r)
            except Exception as e:
                return False,'No HTTP error response: '+repr(e)
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as ex:
            a=ex.submit(idle_timeout); b=ex.submit(loop_timeout)
            check('idle partial client released',a.result)
            check('infinite CGI gets HTTP error',b.result)
        def blocking_wait():
            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as ex:
                pending=ex.submit(request,'/cgi-bin/close_sleep.py',timeout=12)
                time.sleep(.5)
                start=time.monotonic()
                try:
                    r=request('/empty.html',timeout=2)
                    detail='static response in %.2fs: %s'%(time.monotonic()-start,r[0]); ok=True
                except Exception as e:
                    ok=False; detail='static request stalled: '+repr(e)
                try: pending.result()
                except Exception: pass
                return ok,detail
        check('CGI closing stdout must not freeze static requests',blocking_wait)
        check('server still alive',lambda: probe('/empty.html',200))
    finally:
        try:
            os.killpg(p.pid,signal.SIGTERM)
        except ProcessLookupError:
            pass
        p.wait(timeout=5)
        log.close()
        (base/'results.json').write_text(json.dumps(results,indent=2))
    print('Artifacts: '+str(base))
    return 0 if all(r['pass'] for r in results) else 1

if __name__=='__main__':
    base=setup()
    if '--setup' in sys.argv:
        print(base)
    else:
        sys.exit(run(base))
