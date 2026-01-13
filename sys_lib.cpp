#include "sys_lib.h"
#include <Arduino.h>
#include <Adafruit_ST7735.h>

extern Adafruit_ST7735 tft;
extern int cursorX;
extern int cursorY;

Value b_cls(const std::vector<Value>&, Env*) {
  tft.fillScreen(ST77XX_BLACK);
  cursorX = 0;
  cursorY = 0;
  return Value::Nil();
}

Value b_sys_time(const std::vector<Value>&, Env*) {
  return Value::Int(millis());
}

Value b_sys_delay(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Nil();
  int ms = (args[0].type == V_INT) ? args[0].i : (int)args[0].f;
  delay(ms);
  return Value::Nil();
}

Value b_sys_exit(const std::vector<Value>&, Env*) {
  vTaskDelete(NULL);
  return Value::Nil();
}

void load_sys_lib(Env* env) {
  env->define("cls", Value::Func(b_cls));
  env->define("time", Value::Func(b_sys_time));
  env->define("delay", Value::Func(b_sys_delay));
  env->define("exit", Value::Func(b_sys_exit));
}