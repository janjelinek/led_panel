"""Loopback-only Wi-Fi provisioning over USB. Password never logged or written to disk."""
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs
import json, os, termios, select, threading, time
PORT=8766
DEVICE='/dev/cu.usbmodem21301'
latest={}
lock=threading.Lock()
fd=os.open(DEVICE,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
a=termios.tcgetattr(fd);a[0]=0;a[1]=0;a[2]=termios.CS8|termios.CREAD|termios.CLOCAL;a[3]=0;a[4]=a[5]=termios.B115200;a[6][termios.VMIN]=0;a[6][termios.VTIME]=0
termios.tcsetattr(fd,termios.TCSANOW,a)
def reader():
 buf=b''
 while True:
  if select.select([fd],[],[],1)[0]:
   buf+=os.read(fd,4096)
   while b'\n' in buf:
    line,buf=buf.split(b'\n',1)
    if line.startswith(b'STATUS '):
     try:
      value=json.loads(line[7:])
      with lock: latest.clear();latest.update(value)
     except ValueError:pass
threading.Thread(target=reader,daemon=True).start()
HTML='''<!doctype html><html lang="cs"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Připojení LED panelu</title><style>body{background:#111813;color:#f1f5e9;font:16px system-ui;max-width:440px;margin:70px auto;padding:24px}h1{font-size:30px}p{color:#b1beb0;line-height:1.6}label{display:block;margin:20px 0 8px}input,button{box-sizing:border-box;width:100%;padding:14px;border-radius:12px;border:1px solid #46543f;background:#202b23;color:white;font:inherit}button{margin-top:24px;background:#c7f58c;color:#233119;font-weight:700;cursor:pointer}a{color:#c7f58c}#status{margin-top:24px;line-height:1.6}</style><h1>Připojíme panel k Wi-Fi.</h1><p>Zadejte heslo domácí sítě. Tento formulář běží jen na vašem Macu a údaje předá přes USB přímo do ESP32. Heslo se uloží pouze do panelu.</p><form id="form"><label for="ssid">Wi-Fi (2,4 GHz)</label><input id="ssid" value="Rocklive_digital_r" maxlength="32" required><label for="password">Heslo</label><input id="password" type="password" maxlength="63" autocomplete="current-password"><button id="submit">Připojit panel</button></form><div id="status" role="status">Čekám na zadání hesla.</div><script>const el=id=>document.getElementById(id);el('form').onsubmit=async e=>{e.preventDefault();el('submit').disabled=true;try{const r=await fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({ssid:el('ssid').value,password:el('password').value})});if(!r.ok)throw Error('Připojení se nepodařilo zahájit.');el('password').value='';el('status').textContent='Připojuji panel…';}catch(e){el('status').textContent=e.message;}finally{el('submit').disabled=false;}};setInterval(async()=>{try{const s=await(await fetch('/status')).json();if(s.connected){el('status').textContent='Hotovo. Ovládání panelu: ';const a=document.createElement('a');a.href='http://'+s.ip;a.textContent=a.href;el('status').append(a);}}catch(e){}},1500);</script></html>'''
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*a):pass
 def respond(self,code,body,kind='application/json'):
  raw=body.encode();self.send_response(code);self.send_header('Content-Type',kind);self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
 def do_GET(self):
  if self.headers.get('Host')!=f'127.0.0.1:{PORT}':self.respond(403,'{}');return
  if self.path=='/':self.respond(200,HTML,'text/html; charset=utf-8')
  elif self.path=='/status':
   with lock:data=json.dumps(latest)
   self.respond(200,data)
  else:self.respond(404,'{}')
 def do_POST(self):
  if self.path!='/connect' or self.headers.get('Origin')!=f'http://127.0.0.1:{PORT}':self.respond(403,'{}');return
  size=int(self.headers.get('Content-Length','0'))
  if size>2048:self.respond(413,'{}');return
  values=parse_qs(self.rfile.read(size).decode(),keep_blank_values=True)
  ssid=values.get('ssid',[''])[0];password=values.get('password',[''])[0]
  if not 0<len(ssid.encode())<=32 or len(password.encode())>63 or any(c in ssid+password for c in '\r\n\t'):
   self.respond(400,'{}');return
  os.write(fd,('WIFI\t'+ssid+'\t'+password+'\n').encode());self.respond(200,'{"ok":true}')
os.write(fd,b'STATUS\n')
print(f'Wi-Fi setup http://127.0.0.1:{PORT}',flush=True)
try:ThreadingHTTPServer(('127.0.0.1',PORT),Handler).serve_forever()
finally:os.close(fd)
