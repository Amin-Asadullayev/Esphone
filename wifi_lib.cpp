#include "wifi_lib.h"
#include <WiFi.h>
#include <Arduino.h>

Value b_wifi_connect(const std::vector<Value>& args, Env*) {
  if (args.size() < 2 || args[0].type != V_STRING || args[1].type != V_STRING)
    return Value::Int(0);

  const char* ssid = args[0].str.c_str();
  const char* pass = args[1].str.c_str();

  WiFi.begin(ssid, pass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 50) {
    delay(100);
    attempts++;
  }

  return Value::Int(WiFi.status() == WL_CONNECTED ? 1 : 0);
}

Value b_wifi_disconnect(const std::vector<Value>&, Env*) {
  WiFi.disconnect();
  return Value::Nil();
}

Value b_wifi_status(const std::vector<Value>&, Env*) {
  return Value::Int(WiFi.status());
}

Value b_wifi_ip(const std::vector<Value>&, Env*) {
  if (WiFi.status() != WL_CONNECTED) return Value::String("");
  IPAddress ip = WiFi.localIP();
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return Value::String(buf);
}

Value b_wifi_scan(const std::vector<Value>&, Env*) {
  int n = WiFi.scanNetworks();
  std::vector<Value> list;
  for (int i = 0; i < n; i++) {
    list.push_back(Value::String(WiFi.SSID(i).c_str()));
  }
  return Value::List(list);
}

void load_wifi_lib(Env* env) {
  env->define("connect", Value::Func(b_wifi_connect));
  env->define("disconnect", Value::Func(b_wifi_disconnect));
  env->define("status", Value::Func(b_wifi_status));
  env->define("ip", Value::Func(b_wifi_ip));
  env->define("scan", Value::Func(b_wifi_scan));
}