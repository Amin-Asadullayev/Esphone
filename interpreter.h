#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <vector>
#include <map>
#include <set>
#include <string>
#include <cmath>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>

extern Adafruit_ST7735 tft;
extern int cursorX;
extern int cursorY;
extern SemaphoreHandle_t termMutex;

void termPutChar(char c, uint16_t color = ST77XX_WHITE);
void termBackspace();

enum ValueType {
  V_INT,
  V_FLOAT,
  V_STRING,
  V_SYMBOL,
  V_LIST,
  V_FUNC,
  V_LAMBDA,
  V_NIL
};

struct Env;
struct Value;
using BuiltinFn = Value (*)(const std::vector<Value>&, Env*);

struct Lambda {
  std::vector<std::string> params;
  Value* body;
  Env* env;
};

struct Value {
  ValueType type = V_NIL;
  int i = 0;
  double f = 0.0f;
  std::string str;
  std::vector<Value> list;
  BuiltinFn fn = nullptr;
  Lambda* lambda = nullptr;
  Env* lib_env = nullptr;

  static Value Int(int v);
  static Value Float(double v);
  static Value String(const std::string& s);
  static Value Symbol(const std::string& s);
  static Value List(const std::vector<Value>& v);
  static Value Func(BuiltinFn f);
  static Value Nil();
};

using LibLoader = void (*)(Env*);
extern std::map<std::string, LibLoader> builtin_libs;

struct Env {
  Env* parent;
  std::map<std::string, Value> vars;
  std::set<std::string>* loaded_libs;

  Env(Env* p = nullptr);
  bool get(const std::string& k, Value& out);
  bool set_existing(const std::string& k, const Value& v);
  void define(const std::string& k, const Value& v);
};

struct Parser {
  const char* src;
  Parser(const char* s);
  void skip();
  bool eof();
  Value parse();
};

Value eval(Value expr, Env* env);
Value apply(Value fn, const std::vector<Value>& args, Env* env);

struct Lesp {
  Env global;
  Lesp();
  void run_script(const char* src);
};

void load_core_lib(Env* env);
void init_builtin_libs();
Value b_println(const std::vector<Value>& a, Env*);

#endif