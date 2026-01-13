#include "http_lib.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <SD.h>
#include <Arduino.h>

HttpState httpState;

Value b_http_get(const std::vector<Value>& a, Env*) {
  if (a.empty() || a[0].type != V_STRING)
    return Value::String("");

  const String url = a[0].str.c_str();
  HTTPClient http;
  bool https = url.startsWith("https://");

  if (https) {
    WiFiClientSecure* client = new WiFiClientSecure();
    client->setInsecure();

    if (!http.begin(*client, url)) {
      delete client;
      return Value::String("");
    }
  } else {
    WiFiClient client;
    if (!http.begin(client, url))
      return Value::String("");
  }

  int code = http.GET();
  httpState.status = code;

  if (code <= 0) {
    http.end();
    return Value::String("");
  }

  String payload = http.getString();
  httpState.response = payload;
  http.end();
  return Value::String(payload.c_str());
}

Value b_http_get_file(const std::vector<Value>& a, Env*) {
  if (a.size() < 2 || a[0].type != V_STRING || a[1].type != V_STRING)
    return Value::Int(0);

  const String url = a[0].str.c_str();
  const String path = a[1].str.c_str();

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  if (!http.begin(client, url))
    return Value::Int(0);

  int code = http.GET();
  httpState.status = code;

  if (code != HTTP_CODE_OK) {
    http.end();
    return Value::Int(0);
  }

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    http.end();
    return Value::Int(0);
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[256];

  int remaining = http.getSize();

  while (http.connected() && (remaining > 0 || remaining == -1)) {
    size_t avail = stream->available();
    if (avail) {
      Serial.print(5);
      int read = stream->readBytes(buf, min(avail, sizeof(buf)));
      f.write(buf, read);
      if (remaining > 0) remaining -= read;
    }
    delay(1);
  }

  f.close();
  http.end();
  return Value::Int(1);
}

Value b_http_status(const std::vector<Value>&, Env*) {
  return Value::Int(httpState.status);
}

Value b_http_response(const std::vector<Value>&, Env*) {
  return Value::String(httpState.response.c_str());
}

void load_http_lib(Env* env) {
  env->define("get", Value::Func(b_http_get));
  env->define("get-file", Value::Func(b_http_get_file));
  env->define("status", Value::Func(b_http_status));
  env->define("response", Value::Func(b_http_response));
}