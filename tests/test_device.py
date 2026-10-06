"""Exercise the real device API; restore its live scene even on failure.
Usage: python3 tests/test_device.py http://DEVICE_IP
"""
import json,sys,time,urllib.request,urllib.parse,urllib.error
base=sys.argv[1].rstrip('/')
def call(path,data=None,raw=False):
 body=None if data is None else data.encode() if raw else urllib.parse.urlencode(data).encode()
 req=urllib.request.Request(base+path,data=body,headers={'Content-Type':'text/plain' if raw else 'application/x-www-form-urlencoded'})
 with urllib.request.urlopen(req,timeout=8) as res:return json.load(res)
def status():return call('/api/state?pixels=1')
def settings(**kw):return call('/api/settings',kw)
def reject(path,data,raw=False):
 try:call(path,data,raw)
 except urllib.error.HTTPError as e:assert e.code==400;return
 raise AssertionError('Invalid request was accepted')
original=status()
try:
 s=settings(mode=0,on=1,brightness=255,color='ffffff',limitMA=1000)
 assert s['lit']==256 and s['limited'] and s['estimatedMA']<=1000
 s=settings(on=0);assert s['lit']==0
 before=status();reject('/api/settings',dict(mode=2,brightness=999));after=status()
 assert before['mode']==after['mode'] and before['brightness']==after['brightness']
 for data in ['ff0000','zzzzzz'*256]:reject('/api/draw',data,True)
 drawing='ff0000'+'000000'*254+'0000ff';call('/api/draw',drawing,True)
 s=settings(on=1,brightness=16);assert s['lit']==2
 assert status()['drawing']==drawing
 s=settings(mode=2,text='AHOJ 16X16',speed=60,color='00ff00');assert s['mode']==2
 time.sleep(.4);assert status()['preview']!='000000'*256
 call('/api/time',{'epoch':int(time.time())});s=settings(mode=3)
 assert s['clockReady'] and s['lit']>0
 assert s['estimatedMA']<=s['limitMA']
 print('PASS: full panel limit, blackout, atomic validation, drawing, scrolling text, clock')
finally:
 settings(on=0)
 call('/api/draw',original['drawing'],True)
 restore={k:int(original[k]) if isinstance(original[k],bool) else original[k] for k in ('mode','on','brightness','color','text','speed','rotation','serpentine','mirror','limitMA')}
 call('/api/settings',restore)
