#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <iarduino_RTC.h> 
#include <Arduino_JSON.h>
#include <EEPROM.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "Audio.h"

#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC  26

#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18

Audio audio;
JSONVar SensorsData;
iarduino_RTC watch(RTC_DS1307);
AsyncWebServer server(80);

const PROGMEM String html_head ={"<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>.c {border: 1px solid #333; display: inline-block; padding: 5px 15px; text-decoration: none; color: #000;} .c:hover {box-shadow: 0 0 5px rgba(0,0,0,0.3); background: linear-gradient(to bottom, #fcfff4, #e9e9ce); color: #a00;}</style></head><body><center>"};
const char* PARAM_MESSAGE = "dateTime";

// Смещения профилей в EEPROM (индекс профиля -> смещение)
const uint16_t PROFILE_OFFSETS[8] = {0, 0, 49, 98, 147, 196, 245, 294};
// Индекс:     0    1    2   3   4    5    6    7
//                   вс  пн  вт  ср   чт   пт   сб

bool flag_fire   = false;
bool flag_terror = false;
bool fire_flag   = false;
bool terror_flag = false;

bool s_1_f = true;
bool e_1_f = true;
bool s_2_f = true;
bool e_2_f = true;
bool s_3_f = true;
bool e_3_f = true;
bool s_4_f = true;
bool e_4_f = true;
bool s_5_f = true;
bool e_5_f = true;
bool s_6_f = true;
bool e_6_f = true;
bool s_7_f = true;
bool e_7_f = true;
bool s_8_f = true;
bool e_8_f = true;
bool s_9_f = true;
bool e_9_f = true;
bool s_10_f = true;
bool e_10_f = true;
bool s_11_f = true;
bool e_11_f = true;
bool s_12_f = true;
bool e_12_f = true;

uint8_t relay = 15;
uint8_t terror = 32;
uint8_t fire = 33;
uint8_t Led_1 = 2;
uint8_t Led_2 = 4;

uint16_t cur_time_year;
uint8_t cur_time_month;
uint8_t cur_time_day; 
uint8_t cur_time_h;
uint8_t cur_time_m;

uint8_t startPoint;

bool music_on_break = true;
bool music_mode_random = true;
bool music_playing = false;
int music_current_idx = 0;
String music_files[100];
int music_files_count = 0;

File uploadFile;
bool uploadInProgress = false;
String uploadPath;

const char *ssid = "Smart_ring";
const char *password = "SR_00110011";

IPAddress local_ip(192,168,2,1);
IPAddress gateway(192,168,2,1);
IPAddress subnet(255,255,255,0);

const char upload_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Загрузка файлов</title><style>body{font-family:Arial;padding:20px;background:#f0f0f0}.container{max-width:500px;margin:0 auto;background:white;border-radius:10px;padding:20px}.btn{display:block;background:#4CAF50;color:white;text-align:center;padding:12px;text-decoration:none;border-radius:5px;margin:10px 0;border:none;width:100%;cursor:pointer}.btn-gray{background:#666}.progress-container{width:100%;background:#ddd;border-radius:5px;margin:10px 0}.progress-bar{width:0%;height:30px;background:#4CAF50;border-radius:5px;text-align:center;line-height:30px;color:white;transition:width 0.3s}.status{padding:10px;margin:10px 0;border-radius:5px;font-size:14px}.status.success{background:#d4edda;color:#155724}.status.error{background:#f8d7da;color:#721c24}.status.info{background:#d1ecf1;color:#0c5462}.file-list{margin:10px 0;padding:10px;background:#f9f9f9;border-radius:5px;max-height:200px;overflow-y:auto}.file-item{display:flex;justify-content:space-between;padding:5px;border-bottom:1px solid #ddd}.delete-btn{background:#f44336;color:white;border:none;padding:5px 10px;border-radius:3px;cursor:pointer}.delete-btn:hover{background:#d32f2f}</style></head><body><div class='container'><h1>📁 Управление файлами</h1><div class='file-list'><b>📋 Файлы в /music/:</b><br><span id='fileList'>Загрузка...</span></div><hr><h3>📤 Загрузить файлы</h3><form id='uploadForm' enctype='multipart/form-data'><input type='file' id='fileInput' name='file'><button type='submit' class='btn'>📤 Загрузить</button></form><div class='progress-container'><div class='progress-bar' id='progressBar'>0%</div></div><div id='status'></div><a href='/' class='btn btn-gray'>🏠 На главную</a></div><script>const fileInput=document.getElementById('fileInput');const uploadForm=document.getElementById('uploadForm');const progressBar=document.getElementById('progressBar');const statusDiv=document.getElementById('status');const fileListSpan=document.getElementById('fileList');async function loadFileList(){try{const response=await fetch('/list');if(response.ok){const data=await response.text();const files=data.split(',').filter(f=>f.trim().length>0);if(files.length===0){fileListSpan.innerHTML='<i>нет файлов</i>';}else{let html='';for(let i=0;i<files.length;i++){html+=`<div class='file-item'><span>📄 ${files[i]}</span><button class='delete-btn' onclick='deleteFile(\"${files[i]}\")'>🗑️</button></div>`;}fileListSpan.innerHTML=html;}}else{fileListSpan.innerHTML='<i>ошибка загрузки</i>';}}catch(error){fileListSpan.innerHTML='<i>ошибка</i>';}}async function deleteFile(filename){if(!confirm(`Удалить файл "${filename}"?`))return;try{const response=await fetch(`/delete?file=${encodeURIComponent(filename)}`);if(response.ok){showStatus(`✅ Файл ${filename} удалён`,'success');loadFileList();}else{showStatus(`❌ Ошибка удаления ${filename}`,'error');}}catch(error){showStatus(`❌ Ошибка: ${error.message}`,'error');}}function showStatus(message,type){const statusEl=document.createElement('div');statusEl.className='status '+type;statusEl.innerHTML=message;statusDiv.appendChild(statusEl);setTimeout(()=>statusEl.remove(),3000);}uploadForm.onsubmit=async function(e){e.preventDefault();const file=fileInput.files[0];if(!file){showStatus('Выберите файл','error');return;}progressBar.style.width='0%';progressBar.innerHTML='0%';statusDiv.innerHTML='';let percent=0;const interval=setInterval(()=>{if(percent<90){percent+=10;progressBar.style.width=percent+'%';progressBar.innerHTML=percent+'%';}},100);const formData=new FormData();formData.append('file',file);try{const response=await fetch('/upload',{method:'POST',body:formData});clearInterval(interval);if(response.ok){progressBar.style.width='100%';progressBar.innerHTML='100%';showStatus(`✅ ${file.name} - загружен`,'success');loadFileList();}else{showStatus(`❌ ${file.name} - ошибка загрузки`,'error');}}catch(error){clearInterval(interval);showStatus(`❌ Ошибка: ${error.message}`,'error');}setTimeout(()=>{progressBar.style.width='0%';progressBar.innerHTML='0%';},2000);fileInput.value='';};loadFileList();</script></body></html>
)rawliteral";

void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
    Serial.println("SPIFFS mounted successfully");
  }
}

uint8_t getStartPoint(){
  watch.gettime();
  
  // watch.weekday: 1=пн, 2=вт, 3=ср, 4=чт, 5=пт, 6=сб, 7=вс
  // Преобразуем: пн=1, вт=2, ср=3, чт=4, пт=5, сб=6, вс=0
  startPoint = watch.weekday;
  if (startPoint == 7) startPoint = 0;
  
  return startPoint;
}

void getEeepromMap(){
  Serial.println("EEPROM dump timers data:");
  for(uint16_t i = 0; i < 350; i++){
    Serial.print(EEPROM.read(i));
    Serial.print("  ");
    if(i%49 == 0 && i != 0){
      Serial.println("");
      delay(1);
    }
  }  
}

String GetData(){
  String tmp;
  SensorsData["time"] = watch.gettime("H:i:s Y-m-d");
  SensorsData["pointer"] = startPoint;
  
  uint16_t offset = PROFILE_OFFSETS[startPoint];
  
  //1
  if(EEPROM.read(offset+1) < 10){
    tmp = "0"+String(EEPROM.read(offset+1));
  }else{
    tmp = String(EEPROM.read(offset+1));
  }
  
  if(EEPROM.read(offset+2) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+2));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+2));
  }
  SensorsData["start1"] = tmp;
  tmp = "";
  
  if(EEPROM.read(offset+3) < 10){
    tmp = "0"+String(EEPROM.read(offset+3));
  }else{
    tmp = String(EEPROM.read(offset+3));
  }
  
  if(EEPROM.read(offset+4) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+4));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+4));
  }
  SensorsData["stop1"] = tmp;
  tmp = "";

  //2
  if(EEPROM.read(offset+5) < 10){
    tmp = "0"+String(EEPROM.read(offset+5));
  }else{
    tmp = String(EEPROM.read(offset+5));
  }
  
  if(EEPROM.read(offset+6) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+6));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+6));
  }
  SensorsData["start2"] = tmp;
  tmp="";

  if(EEPROM.read(offset+7) < 10){
    tmp = "0"+String(EEPROM.read(offset+7));
  }else{
    tmp = String(EEPROM.read(offset+7));
  }
  
  if(EEPROM.read(offset+8) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+8));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+8));
  }
  SensorsData["stop2"] = tmp;
  tmp = "";

  //3
  if(EEPROM.read(offset+9) < 10){
    tmp = "0"+String(EEPROM.read(offset+9));
  }else{
    tmp = String(EEPROM.read(offset+9));
  }
  
  if(EEPROM.read(offset+10) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+10));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+10));
  }
  SensorsData["start3"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+11) < 10){
    tmp = "0"+String(EEPROM.read(offset+11));
  }else{
    tmp = String(EEPROM.read(offset+11));
  }
  
  if(EEPROM.read(offset+12) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+12));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+12));
  }
  SensorsData["stop3"] = tmp;
  tmp = "";

  //4
  if(EEPROM.read(offset+13) < 10){
    tmp = "0"+String(EEPROM.read(offset+13));
  }else{
    tmp = String(EEPROM.read(offset+13));
  }
  
  if(EEPROM.read(offset+14) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+14));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+14));
  }
  SensorsData["start4"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+15) < 10){
    tmp = "0"+String(EEPROM.read(offset+15));
  }else{
    tmp = String(EEPROM.read(offset+15));
  }
  
  if(EEPROM.read(offset+16) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+16));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+16));
  }
  SensorsData["stop4"] = tmp;
  tmp = "";

  //5
  if(EEPROM.read(offset+17) < 10){
    tmp = "0"+String(EEPROM.read(offset+17));
  }else{
    tmp = String(EEPROM.read(offset+17));
  }
  
  if(EEPROM.read(offset+18) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+18));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+18));
  }
  SensorsData["start5"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+19) < 10){
    tmp = "0"+String(EEPROM.read(offset+19));
  }else{
    tmp = String(EEPROM.read(offset+19));
  }
  
  if(EEPROM.read(offset+20) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+20));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+20));
  }
  SensorsData["stop5"] = tmp;
  tmp = "";

  //6
  if(EEPROM.read(offset+21) < 10){
    tmp = "0"+String(EEPROM.read(offset+21));
  }else{
    tmp = String(EEPROM.read(offset+21));
  }
  
  if(EEPROM.read(offset+22) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+22));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+22));
  }
  SensorsData["start6"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+23) < 10){
    tmp = "0"+String(EEPROM.read(offset+23));
  }else{
    tmp = String(EEPROM.read(offset+23));
  }
  
  if(EEPROM.read(offset+24) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+24));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+24));
  }
  SensorsData["stop6"] = tmp;
  tmp = "";

  //7
  if(EEPROM.read(offset+25) < 10){
    tmp = "0"+String(EEPROM.read(offset+25));
  }else{
    tmp = String(EEPROM.read(offset+25));
  }
  
  if(EEPROM.read(offset+26) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+26));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+26));
  }
  SensorsData["start7"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+27) < 10){
    tmp = "0"+String(EEPROM.read(offset+27));
  }else{
    tmp = String(EEPROM.read(offset+27));
  }
  
  if(EEPROM.read(offset+28) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+28));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+28));
  }
  SensorsData["stop7"] = tmp;
  tmp = "";

  //8
  if(EEPROM.read(offset+29) < 10){
    tmp = "0"+String(EEPROM.read(offset+29));
  }else{
    tmp = String(EEPROM.read(offset+29));
  }
  
  if(EEPROM.read(offset+30) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+30));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+30));
  }
  SensorsData["start8"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+31) < 10){
    tmp = "0"+String(EEPROM.read(offset+31));
  }else{
    tmp = String(EEPROM.read(offset+31));
  }
  
  if(EEPROM.read(offset+32) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+32));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+32));
  }
  SensorsData["stop8"] = tmp;
  tmp = "";

  //9
  if(EEPROM.read(offset+33) < 10){
    tmp = "0"+String(EEPROM.read(offset+33));
  }else{
    tmp = String(EEPROM.read(offset+33));
  }
  
  if(EEPROM.read(offset+34) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+34));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+34));
  }
  SensorsData["start9"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+35) < 10){
    tmp = "0"+String(EEPROM.read(offset+35));
  }else{
    tmp = String(EEPROM.read(offset+35));
  }
  
  if(EEPROM.read(offset+36) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+36));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+36));
  }
  SensorsData["stop9"] = tmp;
  tmp = "";

  //10
  if(EEPROM.read(offset+37) < 10){
    tmp = "0"+String(EEPROM.read(offset+37));
  }else{
    tmp = String(EEPROM.read(offset+37));
  }
  
  if(EEPROM.read(offset+38) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+38));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+38));
  }
  SensorsData["start10"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+39) < 10){
    tmp = "0"+String(EEPROM.read(offset+39));
  }else{
    tmp = String(EEPROM.read(offset+39));
  }
  
  if(EEPROM.read(offset+40) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+40));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+40));
  }
  SensorsData["stop10"] = tmp;
  tmp = "";

  //11
  if(EEPROM.read(offset+41) < 10){
    tmp = "0"+String(EEPROM.read(offset+41));
  }else{
    tmp = String(EEPROM.read(offset+41));
  }
  
  if(EEPROM.read(offset+42) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+42));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+42));
  }
  SensorsData["start11"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+43) < 10){
    tmp = "0"+String(EEPROM.read(offset+43));
  }else{
    tmp = String(EEPROM.read(offset+43));
  }
  
  if(EEPROM.read(offset+44) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+44));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+44));
  }
  SensorsData["stop11"] = tmp;
  tmp = "";

  //12
  if(EEPROM.read(offset+45) < 10){
    tmp = "0"+String(EEPROM.read(offset+45));
  }else{
    tmp = String(EEPROM.read(offset+45));
  }
  
  if(EEPROM.read(offset+46) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+46));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+46));
  }
  SensorsData["start12"] = tmp;
  tmp = "";

  if(EEPROM.read(offset+47) < 10){
    tmp = "0"+String(EEPROM.read(offset+47));
  }else{
    tmp = String(EEPROM.read(offset+47));
  }
  
  if(EEPROM.read(offset+48) < 10){
    tmp += ":0"+ String(EEPROM.read(offset+48));
  }else{
    tmp += ":"+ String(EEPROM.read(offset+48));
  }
  SensorsData["stop12"] = tmp;
  tmp = "";
  
  tmp = JSON.stringify(SensorsData);
  Serial.println(tmp);
  return tmp;
}

void loadMusicList() {
  music_files_count = 0;
  File musicDir = SD.open("/music");
  if (!musicDir) {
    Serial.println("No /music folder");
    return;
  }
  
  File file = musicDir.openNextFile();
  while (file && music_files_count < 100) {
    String name = file.name();
    if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
      music_files[music_files_count] = name;
      music_files_count++;
      Serial.println("Found music: " + name);
    }
    file = musicDir.openNextFile();
  }
  musicDir.close();
  Serial.print("Music files count: ");
  Serial.println(music_files_count);
}

void startMusic() {
  Serial.println("=== startMusic called ===");
  Serial.print("music_on_break: "); Serial.println(music_on_break);
  Serial.print("music_files_count: "); Serial.println(music_files_count);
  Serial.print("music_playing: "); Serial.println(music_playing);
  
  if (!music_on_break) {
    Serial.println("Music is disabled");
    return;
  }
  if (music_files_count == 0) {
    Serial.println("No music files");
    return;
  }
  if (music_playing) {
    Serial.println("Music already playing");
    return;
  }
  
  Serial.println("Starting break music");
  music_playing = true;
  
  if (music_mode_random) {
    music_current_idx = random(music_files_count);
  } else {
    music_current_idx = 0;
  }
  
  String path = "/music/" + music_files[music_current_idx];
  Serial.print("Playing: ");
  Serial.println(path);
  audio.stopSong();
  delay(100);
  audio.connecttoFS(SD, path.c_str());
}

void stopMusic() {
  if (!music_playing) return;
  
  Serial.println("Stopping break music");
  audio.stopSong();
  music_playing = false;
}

void playNextTrack() {
  if (!music_on_break) {
    music_playing = false;
    return;
  }
  
  if (music_mode_random) {
    music_current_idx = random(music_files_count);
  } else {
    music_current_idx++;
    if (music_current_idx >= music_files_count) {
      music_current_idx = 0;
    }
  }
  
  String path = "/music/" + music_files[music_current_idx];
  Serial.print("Playing next: ");
  Serial.println(path);
  audio.stopSong();
  delay(100);
  audio.connecttoFS(SD, path.c_str());
}

void setup(){
  Serial.begin(115200);
  watch.begin();
  
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(1000000);
  SD.begin(SD_CS);

  randomSeed(analogRead(0));
  loadMusicList();

  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);
  pinMode(Led_1, OUTPUT);
  digitalWrite(Led_1, HIGH);
  pinMode(Led_2, OUTPUT);
  digitalWrite(Led_2, HIGH);  

  delay(1000);
  digitalWrite(relay, LOW);
  delay(1000);
  digitalWrite(Led_1, LOW);
  delay(1000);
  digitalWrite(Led_2, LOW);
  
  pinMode(fire, INPUT_PULLUP);
  pinMode(terror, INPUT_PULLUP);
  
  initSPIFFS();
  EEPROM.begin(350);

  getEeepromMap();
  Serial.println(getStartPoint());
  
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.on("/SetTimeWeb", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/setTime.html", "text/html");
  });
  
  server.on("/SetTimersWeb", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimers.html", "text/html");
  });

  server.on("/fire", HTTP_GET, [](AsyncWebServerRequest *request){
    flag_fire = true;
    request->send(200, "text/html", html_head+"<h1>Пожарная тревога активирована!</h1></center></body>");
  });
  
  server.on("/terr", HTTP_GET, [](AsyncWebServerRequest *request){
    flag_terror = true;
    request->send(200, "text/html", html_head+"<h1>Террористическая тревога активирована!</h1></center></body>");
  });
  
  server.on("/SetTimer1", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimer1.html", "text/html");
  });
  server.on("/SetTimer2", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimer2.html", "text/html");
  });
  server.on("/SetTimer3", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimer3.html", "text/html");
  });
  server.on("/SetTimer4", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimer4.html", "text/html");
  });
  server.on("/SetTimer5", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimer5.html", "text/html");
  });
  server.on("/SetTimer6", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimer6.html", "text/html");
  });
  server.on("/SetTimer7", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/SetTimer7.html", "text/html");
  });

  server.on("/get_data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = GetData();
    request->send(200, "application/json", json);
    Serial.println(json);
    json = String();
  });    

server.on("/get_profile", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("num")) {
        request->send(400, "application/json", "{}");
        return;
    }
    
    int profileNum = request->getParam("num")->value().toInt();
    if (profileNum < 1 || profileNum > 7) profileNum = 1;
    
    uint16_t offset = PROFILE_OFFSETS[profileNum];
    
    String json = "{";
    for (int i = 1; i <= 12; i++) {
        int addr = offset + (i-1)*4 + 1;
        int sh = EEPROM.read(addr);
        int sm = EEPROM.read(addr+1);
        int th = EEPROM.read(addr+2);
        int tm = EEPROM.read(addr+3);
        
        // EEPROM по умолчанию 255 -> заменяем на 0
        if (sh == 255) sh = 0;
        if (sm == 255) sm = 0;
        if (th == 255) th = 0;
        if (tm == 255) tm = 0;
        
        // Форматирование времени
        String startTime = String(sh) + ":" + String(sm);
        String stopTime = String(th) + ":" + String(tm);
        
        if (sh < 10) startTime = "0" + startTime;
        if (sm < 10) startTime = startTime.substring(0, 3) + "0" + String(sm);
        if (th < 10) stopTime = "0" + stopTime;
        if (tm < 10) stopTime = stopTime.substring(0, 3) + "0" + String(tm);
        
        json += "\"start" + String(i) + "\":\"" + startTime + "\",";
        json += "\"stop" + String(i) + "\":\"" + stopTime + "\"";
        if (i < 12) json += ",";
    }
    json += "}";
    
    request->send(200, "application/json", json);
});
  
  server.on("/SetTime", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("dateTime") && request->hasParam("weekday")) {
        String datetime = request->getParam("dateTime")->value();
        int weekday_ = request->getParam("weekday")->value().toInt();
        
        int year_ = datetime.substring(0, 4).toInt();
        int month_ = datetime.substring(5, 7).toInt();
        int day_ = datetime.substring(8, 10).toInt();
        int hour_ = datetime.substring(11, 13).toInt();
        int minute_ = datetime.substring(14, 16).toInt();
        
        watch.settime(0, minute_, hour_, day_, month_, year_, weekday_);
        
        request->send(200, "text/html", html_head+"<h2>Дата, время установлены!</h2><br><a href='/'>Вернуться</a></center></body>");
    } else {
        request->send(400, "text/html", "<h2>Ошибка: не переданы параметры</h2><br><a href='/'>Вернуться</a>");
    }
  });

  server.on("/SetTimers", HTTP_GET, [](AsyncWebServerRequest *request){

    Serial.println("=== SetTimers START ===");
    
    // Выводим ВСЕ параметры, которые пришли
    int paramsCount = request->params();
    Serial.print("Params count: ");
    Serial.println(paramsCount);
    
    for (int i = 0; i < paramsCount; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        Serial.print("  ");
        Serial.print(p->name().c_str());
        Serial.print(" = ");
        Serial.println(p->value().c_str());
    }
    uint8_t profileNum;
    String start1_h, start1_m, stop1_h, stop1_m;
    String start2_h, start2_m, stop2_h, stop2_m;
    String start3_h, start3_m, stop3_h, stop3_m;
    String start4_h, start4_m, stop4_h, stop4_m;
    String start5_h, start5_m, stop5_h, stop5_m;
    String start6_h, start6_m, stop6_h, stop6_m;
    String start7_h, start7_m, stop7_h, stop7_m;
    String start8_h, start8_m, stop8_h, stop8_m;
    String start9_h, start9_m, stop9_h, stop9_m;
    String start10_h, start10_m, stop10_h, stop10_m;
    String start11_h, start11_m, stop11_h, stop11_m;
    String start12_h, start12_m, stop12_h, stop12_m;
    
    if (request->hasParam("pName")) {
        profileNum = request->getParam("pName")->value().toInt();
        
        if(profileNum < 1 || profileNum > 7) {
            profileNum = 1;
        }
        
        uint16_t offset = PROFILE_OFFSETS[profileNum];
        
        // Читаем данные из формы
        start1_h = request->getParam("start1")->value().substring(0, 2);
        start1_m = request->getParam("start1")->value().substring(3, 5);
        stop1_h = request->getParam("stop1")->value().substring(0, 2);
        stop1_m = request->getParam("stop1")->value().substring(3, 5);
        
        start2_h = request->getParam("start2")->value().substring(0, 2);
        start2_m = request->getParam("start2")->value().substring(3, 5);
        stop2_h = request->getParam("stop2")->value().substring(0, 2);
        stop2_m = request->getParam("stop2")->value().substring(3, 5);
        
        start3_h = request->getParam("start3")->value().substring(0, 2);
        start3_m = request->getParam("start3")->value().substring(3, 5);
        stop3_h = request->getParam("stop3")->value().substring(0, 2);
        stop3_m = request->getParam("stop3")->value().substring(3, 5);
        
        start4_h = request->getParam("start4")->value().substring(0, 2);
        start4_m = request->getParam("start4")->value().substring(3, 5);
        stop4_h = request->getParam("stop4")->value().substring(0, 2);
        stop4_m = request->getParam("stop4")->value().substring(3, 5);
        
        start5_h = request->getParam("start5")->value().substring(0, 2);
        start5_m = request->getParam("start5")->value().substring(3, 5);
        stop5_h = request->getParam("stop5")->value().substring(0, 2);
        stop5_m = request->getParam("stop5")->value().substring(3, 5);
        
        start6_h = request->getParam("start6")->value().substring(0, 2);
        start6_m = request->getParam("start6")->value().substring(3, 5);
        stop6_h = request->getParam("stop6")->value().substring(0, 2);
        stop6_m = request->getParam("stop6")->value().substring(3, 5);
        
        start7_h = request->getParam("start7")->value().substring(0, 2);
        start7_m = request->getParam("start7")->value().substring(3, 5);
        stop7_h = request->getParam("stop7")->value().substring(0, 2);
        stop7_m = request->getParam("stop7")->value().substring(3, 5);
        
        start8_h = request->getParam("start8")->value().substring(0, 2);
        start8_m = request->getParam("start8")->value().substring(3, 5);
        stop8_h = request->getParam("stop8")->value().substring(0, 2);
        stop8_m = request->getParam("stop8")->value().substring(3, 5);
        
        start9_h = request->getParam("start9")->value().substring(0, 2);
        start9_m = request->getParam("start9")->value().substring(3, 5);
        stop9_h = request->getParam("stop9")->value().substring(0, 2);
        stop9_m = request->getParam("stop9")->value().substring(3, 5);
        
        start10_h = request->getParam("start10")->value().substring(0, 2);
        start10_m = request->getParam("start10")->value().substring(3, 5);
        stop10_h = request->getParam("stop10")->value().substring(0, 2);
        stop10_m = request->getParam("stop10")->value().substring(3, 5);
        
        start11_h = request->getParam("start11")->value().substring(0, 2);
        start11_m = request->getParam("start11")->value().substring(3, 5);
        stop11_h = request->getParam("stop11")->value().substring(0, 2);
        stop11_m = request->getParam("stop11")->value().substring(3, 5);
        
        start12_h = request->getParam("start12")->value().substring(0, 2);
        start12_m = request->getParam("start12")->value().substring(3, 5);
        stop12_h = request->getParam("stop12")->value().substring(0, 2);
        stop12_m = request->getParam("stop12")->value().substring(3, 5);
        
        // Сохраняем в EEPROM (без флагов!)
        uint8_t val;
        val = start1_h.toInt(); EEPROM.write(offset+1, val);
        val = start1_m.toInt(); EEPROM.write(offset+2, val);
        val = stop1_h.toInt(); EEPROM.write(offset+3, val);
        val = stop1_m.toInt(); EEPROM.write(offset+4, val);
        
        val = start2_h.toInt(); EEPROM.write(offset+5, val);
        val = start2_m.toInt(); EEPROM.write(offset+6, val);
        val = stop2_h.toInt(); EEPROM.write(offset+7, val);
        val = stop2_m.toInt(); EEPROM.write(offset+8, val);
        
        val = start3_h.toInt(); EEPROM.write(offset+9, val);
        val = start3_m.toInt(); EEPROM.write(offset+10, val);
        val = stop3_h.toInt(); EEPROM.write(offset+11, val);
        val = stop3_m.toInt(); EEPROM.write(offset+12, val);
        
        val = start4_h.toInt(); EEPROM.write(offset+13, val);
        val = start4_m.toInt(); EEPROM.write(offset+14, val);
        val = stop4_h.toInt(); EEPROM.write(offset+15, val);
        val = stop4_m.toInt(); EEPROM.write(offset+16, val);
        
        val = start5_h.toInt(); EEPROM.write(offset+17, val);
        val = start5_m.toInt(); EEPROM.write(offset+18, val);
        val = stop5_h.toInt(); EEPROM.write(offset+19, val);
        val = stop5_m.toInt(); EEPROM.write(offset+20, val);
        
        val = start6_h.toInt(); EEPROM.write(offset+21, val);
        val = start6_m.toInt(); EEPROM.write(offset+22, val);
        val = stop6_h.toInt(); EEPROM.write(offset+23, val);
        val = stop6_m.toInt(); EEPROM.write(offset+24, val);
        
        val = start7_h.toInt(); EEPROM.write(offset+25, val);
        val = start7_m.toInt(); EEPROM.write(offset+26, val);
        val = stop7_h.toInt(); EEPROM.write(offset+27, val);
        val = stop7_m.toInt(); EEPROM.write(offset+28, val);
        
        val = start8_h.toInt(); EEPROM.write(offset+29, val);
        val = start8_m.toInt(); EEPROM.write(offset+30, val);
        val = stop8_h.toInt(); EEPROM.write(offset+31, val);
        val = stop8_m.toInt(); EEPROM.write(offset+32, val);
        
        val = start9_h.toInt(); EEPROM.write(offset+33, val);
        val = start9_m.toInt(); EEPROM.write(offset+34, val);
        val = stop9_h.toInt(); EEPROM.write(offset+35, val);
        val = stop9_m.toInt(); EEPROM.write(offset+36, val);
        
        val = start10_h.toInt(); EEPROM.write(offset+37, val);
        val = start10_m.toInt(); EEPROM.write(offset+38, val);
        val = stop10_h.toInt(); EEPROM.write(offset+39, val);
        val = stop10_m.toInt(); EEPROM.write(offset+40, val);
        
        val = start11_h.toInt(); EEPROM.write(offset+41, val);
        val = start11_m.toInt(); EEPROM.write(offset+42, val);
        val = stop11_h.toInt(); EEPROM.write(offset+43, val);
        val = stop11_m.toInt(); EEPROM.write(offset+44, val);
        
        val = start12_h.toInt(); EEPROM.write(offset+45, val);
        val = start12_m.toInt(); EEPROM.write(offset+46, val);
        val = stop12_h.toInt(); EEPROM.write(offset+47, val);
        val = stop12_m.toInt(); EEPROM.write(offset+48, val);
        
        EEPROM.commit();
        
        Serial.print("Profile ");
        Serial.print(profileNum);
        Serial.print(" saved at offset: ");
        Serial.println(offset);
    }
    
    request->send(200, "text/html", html_head + "<center>Настройки сохранены!<br><a href='/'>На главную</a></center></body>");
});
  
  server.on("/music_on", HTTP_GET, [](AsyncWebServerRequest *request){
    music_on_break = true;
    request->send(200, "text/html", html_head+"<h2>Музыка на переменах ВКЛЮЧЕНА</h2><a href='/'>Назад</a></center></body>");
  });

  server.on("/music_off", HTTP_GET, [](AsyncWebServerRequest *request){
    music_on_break = false;
    stopMusic();
    request->send(200, "text/html", html_head+"<h2>Музыка на переменах ВЫКЛЮЧЕНА</h2><a href='/'>Назад</a></center></body>");
  });

  server.on("/music_random", HTTP_GET, [](AsyncWebServerRequest *request){
    music_mode_random = true;
    request->send(200, "text/html", html_head+"<h2>Режим: ПРОИЗВОЛЬНЫЙ</h2><a href='/'>Назад</a></center></body>");
  });

  server.on("/music_seq", HTTP_GET, [](AsyncWebServerRequest *request){
    music_mode_random = false;
    request->send(200, "text/html", html_head+"<h2>Режим: ПОСЛЕДОВАТЕЛЬНЫЙ</h2><a href='/'>Назад</a></center></body>");
  });

  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", upload_html);
  });
  
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!index) {
      uploadPath = "/music/" + filename;
      Serial.println("Upload started: " + uploadPath);
      
      if (!SD.exists("/music")) {
        SD.mkdir("/music");
        Serial.println("Created dir: /music");
      }
      
      if (filename.indexOf("/") > 0) {
        String dir = "/music/" + filename.substring(0, filename.lastIndexOf("/"));
        if (!SD.exists(dir)) {
          SD.mkdir(dir);
          Serial.println("Created dir: " + dir);
        }
      }
      
      uploadFile = SD.open(uploadPath, FILE_WRITE);
      if (!uploadFile) {
        Serial.println("Failed to open file for writing");
        return;
      }
    }
    
    if (uploadFile && len > 0) {
      uploadFile.write(data, len);
    }
    
    if (final) {
      if (uploadFile) {
        uploadFile.close();
        Serial.println("Upload finished: " + uploadPath);
      }
      loadMusicList();
    }
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String fileList = "";
    File root = SD.open("/music");
    if (!root) {
      request->send(500, "text/plain", "");
      return;
    }
    
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        if (fileList.length() > 0) fileList += ",";
        fileList += file.name();
      }
      file = root.openNextFile();
    }
    root.close();
    
    request->send(200, "text/plain", fileList);
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      String path = "/music/" + filename;
      
      if (SD.exists(path)) {
        if (SD.remove(path)) {
          Serial.println("Deleted: " + path);
          request->send(200, "text/plain", "OK");
        } else {
          request->send(500, "text/plain", "Delete failed");
        }
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } else {
      request->send(400, "text/plain", "No file specified");
    }
  });

  server.begin();
  Serial.println("");
  Serial.println("HTTP server started");
  getStartPoint();
  
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(10);
}

void audio_eof_mp3(const char *info){
  Serial.print("eof_mp3     ");
  Serial.println(info);
  
  if (music_playing) {
    playNextTrack();
    return;
  }
  
  String fileName = String(info);
  if (fileName.indexOf("e_") >= 0) {
    // Получаем номер урока из имени файла (e_1.mp3 -> 1)
    int lessonNum = fileName.substring(fileName.indexOf("_") + 1, fileName.indexOf(".")).toInt();
    
    // Следующий урок
    int nextLesson = lessonNum + 1;
    
    // Если это последний урок (12) - музыку не включаем
    if (nextLesson > 12) {
      Serial.println("Last lesson of the day - music not started");
    } else {
      // Получаем смещение для текущего профиля
      uint16_t offset = PROFILE_OFFSETS[startPoint];
      
      // Проверяем время начала следующего урока
      int nextStartHour = EEPROM.read(offset + (nextLesson-1)*4 + 1);
      int nextStartMinute = EEPROM.read(offset + (nextLesson-1)*4 + 2);
      
      // Если следующий урок не задан (00:00) - музыку не включаем
      if (nextStartHour == 0 && nextStartMinute == 0) {
        Serial.print("Next lesson ");
        Serial.print(nextLesson);
        Serial.println(" is empty (00:00) - music not started");
      } else {
        Serial.print("Next lesson ");
        Serial.print(nextLesson);
        Serial.print(" exists at ");
        Serial.print(nextStartHour);
        Serial.print(":");
        Serial.println(nextStartMinute);
        startMusic();
      }
    }
  }
  
  if(fire_flag){
    flag_fire = true;
  }
  if(terror_flag){
    flag_terror = true;  
  }
}

void loop(){
  audio.loop();

  if((millis()%100 == 0 && flag_fire) || (!digitalRead(fire) && digitalRead(terror))){
    delay(500);
    flag_fire = false;
    digitalWrite(Led_1, HIGH);
    audio.connecttoFS(SD, "/fire.mp3");
    Serial.println("Audio fire start");
    fire_flag = true;    
  }
  
  if((millis()%110 == 0 && flag_terror) || (!digitalRead(terror) && digitalRead(fire))){
    delay(500);
    flag_terror = false;
    digitalWrite(Led_2, HIGH);
    audio.connecttoFS(SD, "/terror.mp3");
    Serial.println("Audio terrors start");
    terror_flag = true;
  } 

  if(!digitalRead(terror) && !digitalRead(fire)){
    delay(500);
    Serial.println("Stop to play");
    audio.stopSong();
    terror_flag = false;
    fire_flag = false;
    flag_fire = false;
    flag_terror = false;
    digitalWrite(Led_1, LOW);
    digitalWrite(Led_2, LOW);
    ESP.restart();
  }
  
  if(millis() % 500 == 0){
    watch.gettime();
    uint16_t offset = PROFILE_OFFSETS[startPoint];
    
    //1 начало
    if(watch.Hours == EEPROM.read(offset+1) && watch.minutes == EEPROM.read(offset+2) && watch.seconds <= 1){
      s_1_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);
    }
    //2 конец
    if(watch.Hours == EEPROM.read(offset+3) && watch.minutes == EEPROM.read(offset+4) && watch.seconds <= 1){
      e_1_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);
    }
    //3 начало
    if(watch.Hours == EEPROM.read(offset+5) && watch.minutes == EEPROM.read(offset+6) && watch.seconds <= 1){
      s_2_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW); 
      stopMusic();  
    }
    //4 конец
    if(watch.Hours == EEPROM.read(offset+7) && watch.minutes == EEPROM.read(offset+8) && watch.seconds <= 1){
      e_2_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);  
    }
    //5 начало
    if(watch.Hours == EEPROM.read(offset+9) && watch.minutes == EEPROM.read(offset+10) && watch.seconds <= 1){
      s_3_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);
      stopMusic();  
    }
    //6 конец
    if(watch.Hours == EEPROM.read(offset+11) && watch.minutes == EEPROM.read(offset+12) && watch.seconds <= 1){
      e_3_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);
    }
    //7 начало
    if(watch.Hours == EEPROM.read(offset+13) && watch.minutes == EEPROM.read(offset+14) && watch.seconds <= 1){
      s_4_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);      
      digitalWrite(Led_2, LOW);
      stopMusic();  
    }
    //8 конец
    if(watch.Hours == EEPROM.read(offset+15) && watch.minutes == EEPROM.read(offset+16) && watch.seconds <= 1){
      e_4_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);      
      digitalWrite(Led_2, LOW);
    }
    //9 начало
    if(watch.Hours == EEPROM.read(offset+17) && watch.minutes == EEPROM.read(offset+18) && watch.seconds <= 1){
      s_5_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);
      stopMusic();  
    }
    //10 конец
    if(watch.Hours == EEPROM.read(offset+19) && watch.minutes == EEPROM.read(offset+20) && watch.seconds <= 1){
      e_5_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);
    }
    //11 начало
    if(watch.Hours == EEPROM.read(offset+21) && watch.minutes == EEPROM.read(offset+22) && watch.seconds <= 1){
      s_6_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);      
      stopMusic();  
    }
    //12 конец
    if(watch.Hours == EEPROM.read(offset+23) && watch.minutes == EEPROM.read(offset+24) && watch.seconds <= 1){
      e_6_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);
    }
    //13 начало
    if(watch.Hours == EEPROM.read(offset+25) && watch.minutes == EEPROM.read(offset+26) && watch.seconds <= 1){
      s_7_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);     
      stopMusic();  
    }
    //14 конец
    if(watch.Hours == EEPROM.read(offset+27) && watch.minutes == EEPROM.read(offset+28) && watch.seconds <= 1){
      e_7_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);
    }
    //15 начало
    if(watch.Hours == EEPROM.read(offset+29) && watch.minutes == EEPROM.read(offset+30) && watch.seconds <= 1){
      s_8_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);     
      stopMusic();  
    }
    //16 конец
    if(watch.Hours == EEPROM.read(offset+31) && watch.minutes == EEPROM.read(offset+32) && watch.seconds <= 1){
      e_8_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);      
    }
    //17 начало
    if(watch.Hours == EEPROM.read(offset+33) && watch.minutes == EEPROM.read(offset+34) && watch.seconds <= 1){
      s_9_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);
      stopMusic();  
    }
    //18 конец
    if(watch.Hours == EEPROM.read(offset+35) && watch.minutes == EEPROM.read(offset+36) && watch.seconds <= 1){
      e_9_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);   
    }
    //19 начало
    if(watch.Hours == EEPROM.read(offset+37) && watch.minutes == EEPROM.read(offset+38) && watch.seconds <= 1){
      s_10_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW); 
      stopMusic();  
    }
    //20 конец
    if(watch.Hours == EEPROM.read(offset+39) && watch.minutes == EEPROM.read(offset+40) && watch.seconds <= 1){
      e_10_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);  
    }
    //21 начало
    if(watch.Hours == EEPROM.read(offset+41) && watch.minutes == EEPROM.read(offset+42) && watch.seconds <= 1){
      s_11_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);
      digitalWrite(Led_2, LOW);
      stopMusic();  
    }
    //22 конец
    if(watch.Hours == EEPROM.read(offset+43) && watch.minutes == EEPROM.read(offset+44) && watch.seconds <= 1){
      e_11_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW); 
      digitalWrite(Led_2, LOW);  
    }
    //23 начало
    if(watch.Hours == EEPROM.read(offset+45) && watch.minutes == EEPROM.read(offset+46) && watch.seconds <= 1){
      s_12_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);      
      digitalWrite(Led_2, LOW);
      stopMusic();  
    }
    //24 конец
    if(watch.Hours == EEPROM.read(offset+47) && watch.minutes == EEPROM.read(offset+48) && watch.seconds <= 1){
      e_12_f = false;
      digitalWrite(relay, HIGH);
      digitalWrite(Led_2, HIGH);
      delay(2000);
      digitalWrite(relay, LOW);      
      digitalWrite(Led_2, LOW);
    }
  }
  if(watch.Hours == 0 && watch.minutes == 0 && watch.seconds <= 5){
    getStartPoint();
  }
  
  if(!s_1_f){
    s_1_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_1.mp3");
  }
  if(!e_1_f){
    e_1_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_1.mp3");
  }
  if(!s_2_f){
    s_2_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_2.mp3");
  }
  if(!e_2_f){
    e_2_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_2.mp3");
  }
  if(!s_3_f){
    s_3_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_3.mp3");
  }
  if(!e_3_f){
    e_3_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_3.mp3");
  }
  if(!s_4_f){
    s_4_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_4.mp3");
  }
  if(!e_4_f){
    e_4_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_4.mp3");
  }
  if(!s_5_f){
    s_5_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_5.mp3");
  }
  if(!e_5_f){
    e_5_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_5.mp3");
  }
  if(!s_6_f){
    s_6_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_6.mp3");
  }
  if(!e_6_f){
    e_6_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_6.mp3");
  }
  if(!s_7_f){
    s_7_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_7.mp3");
  }
  if(!e_7_f){
    e_7_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_7.mp3");
  }
  if(!s_8_f){
    s_8_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_8.mp3");
  }
  if(!e_8_f){
    e_8_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_8.mp3");
  }
  if(!s_9_f){
    s_9_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_9.mp3");
  }
  if(!e_9_f){
    e_9_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_9.mp3");
  }
  if(!s_10_f){
    s_10_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_10.mp3");
  }
  if(!e_10_f){
    e_10_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_10.mp3");
  }
  if(!s_11_f){
    s_11_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_11.mp3");
  }
  if(!e_11_f){
    e_11_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_11.mp3");
  }
  if(!s_12_f){
    s_12_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/s_12.mp3");
  }
  if(!e_12_f){
    e_12_f = true;
    audio.setVolume(20);
    audio.connecttoFS(SD, "/e_12.mp3");
  }
}
