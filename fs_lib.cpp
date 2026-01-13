#include "fs_lib.h"
#include <SD.h>
#include <Arduino.h>

Value b_fs_exists(const std::vector<Value>& args, Env*) {
  if (args.empty() || args[0].type != V_STRING) return Value::Int(0);
  String path = "/" + String(args[0].str.c_str());
  return Value::Int(SD.exists(path.c_str()));
}

Value b_fs_read(const std::vector<Value>& args, Env*) {
  if (args.empty() || args[0].type != V_STRING) return Value::String("");
  String path = "/" + String(args[0].str.c_str());
  Serial.println(path);
  File f = SD.open(path.c_str());
  if (!f) Serial.println("okay");
  if (!f) return Value::String("");

  String content;
  while (f.available()) {
    char c = f.read();
    if (c != '\r') content += c;
  }
  f.close();
  return Value::String(content.c_str());
}

Value b_fs_write(const std::vector<Value>& args, Env*) {
  if (args.size() < 2 || args[0].type != V_STRING || args[1].type != V_STRING)
    return Value::Int(0);

  String path = "/" + String(args[0].str.c_str());
  File f = SD.open(path.c_str(), FILE_WRITE);
  if (!f) return Value::Int(0);

  f.print(args[1].str.c_str());
  f.close();
  return Value::Int(1);
}

Value b_fs_append(const std::vector<Value>& args, Env*) {
  if (args.size() < 2 || args[0].type != V_STRING || args[1].type != V_STRING)
    return Value::Int(0);

  String path = "/" + String(args[0].str.c_str());
  File f = SD.open(path.c_str(), FILE_APPEND);
  if (!f) return Value::Int(0);

  f.print(args[1].str.c_str());
  f.close();
  return Value::Int(1);
}

Value b_fs_remove(const std::vector<Value>& args, Env*) {
  if (args.empty() || args[0].type != V_STRING) return Value::Int(0);
  String path = "/" + String(args[0].str.c_str());
  return Value::Int(SD.remove(path.c_str()));
}

void load_fs_lib(Env* env) {
  env->define("exists", Value::Func(b_fs_exists));
  env->define("read", Value::Func(b_fs_read));
  env->define("write", Value::Func(b_fs_write));
  env->define("append", Value::Func(b_fs_append));
  env->define("remove", Value::Func(b_fs_remove));
}