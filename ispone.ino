#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <cmath>
#include <set>
#include <string>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "interpreter.h"
#include "math_lib.h"
#include "sys_lib.h"
#include "fs_lib.h"
#include "wifi_lib.h"
#include "http_lib.h"

SemaphoreHandle_t termMutex;

#define TFT_CS 5
#define TFT_DC 22
#define TFT_RST 21
#define SD_CS 27
SPIClass hspi(HSPI);

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

String filename;
int cursorX = 0;
int cursorY = 0;
constexpr int CHAR_W = 6;
constexpr int CHAR_H = 8;
String inputBuffer;

void termPutChar(char c, uint16_t color) {
  xSemaphoreTake(termMutex, portMAX_DELAY);
  tft.setTextColor(color);
  if (c == '\n') {
    cursorX = 0;
    cursorY += CHAR_H;
  } else {
    tft.setCursor(cursorX, cursorY);
    tft.print(c);
    cursorX += CHAR_W;

    if (cursorX + CHAR_W > tft.width()) {
      cursorX = 0;
      cursorY += CHAR_H;
    }
  }

  if (cursorY + CHAR_H > tft.height()) {
    tft.fillScreen(ST77XX_BLACK);
    cursorX = 0;
    cursorY = 0;
  }

  xSemaphoreGive(termMutex);
}

void termBackspace() {
  xSemaphoreTake(termMutex, portMAX_DELAY);

  if (cursorX >= CHAR_W) {
    cursorX -= CHAR_W;
  } else if (cursorY >= CHAR_H) {
    cursorY -= CHAR_H;
    cursorX = tft.width() - CHAR_W;
  }

  tft.fillRect(cursorX, cursorY, CHAR_W, CHAR_H, ST77XX_BLACK);

  xSemaphoreGive(termMutex);
}

struct ScriptParam {
  char* src;
  std::vector<String> args;
};

void printShInit() {
  termPutChar('$', ST77XX_RED);
  termPutChar(' ', ST77XX_RED);
}

void runScriptTask(void* param) {
  ScriptParam* sp = (ScriptParam*)param;
  Lesp vm;

  load_core_lib(&vm.global);
  vm.global.loaded_libs->insert("core");

  vm.run_script(sp->src);

  Value mainFn;
  if (vm.global.get("main", mainFn) && mainFn.type == V_LAMBDA) {
    std::vector<Value> argValues;
    for (auto& s : sp->args)
      argValues.push_back(Value::String(s.c_str()));

    apply(mainFn, argValues, &vm.global);
  }

  free(sp->src);
  delete sp;

  printShInit();
  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);
  hspi.begin(14, 26, 13, SD_CS);

  if (!SD.begin(SD_CS, hspi, 2000000)) {
    Serial.println("SD.begin FAILED");
  } else {
    Serial.println("SD.begin OK");
  }

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  termMutex = xSemaphoreCreateMutex();
  
  init_builtin_libs();
  
  printShInit();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r' || c == '\n') {
      termPutChar('\n');

      if (inputBuffer.length()) {
        std::vector<String> parts;
        int start = 0;
        for (int i = 0; i <= inputBuffer.length(); i++) {
          if (i == inputBuffer.length() || inputBuffer[i] == ' ') {
            parts.push_back(inputBuffer.substring(start, i));
            start = i + 1;
          }
        }

        if (parts.empty()) return;

        String fileName = parts[0];
        std::vector<String> args(parts.begin() + 1, parts.end());
        Serial.println(fileName);
        String path = "/" + fileName + ".txt";
        Serial.println(path);
        File f = SD.open(path);
        if (f) {
          String src;
          while (f.available()) {
            char ch = f.read();
            if (ch != '\r') src += ch;
          }
          f.close();

          auto* sp = new ScriptParam;
          sp->src = strdup(src.c_str());
          sp->args = args;

          xTaskCreatePinnedToCore(
            runScriptTask,
            "ScriptTask",
            16000,
            sp,
            1,
            NULL,
            1);
        } else {
          b_println({ Value::String("File not found") }, nullptr);
          printShInit();
        }

        inputBuffer = "";
      }

    } else if (c == 8 || c == 127) {
      if (inputBuffer.length()) {
        inputBuffer.remove(inputBuffer.length() - 1);
        termBackspace();
      }
    } else {
      inputBuffer += c;
      termPutChar(c, 0x03E0);
    }
  }
}