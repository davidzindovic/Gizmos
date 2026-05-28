#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Arduino.h>
#include <WiFi.h>
//#include <Hash.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5
#define SEALEVELPRESSURE_HPA (1013.25)

#define TIPKA_MODE 27
#define TIPKA_A 33
#define TIPKA_B 26

#define LDR_ANALOG 36
#define MIC_ANALOG 39 

#define WIFI_ON_PIN 4

#define ST_VZORCEV 30
#define ST_VELICIN 8 //7 v tabeli, 8. so PEOPLE
#define PROSTOR_ZA_MOJE_MERITVE 10 //lahko pomni največ 10 meritev lastnih

#define TIPKE_TIME_THRESHOLD 200 //v ms
#define LCD_REFRESH_TIME 150
#define MERITVE_REFRESH_TIME 1000

#define DEBUG 0

const char* ssid     = "GREEN-NUDGE-5";
const char* password = "ALUO-FE-5";

//html variables:
float TEMPERATURE = 0.0;
float HUMIDITY = 0.0;
float PRESSURE = 0.0;
float ALTITUDE = 0.0;
float GAS = 0.0;
float BRIGHTNESS = 0.0;
float LOUDNESS = 0.0;
uint16_t PEOPLE = 0;

//################# SERVER ZADEVE ##################################


AsyncWebServer server(80);

// HTML and CSS for the web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      margin: 0px auto;
      text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; margin: 20px 0; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align: middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>Summer school Measuring station 5</h2>
  

    <p>
      <span class="dht-labels">Temperature</span> 
      <span id="temperature">%TEMPERATURE%</span>
      <sup class="units">&deg;C</sup>
    </p>

    <p>
      <span class="dht-labels">Humidity</span>
      <span id="humidity">%HUMIDITY%</span>
      <sup class="units">%%</sup>
    </p>

    <p>
      <span class="dht-labels">Pressure</span>
      <span id="pressure">%PRESSURE%</span>
      <sup class="units">hPa</sup>
    </p>

        <p>
      <span class="dht-labels">Altitude</span>
      <span id="altitude">%ALTITUDE%</span>
      <sup class="units">m</sup>
    </p>

        <p>
      <span class="dht-labels">Gas</span>
      <span id="gas">%GAS%</span>
      <sup class="units">kOhm</sup>
    </p>

        <p>
      <span class="dht-labels">Brightness</span>
      <span id="brightness">%BRIGHTNESS%</span>
      <sup class="units">lux</sup>
    </p>

        <p>
      <span class="dht-labels">Loudness</span>
      <span id="loudness">%LOUDNESS%</span>
      <sup class="units">Db</sup>
    </p>
       <p>
      <span class="dht-labels">People</span>
      <span id="people">%PEOPLE%</span>
      <sup class="units"></sup>
    </p>

</body>
<script>
function updateData(id, endpoint) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById(id).innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", endpoint, true);
  xhttp.send();
}

setInterval(function () {
  updateData("temperature", "/temperature");
  updateData("humidity", "/humidity");
  updateData("pressure", "/pressure");
  updateData("altitude", "/altitude");
  updateData("gas", "/gas");
  updateData("brightness", "/brightness");
  updateData("loudness", "/loudness");
  updateData("people", "/people");
}, 10000);
</script>
</html>)rawliteral";

//##########################################################


uint16_t povprecja[ST_VELICIN-1][ST_VZORCEV]={0}; //tabela preteklih meritev, iz kjer računamo povprečje
uint32_t moje_meritve_vrednosti[PROSTOR_ZA_MOJE_MERITVE][2];//hrani v prvem stolpcu izmerjeno vrednost, v drugem cas zadnje meritve
String moje_meritve_napisi[PROSTOR_ZA_MOJE_MERITVE][2];    //hrani v prvem stolpcu ime velicine, v drugem pa enote

String velicine[ST_VELICIN-1]={"Loudness: ","Humidity: ","Bright: ","Temp: ","Pressure: ","Gas: ","Altitude: "};
String enote[ST_VELICIN-1]={"dB","%","lx","C","hPa","k","m"};

uint8_t kazalec=0; //da vem katera meritva je bila zadnja
bool wifi=0;

byte stopinje[8] = {
  0b01100,
  0b10010,
  0b01100,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

byte ohm[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b10001,
  0b10001,
  0b11011,
  0b01010,
  0b11011
};

int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows); 

Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);


//########################## SETUP #############################
void setup() {

pinMode(TIPKA_MODE,INPUT_PULLUP);
pinMode(TIPKA_A,INPUT_PULLUP);
pinMode(TIPKA_B,INPUT_PULLUP);

pinMode(LDR_ANALOG,INPUT);
pinMode(MIC_ANALOG,INPUT);

#if DEBUG
Serial.begin(115200);delay(1000);
#endif

if(!digitalRead(WIFI_ON_PIN)){srvr();}

while(!bme.begin()){}

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

lcd.init();
lcd.backlight();
}
// #################################################

//######################## LOOP ############################
void loop() {

//spremenljivka za menjavanje kanala za prikaz
//ko prideš v nek mode se spremeni, da prideš ven
//moraš pritisniti b(back_button)
static bool window_pointer=1;//dokler je 1 lahko greš v mode
static uint8_t izbrana_velicina=0;
static uint8_t moje_meritve_pointer=0;
static bool moje_meritve_ptr_of=0; //za spremljanje ponovnega poteka
static bool moje_meritve_prioriteta=0;

//maske za mode:
static uint8_t mask_mode_scroll=0b100;
static uint8_t mask_save_scroll=0b001; // scrolla po saveanih meritvah, napiše čas od meritve 
static uint8_t mask_back_ok_button=0b010;
static uint8_t mask_save=0b011;
static uint8_t mask_wifi_publish=0b110; //poskusi se dat na wifi in publishat
static uint8_t mask_display_toggle=0b101;

static uint32_t cajt_zadnje_meritve=0;

uint8_t TIPKE=fronte_tipk();

#if DEBUG==2

Serial.print("TIPKE: ");
Serial.println(TIPKE,BIN);

#endif

//da ne gledamo pogojev če ni treba:
if(TIPKE>0){
if(TIPKE==mask_display_toggle)
{ 
  #if DEBUG
  Serial.println("DISPLAY BACKLIGHT TOGGLE");
  #endif
  
  static bool display_on=1;
  display_on=!display_on;
  if(display_on)lcd.backlight();
  else lcd.noBacklight();
}
//če sočasno pritisnemo tipki MODE in B sočasno publishamo zadnje shranjene meritve vseh veličin
//neposredno iz tabele iz katere tudi računamo povprečja (torej zadnji stolpec)
else if((TIPKE==mask_wifi_publish)&&(window_pointer))
{ 
  #if DEBUG
  Serial.println("WIFI PUBLISH");
  #endif
   
  window_pointer=0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Publishing...");

  if(wifi)
  {
  wifi_publish();
  delay(100); //da zgleda proper
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Successfully");
  lcd.setCursor(0,1);
  lcd.print("published!");
  delay(500);
  }

  else
  {
  delay(100); //da zgleda proper
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Can't publish,");
  lcd.setCursor(0,1);
  lcd.print("not plugged in.");
  delay(500);
  }
  
}
//če pritisnemo tipki A in B sočasno shranimo
//meritev, ki je v trenutku pritiska prikazana na zaslonu
// po uspešni shranitvi moramo pritisniti B za izhod
else if((TIPKE==mask_save)&&(window_pointer))
{ 
  #if DEBUG
  Serial.println("SHRANJEVANJE");
  #endif
   
  window_pointer=0;
  moje_meritve_napisi[moje_meritve_pointer][0]=velicine[izbrana_velicina];//zapišem v tabelo "ime veličine"
  moje_meritve_vrednosti[moje_meritve_pointer][0]=povprecja[izbrana_velicina][(kazalec-1)*(kazalec>0)+(ST_VZORCEV-1)*(kazalec==0)]; //prepišem zadnjo vrednost
  moje_meritve_napisi[moje_meritve_pointer][1]=enote[izbrana_velicina];
  moje_meritve_vrednosti[moje_meritve_pointer][1]=millis();
  moje_meritve_pointer++;
  if(moje_meritve_pointer==PROSTOR_ZA_MOJE_MERITVE){moje_meritve_pointer=0;moje_meritve_ptr_of=1;}
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("You saved the");
  lcd.setCursor(0,1);
  lcd.print("measurment!");
  
}
//ob pritisku na tipko B štejemo ljudi če smo na home screen
//če nismo na home screen se vrnemo na home screen
else if((TIPKE==mask_back_ok_button))
{ 
  #if DEBUG
  Serial.println("ČLOVEK++");
  #endif
  
  if(window_pointer)PEOPLE++;
  else window_pointer=1; //vrne na default window value, kar pomeni da smo na home screen

  if(moje_meritve_prioriteta)moje_meritve_prioriteta=0;
}
//tukaj lahko prikazujemo lastne meritve s pritiskom tipke A, pri čemer se odpre ločeno okno
//za izhod iz načina za ogled lastnih meritev moramo pritisniti tipko B
else if((TIPKE==mask_save_scroll)&&(window_pointer)||(TIPKE==mask_save_scroll)&&(moje_meritve_prioriteta))
{ 
  #if DEBUG
  Serial.println("LISTAM SAVE");
  #endif

  static uint8_t save_scroll_pointer=0;
  if(!moje_meritve_prioriteta)
  {
    save_scroll_pointer=0;//ob ponovnem vstopu resetiram pointer za moj save
    moje_meritve_prioriteta=1;//postavimo prioriteto da smo v posebnem sub meniju
    window_pointer=0;
  }
//if((!(moje_meritve_pointer==0))||(moje_meritve_ptr_of))IzpisLastnihMeritev(save_scroll_pointer);

  //če je pointer na koncu seznama ali pa na koncu polnih elementov preprečimo inkrementiranje
  if((((save_scroll_pointer)==moje_meritve_pointer)&&(!moje_meritve_ptr_of))||((save_scroll_pointer)==PROSTOR_ZA_MOJE_MERITVE)){}
  else {IzpisLastnihMeritev(save_scroll_pointer);save_scroll_pointer++;}
}
//s pritiskom na tipko MODE na home screenu listamo po
//prikazih trenutne vrednosti veličin (vsak pritisk nam prikaže naslednjo veličino)
//prikaz se zgodi izven if-a
else if((TIPKE==mask_mode_scroll)&&(window_pointer))
{ 
  #if DEBUG
  Serial.println("SCROLL");
  #endif
  
  izbrana_velicina++;
  if(izbrana_velicina==ST_VELICIN)izbrana_velicina=0;
}
}


//izven checkanja tipk:

if((millis()-cajt_zadnje_meritve)>MERITVE_REFRESH_TIME)
{
 
meritve();
cajt_zadnje_meritve=millis();
}

//meritve();

if((izbrana_velicina==7) && (window_pointer))IzpisNaEkran(izbrana_velicina,PEOPLE,1);
else if(window_pointer)IzpisNaEkran(izbrana_velicina,povprecja[izbrana_velicina][(kazalec-1)*(kazalec>0)+(ST_VZORCEV-1)*(kazalec==0)],1); //povprecje skos izpisuje ker mi je zmanjkal tipk lol

//test izpisa arraya velicin (samo napisov):
//for (uint8_t i=0;i<(ST_VELICIN-1);i++){IzpisNaEkran(i,(uint16_t)i,0);delay(500);lcd.clear();}

//test analognih velicin:
//Serial.print("LDR: ");Serial.print(osvetljenost());
//delay(100);
//Serial.print("  |  MIC: ");Serial.println(hrup());
//delay(100);

//test tipkovnice:
//Serial.println(fronte_tipk(),BIN);delay(200);

}
//################## KONEC LOOPA ###########################


//--------------------------FUNKCIJE:---------------------------------

//#########################IZPIS NA EKRAN##############################

//ta funkcija vzame index, kje se nahajajo stringi za izpis imen in enot veličin
//ter vrednost za izpis (razberemo iz tabele preteklih meritev)
//dodana je opcija za povprečenje, ki je trenutno vedno on v glavnem loopu
//zaradi pomanjkanja kombinacij gumbov lol
//aja drgač pa zapiše stvari na ekran talele funkcija
void IzpisNaEkran(uint8_t velicina_index, uint16_t vrednost,bool avrg)
{ 
static uint32_t zadnji_refresh=0;

#if DEBUG==2
Serial.print("IZPIS NA EKRAN | INDEX & VREDNOST: ");
Serial.print(velicina_index);
Serial.print("  ");
Serial.println(vrednost);
#endif

//delay(80);

if((millis()-zadnji_refresh)>=LCD_REFRESH_TIME)
{
lcd.clear();

if(velicina_index==7)
{
  lcd.setCursor(0, 0);
  lcd.print("NUMBER OF");
  lcd.setCursor(0, 1);
  lcd.print("PEOPLE");
  lcd.setCursor(16-(vrednost>=0)-(vrednost>9)-(vrednost>99),0); //rezerviramo dovolj števk
  lcd.print(vrednost);
}

else{
  uint8_t unit_rezerva; //+1 za presledek med vrednostjo in enoto
  if((velicina_index==0) || (velicina_index==2) || (velicina_index==3)||(velicina_index==5)) unit_rezerva=2+1;
  else if(velicina_index==4)unit_rezerva=3+1;
  else unit_rezerva=1+1;
  
  lcd.setCursor(0, 0);
  lcd.print(velicine[velicina_index]);
  lcd.setCursor(16-(vrednost>=0)-(vrednost>9)-(vrednost>99)-(vrednost>999)-unit_rezerva,0); //rezerviramo dovolj števk
  lcd.print(vrednost);
  lcd.setCursor(16-unit_rezerva+1,0); //-1 da bo presledek

  if(velicina_index==3)
  {
    lcd.createChar(0, stopinje);
    lcd.setCursor(16-unit_rezerva+1,0);
    lcd.write((byte)0);
    lcd.print(enote[velicina_index]);
  }
  else if(velicina_index==5)
    {
    lcd.createChar(0, ohm);
    lcd.setCursor(16-unit_rezerva+1,0);
    lcd.print(enote[velicina_index]);
    lcd.setCursor(16-unit_rezerva+2,0);
    lcd.write((byte)0);
  }
  else lcd.print(enote[velicina_index]);

  if(avrg)
  {
    uint8_t index=0;
    uint8_t nenicelni_index=0;
    uint32_t povprecje=0;

    while(index<ST_VZORCEV)
    {
      
      if(povprecja[velicina_index][index]!=0)
      {
      povprecje+=povprecja[velicina_index][index];
      nenicelni_index++;
      }
      index++;
    }
    povprecje/=((nenicelni_index+1)*(nenicelni_index==0)+(nenicelni_index)*(nenicelni_index>0));
    lcd.setCursor(0,1);
    lcd.print("Average: ");
    lcd.setCursor(11,1);
    lcd.print(povprecje);
    //lcd.setCursor(11+povprecje>=0+povprecje>9+povprecje>99+1,1);
    //lcd.print(enote[velicina_index]);
  }
}
zadnji_refresh=millis(); 
}

}
//#####################################################################


//######################IZPIS SHRANJENIH MERITEV NA EKRAN####################### 

//ta funkcija je namenjena izpisu iz tabele lastnih meritev (torej meritev,
//ki jih je uporabnik shranil). Deluje podobno kot navadna funkcija
//IzpisNaEkran, vendar tukaj drugače formatiram zamike in izpišem minuli čas
void IzpisLastnihMeritev(uint8_t SaveScrool_ptr)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(moje_meritve_napisi[SaveScrool_ptr][0]); //izpis imena velicine
  lcd.setCursor(16-(moje_meritve_vrednosti[SaveScrool_ptr][0]>=0)-(moje_meritve_vrednosti[SaveScrool_ptr][0]>9)-(moje_meritve_vrednosti[SaveScrool_ptr][0]>99)-(moje_meritve_vrednosti[SaveScrool_ptr][0]>999)-3,0); //rezerva je 3 ker je hPa najdaljša enota, jbg
  lcd.print(moje_meritve_vrednosti[SaveScrool_ptr][0]); //izpis izmerjene vrednosti
  lcd.setCursor(16-3,0); //rezerva je 3 ker je hPa najdaljša enota, jbg
  lcd.print(moje_meritve_napisi[SaveScrool_ptr][1]); //izpis enote
  lcd.setCursor(0,1);
  lcd.print("TimePast:");
  uint32_t minute=millis()/1000/60-moje_meritve_vrednosti[SaveScrool_ptr][1]/1000/60;
  uint32_t sekunde=millis()/1000-moje_meritve_vrednosti[SaveScrool_ptr][1]/1000-minute*60;
  #if DEBUG
  Serial.print("MINUTE: ");Serial.print(minute);
  Serial.print("  | SEKUNDE: ");Serial.println(sekunde);
  #endif
  lcd.setCursor(10+4-(sekunde>9)-2*(minute>0)-(minute>9),1); //plac za minute rabi se enoto zato 2*
  if(minute>0)
  {
    lcd.print(minute);
    lcd.setCursor(10+4-(sekunde>9)-2*(minute>0)-(minute>9)+(minute>0)+(minute>9),1);
    lcd.print("m");
    lcd.setCursor(15-1-(sekunde>9),1);
    lcd.print(sekunde);
    lcd.setCursor(15,1);
    lcd.print("s");
  }
  else
  {
    lcd.print(sekunde);
    lcd.setCursor(15,1);
    lcd.print("s");
  }
}

//#############################################################


//######################### MERITVE ######################

//ta funkcija prebere vse module in shrani meritve v tabelo
//ter premakne kazalec na naslednje mesto za zapis meritev
void meritve()
{
static bool kuadej = 0;

if(kuadej)
{

if(bme.endReading()){
#if DEBUG
Serial.println("Nove Meritve!");
#endif
povprecja[0][kazalec]=hrup();
povprecja[2][kazalec]=osvetljenost();
povprecja[1][kazalec]=bme.humidity;
povprecja[3][kazalec]=bme.temperature;
povprecja[4][kazalec]=bme.pressure/100.0;
povprecja[5][kazalec]=bme.gas_resistance/1000.0;
povprecja[6][kazalec]=bme.readAltitude(SEALEVELPRESSURE_HPA);

kazalec++;
if(kazalec>=ST_VZORCEV)kazalec=0;
kuadej=0;
}
}

if(!kuadej)
{ 
  if(bme.beginReading()==0){}
  else
  {
  kuadej=1;
  }
}


}
//#######################################################

//##################### HRUP ################################

//ta funkcija odpre primeren tranzistor in
//izmeri ter vrne vrednost na mikrofonu

uint16_t hrup()
{
   uint16_t tisina=2040; //vrednost brez zvokov
  //const uint16_t tisina_wiggle=50;
   uint16_t hrup_minimum_db=15; //probavu da bo v sobi cca 30
   uint16_t najvecji_pricakovan_odmik=1200;
   uint16_t hrup_max_db=90;
  uint16_t hrup=analogRead(MIC_ANALOG);
  float hrup_output=(abs(tisina-hrup))*(hrup_max_db-hrup_minimum_db)/najvecji_pricakovan_odmik+hrup_minimum_db;
  return(hrup_output);
}
//#########################################################

//############# OSVETLJENOST ##########################

//ta funkcija odpre primeren tranzistor in
//izmeri ter vrne vrednost na LDR uporu
uint16_t osvetljenost()
{ //problem so LED od modulov -> faljena meritev
   float ldr_vcc=3.3;
   float pullup_upor=100000;
   uint16_t upornost_lux_min=100000;
   uint16_t upornost_lux_max=1000;
   uint16_t lux_min=0.01; //polna luna
   uint16_t lux_max=10000;//sončna svetloba, ne direkt sonce

  uint16_t lux_rn=analogRead(LDR_ANALOG);
  float trenutna_ldr_upornost=abs(-pullup_upor/(1-(ldr_vcc/(lux_rn*ldr_vcc/2/4095+0.001))));
  float lux_output=(1-(trenutna_ldr_upornost/upornost_lux_min))*lux_max+lux_min;
  return(lux_output);
}
//##################################################

//################## TIPKE ##########################

//ta funkcija vrne masko tipk, ki so bile pritisnjene
uint8_t fronte_tipk()
{
static uint8_t tipke_cajt=0;
bool return_flag=0;
static uint8_t tipke_prej;

uint8_t tipke=(digitalRead(TIPKA_MODE)<<2)|(digitalRead(TIPKA_B)<<1)|(digitalRead(TIPKA_A)<<0);

if((tipke_prej>tipke)&&((millis()-tipke_cajt)>TIPKE_TIME_THRESHOLD)){return_flag=1;tipke_cajt=millis();}

tipke_prej=tipke;

if(return_flag)return (0b111-((digitalRead(TIPKA_MODE)<<2)|(digitalRead(TIPKA_B)<<1)|(digitalRead(TIPKA_A)<<0)));
else return 0;
}
//###################################################

//############ WIFI ###########################

//ta funkcija posodobi globalne spremenljivke, ki jih mikrokrmilnik
//že sam od sebe meče v html stran, ki jo uporabnik odpre na telefonu
void wifi_publish() 
{
LOUDNESS = povprecja[0][kazalec];
HUMIDITY = povprecja[1][kazalec];
BRIGHTNESS = povprecja[2][kazalec];
TEMPERATURE = povprecja[3][kazalec];
PRESSURE = povprecja[4][kazalec];
GAS = povprecja[5][kazalec];
ALTITUDE = povprecja[6][kazalec];
}
//#################################################


//################################
void srvr()
{


WiFi.softAP(ssid, password);
IPAddress IP = WiFi.softAPIP();

#if DEBUG
Serial.print("AP IP address: ");
Serial.println(IP);
#endif

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Route to get temperature
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(TEMPERATURE).c_str());
  });

  // Route to get humidity
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(HUMIDITY).c_str());
  });

  // Route to get pressure
  server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(PRESSURE).c_str());
  });

    server.on("/altitude", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(ALTITUDE).c_str());
  });

    server.on("/gas", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(GAS).c_str());
  });

    server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(BRIGHTNESS).c_str());
  });

      server.on("/loudness", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(LOUDNESS).c_str());
  });

      server.on("/people", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(PEOPLE).c_str());
  });

  // Start server
  server.begin();
  wifi=1;

}

String processor(const String& var) {
  if (var == "TEMPERATURE") {
    return String(TEMPERATURE);
  } else if (var == "HUMIDITY") {
    return String(HUMIDITY);
  } else if (var == "PRESSURE") {
    return String(PRESSURE);
  }else if (var == "ALTITUDE") {
    return String(ALTITUDE);
  }else if (var == "GAS") {
    return String(GAS);
  }
  else if (var == "BRIGHTNESS") {
    return String(BRIGHTNESS);
  }
  else if (var == "LOUDNESS") {
    return String(LOUDNESS);
  }
  else if (var == "PEOPLE") {
    return String(PEOPLE);
  }
  return String();
}
