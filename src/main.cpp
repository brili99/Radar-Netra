#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Ticker.h>
#include <DFPlayerMini_Fast.h>

#define DEBUG_LEVEL 1
#define DEBUG_MP3 false
#define INTERVAL_LISTEN_BUTTON 200
#define SERIAL_BAUD_RATE 115200
#define DFPLAYER_READY_WHEN_COUNTER 1

void scanDevice();
void bleInit();
void initDfplayer();
bool playMusicDfplayer(int music, bool force);
void initPins();
void pinListener();
void btnAction();
void btnPanicAction();
void btnTestAction();
void busyAction();
void modeAction();
void panicAction();

int scanTime = 5,
    nearestRSSI;
bool foundITag = false,
     busy = false,
     dfplayer_ready = true,
     skip_one_counter = false,
     toggle_panic = false,
     btn_panic = true,
     btn_panic_clicked = false,
     btn_test = true,
     btn_test_clicked = false;
byte mode_dfplayer = 0; // 0 sedang tidak diguanakan, 1 terdeteksi, 2 panik, 3 test
unsigned long dfplayer_ready_counter = 0, time_last_playing = 0;

// Pins
const byte pled1 = 21,
           pled2 = 22,
           prelay = 23,
           pbusy = 4,
           ppanic = 34,
           ptest = 35;

BLEScan *pBLEScan;
DFPlayerMini_Fast myMP3;
Ticker ticker;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    String _name = advertisedDevice.getName().c_str();
    int _rssi = advertisedDevice.getRSSI();
    String _address = advertisedDevice.getAddress().toString().c_str();
    _name.trim();
    if (_name == "iTAG")
    {
#if DEBUG_LEVEL
      Serial.println((String)_rssi + "\t" + _address + "\t" + _name);
#endif
      if (!foundITag)
      {
        nearestRSSI = _rssi;
      }
      else
      {
        if (_rssi > nearestRSSI)
        {
          nearestRSSI = _rssi;
        }
      }
      if (_rssi > -90)
      {
        if (!toggle_panic)
          if (playMusicDfplayer(1, false))
          {
            mode_dfplayer = 1;
            skip_one_counter = true;
          }
      }
      foundITag = true;
    }
  }
};

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);
  initPins();
  bleInit();
  initDfplayer();
  ticker.attach_ms(INTERVAL_LISTEN_BUTTON, pinListener);
}

void loop()
{
  scanDevice();
}

void bleInit()
{
#if DEBUG_LEVEL
  Serial.println("Scanning...");
#endif
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  // pBLEScan->setInterval(100);
  // pBLEScan->setWindow(99); // less or equal setInterval value
}

void scanDevice()
{
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  if (foundITag)
  {
#if DEBUG_LEVEL
    Serial.println("iTAG found!");
#endif
  }
  foundITag = false;
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
}

void initDfplayer()
{
  Serial2.begin(9600);
#if DEBUG_MP3
  myMP3.begin(Serial2, true);
#else
  myMP3.begin(Serial2);
#endif
  delay(1000);
#if DEBUG_LEVEL
  Serial.println("Setting volume to max");
#endif
  myMP3.volume(30);
#if DEBUG_LEVEL
  Serial.println("Looping track 1");
#endif
}

bool playMusicDfplayer(int music, bool force)
{
  if (force)
  {
#if DEBUG_LEVEL
    Serial.println("Force play " + String(music));
#endif
    myMP3.playFromMP3Folder(music);
    // time_last_playing = millis();
    return true;
  }
  if (
      dfplayer_ready
      // && (millis() - time_last_playing > 1000)
  )
  {
#if DEBUG_LEVEL
    Serial.println("play " + String(music));
#endif
    myMP3.playFromMP3Folder(music);
    // time_last_playing = millis();
    return true;
  }
#if DEBUG_LEVEL
  Serial.println("dfplayer is not ready");
#endif
  return false;
}

void initPins()
{
  pinMode(ppanic, INPUT);
  pinMode(ptest, INPUT);
  pinMode(pbusy, INPUT);
  pinMode(pled1, OUTPUT);
  pinMode(pled2, OUTPUT);
  pinMode(prelay, OUTPUT);
}

void pinListener()
{
  btn_panic = digitalRead(ppanic);
  btn_test = digitalRead(ptest);
  busy = digitalRead(pbusy);

  btnAction();
  modeAction();
  busyAction();
  panicAction();
}

void panicAction()
{
  if (toggle_panic)
  {
    if (playMusicDfplayer(2, false))
    {
      mode_dfplayer = 2;
      skip_one_counter = true;
    }
  }
}

void btnAction()
{
  if (!btn_panic && !btn_panic_clicked)
  {
    btn_panic_clicked = true;
    btnPanicAction();
  }
  if (btn_panic && btn_panic_clicked)
  {
    btn_panic_clicked = false;
  }

  if (!btn_test && !btn_test_clicked)
  {
    btn_test_clicked = true;
    btnTestAction();
  }
  if (btn_test && btn_test_clicked)
  {
    btn_test_clicked = false;
  }
}

void btnTestAction()
{
  if (toggle_panic)
  {
    return;
  }
  if (playMusicDfplayer(1, false))
  {
    mode_dfplayer = 1;
    skip_one_counter = true;
  }
}

void btnPanicAction()
{
  toggle_panic = !toggle_panic;
  if (!toggle_panic)
  {
    return;
  }

  if (playMusicDfplayer(2, true))
  {
    mode_dfplayer = 2;
    skip_one_counter = true;
  }
}

void busyAction()
{
  // To fix fake ready from dfplayer
  // Fix pulse in pin busy
  if (busy)
  {
    dfplayer_ready_counter++;
  }
  else
  {
    dfplayer_ready_counter = 0;
  }

  if (dfplayer_ready_counter > DFPLAYER_READY_WHEN_COUNTER)
  {
    if (skip_one_counter) // bug fix back to mode 0 after click panic btn
    {
      skip_one_counter = false;
      return;
    }

    dfplayer_ready = true;
    mode_dfplayer = 0;
    return;
  }
  dfplayer_ready = false;
}

void modeAction()
{
#if DEBUG_LEVEL
  Serial.print("counter: ");
  Serial.print(dfplayer_ready_counter);
  Serial.print("\tmode: ");
  Serial.println(mode_dfplayer);
#endif
  if (toggle_panic)
  {
    mode_dfplayer = 2;
  }

  switch (mode_dfplayer)
  {
  case 3: // Test Mode
  case 1: // Terdeteksi Mode
    digitalWrite(pled1, HIGH);
    digitalWrite(pled2, LOW);
    digitalWrite(prelay, LOW);
    break;
  case 2: // Panik Mode
    digitalWrite(pled1, LOW);
    digitalWrite(pled2, HIGH);
    digitalWrite(prelay, HIGH);
    break;

  default:
    digitalWrite(pled2, LOW);
    digitalWrite(pled1, LOW);
    digitalWrite(prelay, LOW);
    break;
  }
}