"""Local browser QA only; simulated device, never drives physical LEDs."""
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
from pathlib import Path
import json, time
ROOT=Path(__file__).resolve().parent.parent
state=dict(mode=0,on=False,brightness=16,color='4dffaa',text='AHOJ!',speed=100,rotation=0,serpentine=True,mirror=False,limitMA=1000,estimatedMA=256,lit=0,limited=False,effectiveBrightness=16,connected=True,ssid='Rocklive_digital_r',ip='192.168.1.42',ap=False,clockReady=True,clock='19:42:00',uptime=1,heap=150000,drawing='000000'*256,preview='4dffaa'*256)
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*a): pass
 def reply(self,data,code=200,kind='application/json'):
  raw=(json.dumps(data) if kind=='application/json' else data).encode();self.send_response(code);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
 def do_GET(self):
  if self.path.startswith('/api/state'):
   state['preview']=state['drawing'] if state['mode']==1 else state['color']*256
   self.reply(state)
  elif self.path=='/': self.reply((ROOT/'web/index.html').read_text(),kind='text/html; charset=utf-8')
  else:self.reply({},404)
 def do_POST(self):
  body=self.rfile.read(int(self.headers.get('Content-Length',0))).decode()
  if self.path=='/api/draw':
   assert len(body)==1536;state['drawing']=body;state['mode']=1
  elif self.path=='/api/settings':
   for k,v in parse_qs(body,keep_blank_values=True).items():state[k]=v[0] if k in ('text','color') else bool(int(v[0])) if k in ('on','serpentine','mirror') else int(v[0])
  self.reply(state)
print('UI preview http://127.0.0.1:8765',flush=True)
ThreadingHTTPServer(('127.0.0.1',8765),Handler).serve_forever()
