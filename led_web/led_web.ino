#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <sys/time.h>
#include "matrix_logic.h"
#include "web_ui.h"

constexpr uint8_t LED_PIN=8;
constexpr uint16_t MAX_LIMIT_MA=4000;
const char *DEFAULT_SSID="Rocklive_digital_r";
const char *AP_SSID="LED-Panel-Setup";
const char *AP_PASSWORD="pixel1616";
const char *TZ_PRAGUE="CET-1CEST,M3.5.0,M10.5.0/3";
WebServer server(80);
Preferences prefs;
Adafruit_NeoPixel leds(256,LED_PIN,NEO_GRB+NEO_KHZ800);
matrix::Settings settings;
matrix::Stats stats={256,0,0,false};
uint32_t drawing[256]={},framebuffer[256]={},outputFrame[256]={};
String textMessage="AHOJ!",ssid,password;
String pendingSSID,pendingPassword;
bool wifiPending=false,apActive=false,wasConnected=false;
uint32_t connectStarted=0,connectedAt=0,lastRender=0,lastLog=0,textStep=0,lastTextStep=0;

String escaped(const String &s){
 String out;out.reserve(s.length()+8);
 for(size_t i=0;i<s.length();i++){unsigned char c=s[i];
  if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){char b[7];snprintf(b,sizeof(b),"\\u%04x",c);out+=b;}else out+=char(c);
 }return out;
}
String hexColor(uint32_t c){char b[7];snprintf(b,sizeof(b),"%06lx",(unsigned long)(c&0xffffff));return String(b);}
bool parseHex(const String &s,uint32_t &v){
 if(s.length()!=6)return false;v=0;
 for(int i=0;i<6;i++){char c=s[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(n<0)return false;v=(v<<4)|n;}return true;
}
bool integerArg(const char *name,int lo,int hi,int &value){
 String a=server.arg(name);if(a.isEmpty()||a.length()>10)return false;
 for(size_t i=0;i<a.length();i++)if(a[i]<'0'||a[i]>'9')return false;
 long n=a.toInt();if(n<lo||n>hi)return false;value=n;return true;
}
void jsonResponse(int code,const String &data){server.sendHeader("Cache-Control","no-store");server.send(code,"application/json",data);}
void error(const char *message){jsonResponse(400,String("{\"error\":\"")+message+"\"}");}
bool sameOrigin(){String o=server.header("Origin");if(o.length()&&o!=String("http://")+server.hostHeader()){jsonResponse(403,"{\"error\":\"Cizi puvod pozadavku\"}");return false;}return true;}
void startAP(){if(apActive)return;WiFi.mode(WIFI_AP_STA);apActive=WiFi.softAP(AP_SSID,AP_PASSWORD);Serial.printf("SETUP AP: %s http://192.168.4.1\n",AP_SSID);}
void scheduleWiFi(const String &name,const String &pass){pendingSSID=name;pendingPassword=pass;wifiPending=true;}
void networkTick(){
 if(wifiPending){
  wifiPending=false;ssid=pendingSSID;password=pendingPassword;pendingPassword="";
  prefs.putString("ssid",ssid);prefs.putString("pass",password);
  WiFi.disconnect();WiFi.begin(ssid.c_str(),password.c_str());connectStarted=millis();wasConnected=false;
 }
 bool connected=WiFi.status()==WL_CONNECTED;
 if(connected&&!wasConnected){
  connectedAt=millis();configTzTime(TZ_PRAGUE,"pool.ntp.org","time.cloudflare.com");
  MDNS.end();if(MDNS.begin("led-panel"))MDNS.addService("http","tcp",80);
  Serial.printf("CONNECTED http://%s http://led-panel.local\n",WiFi.localIP().toString().c_str());
 }
 if(!connected&&wasConnected)connectStarted=millis();
 wasConnected=connected;
 if(!connected&&millis()-connectStarted>25000)startAP();
 // Leave the setup AP available long enough to display the LAN IP.
 if(connected&&apActive&&millis()-connectedAt>120000){WiFi.softAPdisconnect(true);apActive=false;}
}
void render(){
 memset(framebuffer,0,sizeof(framebuffer));
 if(settings.mode==0)for(int i=0;i<256;i++)framebuffer[i]=settings.color;
 else if(settings.mode==1)memcpy(framebuffer,drawing,sizeof(drawing));
 else if(settings.mode==2){
  if(millis()-lastTextStep>=settings.speed){lastTextStep=millis();textStep++;}
  matrix::textFrame(framebuffer,textMessage.c_str(),textStep,settings.color);
 }else{
  time_t now=time(nullptr);struct tm local;localtime_r(&now,&local);
  matrix::clockFrame(framebuffer,local.tm_hour,local.tm_min,now>1700000000,settings.color);
 }
 stats=matrix::limitFrame(framebuffer,outputFrame,settings);
 for(int y=0;y<16;y++)for(int x=0;x<16;x++)leds.setPixelColor(matrix::physicalIndex(x,y,settings),outputFrame[y*16+x]);
 leds.show();
}
String stateJSON(bool includePixels){
 time_t now=time(nullptr);struct tm local;localtime_r(&now,&local);char clockText[12];strftime(clockText,sizeof(clockText),"%H:%M:%S",&local);
 String s;s.reserve(includePixels?4100:800);
 s="{\"mode\":"+String(settings.mode)+",\"on\":"+String(settings.on?"true":"false")+",\"brightness\":"+String(settings.brightness);
 s+=",\"color\":\""+hexColor(settings.color)+"\",\"text\":\""+escaped(textMessage)+"\",\"speed\":"+String(settings.speed);
 s+=",\"rotation\":"+String(settings.rotation)+",\"serpentine\":"+String(settings.serpentine?"true":"false")+",\"mirror\":"+String(settings.mirror?"true":"false");
 s+=",\"limitMA\":"+String(settings.limitMA)+",\"estimatedMA\":"+String(stats.estimatedMA)+",\"lit\":"+String(stats.lit)+",\"limited\":"+String(stats.limited?"true":"false")+",\"effectiveBrightness\":"+String(stats.effectiveBrightness);
 s+=",\"connected\":"+String(WiFi.status()==WL_CONNECTED?"true":"false")+",\"ssid\":\""+escaped(ssid.length()?ssid:DEFAULT_SSID)+"\",\"ip\":\""+(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():String(""))+"\",\"ap\":"+String(apActive?"true":"false");
 s+=",\"clockReady\":"+String(now>1700000000?"true":"false")+",\"clock\":\""+clockText+"\",\"uptime\":"+String(millis()/1000)+",\"heap\":"+String(ESP.getFreeHeap());
 if(includePixels){s+=",\"drawing\":\"";for(int i=0;i<256;i++)s+=hexColor(drawing[i]);s+="\",\"preview\":\"";for(int i=0;i<256;i++)s+=hexColor(framebuffer[i]);s+='"';}
 s+='}';return s;
}
void updateSettings(){
 if(!sameOrigin())return;
 matrix::Settings next=settings;String nextText=textMessage;int n;
 const char *keys[]={"mode","brightness","rotation","speed","limitMA","on","serpentine","mirror"};
 const int lo[]={0,0,0,35,300,0,0,0},hi[]={3,255,3,400,MAX_LIMIT_MA,1,1,1};
 for(int i=0;i<8;i++)if(server.hasArg(keys[i])){
  if(!integerArg(keys[i],lo[i],hi[i],n)){error("Neplatna hodnota nastaveni");return;}
  switch(i){case 0:next.mode=n;break;case 1:next.brightness=n;break;case 2:next.rotation=n;break;case 3:next.speed=n;break;case 4:next.limitMA=n;break;case 5:next.on=n;break;case 6:next.serpentine=n;break;case 7:next.mirror=n;break;}
 }
 if(server.hasArg("color")&&!parseHex(server.arg("color"),next.color)){error("Neplatna barva");return;}
 if(server.hasArg("text")){
  nextText=server.arg("text");if(nextText.length()>64){error("Text smi mit nejvyse 64 znaku");return;}
  for(size_t i=0;i<nextText.length();i++)if((uint8_t)nextText[i]<32||(uint8_t)nextText[i]>126){error("Pouzijte text bez diakritiky");return;}
  nextText.toUpperCase();
 }
 if(nextText!=textMessage||next.mode!=settings.mode){textStep=0;lastTextStep=millis();}
 settings=next;textMessage=nextText;render();jsonResponse(200,stateJSON(false));
}
void saveScene(){
 if(!sameOrigin())return;
 prefs.putUChar("mode",settings.mode);prefs.putUChar("brightness",settings.brightness);prefs.putUInt("color",settings.color);
 prefs.putUChar("rotation",settings.rotation);prefs.putBool("serpentine",settings.serpentine);prefs.putBool("mirror",settings.mirror);
 prefs.putBool("on",settings.on);prefs.putUShort("speed",settings.speed);prefs.putUShort("limit",settings.limitMA);
 prefs.putString("text",textMessage);prefs.putBytes("drawing",drawing,sizeof(drawing));
 jsonResponse(200,"{\"ok\":true}");
}
void loadScene(){
 settings.mode=prefs.getUChar("mode",0);if(settings.mode>3)settings.mode=0;
 settings.brightness=prefs.getUChar("brightness",16);settings.color=prefs.getUInt("color",0x4DFFAA)&0xffffff;
 settings.rotation=prefs.getUChar("rotation",0)%4;settings.serpentine=prefs.getBool("serpentine",true);settings.mirror=prefs.getBool("mirror",false);
 settings.on=prefs.getBool("on",false);settings.speed=constrain(prefs.getUShort("speed",100),35,400);
 settings.limitMA=constrain(prefs.getUShort("limit",matrix::DEFAULT_LIMIT_MA),300,MAX_LIMIT_MA);
 textMessage=prefs.getString("text","AHOJ!");if(textMessage.length()>64)textMessage="AHOJ!";
 if(prefs.getBytesLength("drawing")==sizeof(drawing))prefs.getBytes("drawing",drawing,sizeof(drawing));
}
void serialTick(){
 static String line;
 while(Serial.available()){
  char c=Serial.read();if(c=='\r')continue;
  if(c=='\n'){
   if(line.startsWith("WIFI\t")){
    int split=line.indexOf('\t',5);String name=line.substring(5,split),pass=line.substring(split+1);
    if(split>=5&&name.length()>0&&name.length()<=32&&pass.length()<=63){scheduleWiFi(name,pass);Serial.println("WIFI ACCEPTED");}else Serial.println("WIFI INVALID");
   }else if(line=="OFF"){settings.on=false;render();}
   else if(line=="STATUS")Serial.println(String("STATUS ")+stateJSON(false));
   line="";
  }else if(line.length()<160)line+=c;else line="";
 }
}
void setup(){
 leds.begin();leds.clear();leds.show();Serial.begin(115200);
 prefs.begin("panel-web",false);loadScene();
 ssid=prefs.getString("ssid","");password=prefs.getString("pass","");
 WiFi.setHostname("led-panel");WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);
 if(ssid.length())WiFi.begin(ssid.c_str(),password.c_str());else startAP();
 connectStarted=millis();setenv("TZ",TZ_PRAGUE,1);tzset();
 const char *headers[]={"Origin"};server.collectHeaders(headers,1);
 server.on("/",HTTP_GET,[]{server.sendHeader("Cache-Control","no-cache");server.send_P(200,"text/html; charset=utf-8",WEB_UI);});
 server.on("/api/state",HTTP_GET,[]{jsonResponse(200,stateJSON(server.arg("pixels")=="1"));});
 server.on("/api/settings",HTTP_POST,updateSettings);
 server.on("/api/draw",HTTP_POST,[]{
  if(!sameOrigin())return;String data=server.arg("plain");
  if(data.length()!=1536){error("Obrazek musi obsahovat 256 RGB pixelu");return;}
  uint32_t candidate[256];for(int i=0;i<256;i++)if(!parseHex(data.substring(i*6,i*6+6),candidate[i])){error("Neplatna barva pixelu");return;}
  memcpy(drawing,candidate,sizeof(drawing));settings.mode=1;render();jsonResponse(200,"{\"ok\":true}");
 });
 server.on("/api/save",HTTP_POST,saveScene);
 server.on("/api/time",HTTP_POST,[]{
  if(!sameOrigin())return;String raw=server.arg("epoch");
  if(raw.length()!=10){error("Neplatny cas");return;}for(size_t i=0;i<raw.length();i++)if(!isDigit(raw[i])){error("Neplatny cas");return;}
  int64_t t=strtoll(raw.c_str(),nullptr,10);if(t<1700000000LL||t>4102444800LL){error("Neplatny cas");return;}
  timeval tv={time_t(t),0};settimeofday(&tv,nullptr);render();jsonResponse(200,"{\"ok\":true}");
 });
 server.on("/api/wifi",HTTP_POST,[]{
  if(!sameOrigin())return;String name=server.arg("ssid"),pass=server.arg("password");
  if(!name.length()||name.length()>32||pass.length()>63){error("Neplatne udaje Wi-Fi");return;}
  scheduleWiFi(name,pass);jsonResponse(200,"{\"ok\":true}");
 });
 server.onNotFound([]{jsonResponse(404,"{\"error\":\"Nenalezeno\"}");});
 server.begin();render();
}
void loop(){
 server.handleClient();serialTick();networkTick();
 if(millis()-lastRender>=40){lastRender=millis();render();}
 if(millis()-lastLog>10000){lastLog=millis();Serial.println(String("STATUS ")+stateJSON(false));}
 delay(2);
}
