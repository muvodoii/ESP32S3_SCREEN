
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <ui.h>
#include <stdio.h>
#include <Arduino.h>
#include "RTClib.h"

#include "WiFi.h"
#include <vector>
#include <EEPROM.h>
#include "time.h"

#define EEPROM_SIZE 512  
#define EEPROM_ADDRESS_BASE 0  


#define SSID_ADDRESS_OFFSET 100  
#define PASSWORD_ADDRESS_OFFSET SSID_ADDRESS_OFFSET + 64 
extern lv_obj_t * ui_valueabc;

#define EEPROM_SIZE 128
#define EEPROM_ADDR_WIFI_FLAG 0
#define EEPROM_ADDR_WIFI_CREDENTIAL 4

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -8 * 60 * 60;  // Set your timezone here
const int daylightOffset_sec = 0;

typedef enum {
  NONE,
  NETWORK_SEARCHING,
  NETWORK_CONNECTED_POPUP,
  NETWORK_CONNECTED,
  NETWORK_CONNECT_FAILED
} Network_Status_t;
Network_Status_t networkStatus = NONE;

const int coi  = 35;

////
static lv_style_t border_style;
static lv_style_t popupBox_style;
static lv_obj_t *timeLabel;
static lv_obj_t *settings;
static lv_obj_t *settingBtn;
static lv_obj_t *settingCloseBtn;
static lv_obj_t *settingWiFiSwitch;
static lv_obj_t *wfList;
static lv_obj_t *settinglabel;
static lv_obj_t *mboxConnect;
static lv_obj_t *mboxTitle;
static lv_obj_t *mboxPassword;
static lv_obj_t *mboxConnectBtn;
static lv_obj_t *mboxCloseBtn;
static lv_obj_t *keyboard;
static lv_obj_t *popupBox;
static lv_obj_t *popupBoxCloseBtn;
static lv_timer_t *timer;

static int foundNetworks = 0;
unsigned long networkTimeout = 10 * 1000;
String ssidName, ssidPW;

TaskHandle_t ntScanTaskHandler, ntConnectTaskHandler;
std::vector<String> foundWifiList;
////



extern lv_obj_t * ui_Label1;
extern lv_obj_t * ui_Label2;
extern uint8_t gio, phut, giay;
extern uint8_t setBuzzer;

 RTC_PCF8563 rtc;
// extern lv_obj_t *ui_Screen4;
 char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define GFX_BL DF_GFX_BL 

#define GFX_BL 44
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */
);

 Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
   bus,
   480 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
   272 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
   1 /* pclk_active_neg */, 16000000 /* prefer_speed */, true /* auto_flush */);


#include "touch.h"
#include "ui.h"

/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

String valueFromEEPROM;
void setup()
{
   Serial.begin(115200);
   EEPROM.begin(EEPROM_SIZE);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  pinMode(coi,OUTPUT);
  Serial.println("Hello World");

#ifdef GFX_PWD
  pinMode(GFX_PWD, OUTPUT);
  digitalWrite(GFX_PWD, HIGH);
#endif

  // Init touch device
  touch_init(gfx->width(), gfx->height());

  // Init Display
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif

  lv_init();

  screenWidth = gfx->width();
  screenHeight = gfx->height();
#ifdef ESP32
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 10, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
  disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * 10);
#endif
  if (!disp_draw_buf)
  {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * 10);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    ui_init();

    Serial.println("Setup done");
    
    if (! rtc.begin())
    {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    }

    if (rtc.lostPower()) {
    Serial.println("RTC is NOT initialized, let's set the time!");
    
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  rtc.start();
  }
 
}

static uint32_t tick1 = 0, tick2 = 0;
static uint32_t systick_timer = 0;
void loop() {
     
    systick_timer = millis();
    if(systick_timer - tick1 > 5 )
    {
      tick1 = systick_timer;
      lv_timer_handler();
      buzzer_action();
    }
    
    if(systick_timer - tick2 > 1000 )
    {
      tick2 = systick_timer;
      
    DateTime now = rtc.now();

    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

    gio = now.hour();  
    phut = now.minute(); 
    giay = now.second(); 
    }
    Serial.println("scan start");

  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
      Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10);
    }
  }
  Serial.println("");

  Serial.println("Enter the number of the network you want to connect to:");
  while (Serial.available() == 0) {
  }
  
  // Read user input (network index)
  int selectedNetwork = Serial.parseInt();
  if (selectedNetwork > 0 && selectedNetwork <= n) {
    Serial.print("Selected network: ");
    Serial.println(WiFi.SSID(selectedNetwork - 1));

  
    Serial.println("Enter WiFi password:");
    while (Serial.available() == 0) {
     
    }
    
    
    String password = Serial.readStringUntil('\n');
    password.trim(); // Remove any leading/trailing whitespace

    
    storeCredentials(selectedNetwork - 1, WiFi.SSID(selectedNetwork - 1), password);

 
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WiFi.SSID(selectedNetwork - 1).c_str(), password.c_str());

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.print("WiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Invalid selection. Please enter a valid number.");
  }

  Serial.println("Scan again in 5 seconds...");
  delay(5000);

}
void storeCredentials(int index, String ssid, String password) {

  int ssidAddr = EEPROM_ADDRESS_BASE + SSID_ADDRESS_OFFSET + index * 64; 
  int passwordAddr = EEPROM_ADDRESS_BASE + PASSWORD_ADDRESS_OFFSET + index * 64; 

  // Write SSID to EEPROM
  for (size_t i = 0; i < ssid.length(); ++i) {
    EEPROM.write(ssidAddr + i, ssid[i]);
  }
  EEPROM.write(ssidAddr + ssid.length(), '\0'); // Null-terminate the string

  // Write password to EEPROM
  for (size_t i = 0; i < password.length(); ++i) {
    EEPROM.write(passwordAddr + i, password[i]);
  }
  EEPROM.write(passwordAddr + password.length(), '\0'); // Null-terminate the string

  // Commit the EEPROM contents to flash
  EEPROM.commit();
}

void readCredentials(int index, String &ssid, String &password) {

  int ssidAddr = EEPROM_ADDRESS_BASE + SSID_ADDRESS_OFFSET + index * 64;
  int passwordAddr = EEPROM_ADDRESS_BASE + PASSWORD_ADDRESS_OFFSET + index * 64; 

  // Read SSID from EEPROM
  ssid = "";
  char ch = EEPROM.read(ssidAddr);
  while (ch != '\0') {
    ssid += ch;
    ch = EEPROM.read(++ssidAddr);
  }

  // Read password from EEPROM
  password = "";
  ch = EEPROM.read(passwordAddr);
  while (ch != '\0') {
    password += ch;
    ch = EEPROM.read(++passwordAddr);
  }
}

void buzzer_action(void)
{
  if(setBuzzer ==  1)
  {
    digitalWrite(35, HIGH);
    delay(50);
    digitalWrite(35, LOW);
    setBuzzer = 0;
 
  }
}

void saveToEEPROM(const char* valueToSave) {
    Serial.print("Saving value to EEPROM: ");
    Serial.println(valueToSave);

    int len = strlen(valueToSave);
    for (int i = 0; i < len; ++i) {
        EEPROM.write(i, valueToSave[i]);
    }
    EEPROM.write(len, '\0'); // Kết thúc chuỗi bằng ký tự null
    EEPROM.commit(); // Lưu các thay đổi vào EEPROM

    Serial.println("Value saved to EEPROM.");
}

String readFromEEPROM() {
    char valueRead[20]; // Độ dài tối đa của chuỗi cần đọc

    int addr = 0;
    char ch = EEPROM.read(addr++);
    int i = 0;

    Serial.println("Reading from EEPROM:");
    while (ch != '\0' && i < 19) {
        valueRead[i++] = ch;
        Serial.print(ch);
        ch = EEPROM.read(addr++);
    }
    valueRead[i] = '\0';

    Serial.print("Value read from EEPROM: ");
    Serial.println(valueRead);

    return String(valueRead);
}