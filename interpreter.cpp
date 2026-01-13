#include "interpreter.h"
#include "math_lib.h"
#include "sys_lib.h"
#include "fs_lib.h"
#include "wifi_lib.h"
#include "http_lib.h"
#include <cstdlib>


std::map<std::string, LibLoader> builtin_libs;

Value Value::Int(int v) {
  Value x;
  x.type = V_INT;
  x.i = v;
  return x;
}

Value Value::Float(double v) {
  Value x;
  x.type = V_FLOAT;
  x.f = v;
  return x;
}

Value Value::String(const std::string& s) {
  Value x;
  x.type = V_STRING;
  x.str = s;
  return x;
}

Value Value::Symbol(const std::string& s) {
  Value x;
  x.type = V_SYMBOL;
  x.str = s;
  return x;
}

Value Value::List(const std::vector<Value>& v) {
  Value x;
  x.type = V_LIST;
  x.list = v;
  return x;
}

Value Value::Func(BuiltinFn f) {
  Value x;
  x.type = V_FUNC;
  x.fn = f;
  return x;
}

Value Value::Nil() {
  return Value();
}

Env::Env(Env* p) : parent(p) {
  if (parent)
    loaded_libs = parent->loaded_libs;
  else
    loaded_libs = new std::set<std::string>();
}

bool Env::get(const std::string& k, Value& out) {
  if (vars.count(k)) {
    out = vars[k];
    return true;
  }
  if (parent) return parent->get(k, out);
  return false;
}

bool Env::set_existing(const std::string& k, const Value& v) {
  if (vars.count(k)) {
    vars[k] = v;
    return true;
  }
  if (parent) return parent->set_existing(k, v);
  return false;
}

void Env::define(const std::string& k, const Value& v) {
  vars[k] = v;
}

Parser::Parser(const char* s) : src(s) {}

void Parser::skip() {
  while (*src == ' ' || *src == '\n' || *src == '\t') src++;
}

bool Parser::eof() {
  skip();
  return *src == 0;
}

Value Parser::parse() {
  skip();
  if (*src == '(') {
    src++;
    std::vector<Value> items;
    while (true) {
      skip();
      if (*src == ')') {
        src++;
        break;
      }
      items.push_back(parse());
    }
    return Value::List(items);
  }

  if (*src == '"') {
    src++;
    const char* start = src;
    while (*src && *src != '"') src++;
    std::string s(start, src);
    if (*src == '"') src++;
    return Value::String(s);
  }

  if ((*src >= '0' && *src <= '9') || (*src == '-' && src[1] >= '0')) {
    char* endptr;
    double val = strtod(src, &endptr);
    src = endptr;
    if (std::floor(val) == val)
      return Value::Int((int)val);
    else
      return Value::Float(val);
  }

  const char* start = src;
  while (*src && *src != ' ' && *src != '\n' && *src != ')') src++;
  return Value::Symbol(std::string(start, src));
}

Value apply(Value fn, const std::vector<Value>& args, Env* env) {
  if (fn.type == V_FUNC) return fn.fn(args, env);
  if (fn.type == V_LAMBDA) {
    Env local(fn.lambda->env);
    for (size_t i = 0; i < fn.lambda->params.size(); i++)
      local.define(fn.lambda->params[i], args[i]);
    return eval(*fn.lambda->body, &local);
  }
  return Value::Nil();
}

Value eval(Value expr, Env* env) {
  switch (expr.type) {
    case V_INT:
    case V_FLOAT:
    case V_STRING:
    case V_FUNC:
    case V_LAMBDA: return expr;
    case V_SYMBOL:
      {
        size_t dot = expr.str.find('.');
        if (dot != std::string::npos) {
          std::string lib = expr.str.substr(0, dot);
          std::string sym = expr.str.substr(dot + 1);

          Value libVal;
          if (!env->get(lib, libVal)) return Value::Nil();
          if (!libVal.lib_env) return Value::Nil();

          Value out;
          if (libVal.lib_env->get(sym, out)) return out;
          return Value::Nil();
        }

        Value out;
        if (env->get(expr.str, out)) return out;
        return Value::Nil();
      }

    case V_LIST:
      {
        if (expr.list.empty()) return Value::Nil();
        Value head = expr.list[0];

        if (head.type == V_SYMBOL && head.str == "def") {
          Value v = eval(expr.list[2], env);
          env->define(expr.list[1].str, v);
          return v;
        }

        if (head.type == V_SYMBOL && head.str == "set!") {
          Value v = eval(expr.list[2], env);
          env->set_existing(expr.list[1].str, v);
          return v;
        }

        if (head.type == V_SYMBOL && head.str == "begin") {
          Env local(env);
          Value r = Value::Nil();
          for (size_t i = 1; i < expr.list.size(); i++)
            r = eval(expr.list[i], &local);
          return r;
        }

        if (head.type == V_SYMBOL && head.str == "if") {
          return eval(expr.list[1], env).i ? eval(expr.list[2], env) : eval(expr.list[3], env);
        }

        if (head.type == V_SYMBOL && head.str == "while") {
          Value r = Value::Nil();
          while (eval(expr.list[1], env).i) r = eval(expr.list[2], env);
          return r;
        }

        if (head.type == V_SYMBOL && head.str == "lambda") {
          Lambda* l = new Lambda();
          for (auto& p : expr.list[1].list) l->params.push_back(p.str);
          l->body = new Value(expr.list[2]);
          l->env = env;
          Value v;
          v.type = V_LAMBDA;
          v.lambda = l;
          return v;
        }

        if (head.type == V_SYMBOL && head.str == "include") {
          if (expr.list.size() < 2) return Value::Nil();

          Value lib = expr.list[1];
          if (lib.type != V_SYMBOL) return Value::Nil();
          const std::string& name = lib.str;

          if (env->loaded_libs->count(name)) return Value::Nil();
          env->loaded_libs->insert(name);

          if (name == "core") return Value::Nil();

          if (builtin_libs.count(name)) {
            Env* lib_env = new Env(env);
            builtin_libs[name](lib_env);

            Value libValue;
            libValue.type = V_SYMBOL;
            libValue.str = name;
            libValue.lib_env = lib_env;

            env->define(name, libValue);
            return Value::Nil();
          }

          std::string path = "/" + name + ".txt";
          File f = SD.open(path.c_str());
          if (!f) return Value::Nil();

          String src;
          while (f.available()) {
            char c = f.read();
            if (c != '\r') src += c;
          }
          f.close();

          Env* lib_env = new Env(env);
          Parser p(src.c_str());
          while (!p.eof()) eval(p.parse(), lib_env);

          Value libValue;
          libValue.type = V_SYMBOL;
          libValue.str = name;
          libValue.lib_env = lib_env;

          env->define(name, libValue);
          return Value::Nil();
        }
        Value fn = eval(head, env);
        std::vector<Value> args;
        for (size_t i = 1; i < expr.list.size(); i++)
          args.push_back(eval(expr.list[i], env));
        return apply(fn, args, env);
      }
    default: return Value::Nil();
  }
}

Lesp::Lesp() {}

void Lesp::run_script(const char* src) {
  Parser p(src);
  while (!p.eof()) eval(p.parse(), &global);
}


Value b_add(const std::vector<Value>& a, Env*) {
  bool is_float = false;
  double sum = 0;
  for (auto& v : a) {
    if (v.type == V_FLOAT) is_float = true;
    sum += (v.type == V_INT) ? v.i : v.f;
  }
  return is_float ? Value::Float(sum) : Value::Int((int)sum);
}

Value b_sub(const std::vector<Value>& a, Env*) {
  if (a.empty()) return Value::Int(0);
  bool is_float = false;
  double r = (a[0].type == V_INT) ? a[0].i : a[0].f;
  if (a[0].type == V_FLOAT) is_float = true;
  for (size_t i = 1; i < a.size(); i++) {
    if (a[i].type == V_FLOAT) is_float = true;
    r -= (a[i].type == V_INT) ? a[i].i : a[i].f;
  }
  return is_float ? Value::Float(r) : Value::Int((int)r);
}

Value b_mul(const std::vector<Value>& a, Env*) {
  bool is_float = false;
  double r = 1;
  for (auto& v : a) {
    if (v.type == V_FLOAT) is_float = true;
    r *= (v.type == V_INT) ? v.i : v.f;
  }
  return is_float ? Value::Float(r) : Value::Int((int)r);
}

Value b_div(const std::vector<Value>& a, Env*) {
  if (a.empty()) return Value::Int(0);
  double r = (a[0].type == V_INT) ? a[0].i : a[0].f;
  for (size_t i = 1; i < a.size(); i++)
    r /= (a[i].type == V_INT) ? a[i].i : a[i].f;
  return Value::Float(r);
}

Value b_lt(const std::vector<Value>& a, Env*) {
  double x = (a[0].type == V_INT) ? a[0].i : a[0].f;
  double y = (a[1].type == V_INT) ? a[1].i : a[1].f;
  return Value::Int(x < y);
}

Value b_lte(const std::vector<Value>& a, Env*) {
  double x = (a[0].type == V_INT) ? a[0].i : a[0].f;
  double y = (a[1].type == V_INT) ? a[1].i : a[1].f;
  return Value::Int(x <= y);
}

Value b_gte(const std::vector<Value>& a, Env*) {
  double x = (a[0].type == V_INT) ? a[0].i : a[0].f;
  double y = (a[1].type == V_INT) ? a[1].i : a[1].f;
  return Value::Int(x >= y);
}

Value b_eq(const std::vector<Value>& a, Env*) {
  if (a.size() < 2) return Value::Int(0);
  if (a[0].type != a[1].type) return Value::Int(0);
  if (a[0].type == V_INT) return Value::Int(a[0].i == a[1].i);
  if (a[0].type == V_FLOAT) return Value::Int(a[0].f == a[1].f);
  if (a[0].type == V_STRING) return Value::Int(a[0].str == a[1].str);
  return Value::Int(0);
}

Value b_println(const std::vector<Value>& a, Env*) {
  for (auto& v : a) {
    if (v.type == V_INT) {
      String s = String(v.i);
      for (char c : s) termPutChar(c);
    } else if (v.type == V_FLOAT) {
      String s = String(v.f);
      for (char c : s) termPutChar(c);
    } else if (v.type == V_STRING) {
      for (char c : v.str) termPutChar(c);
    }
  }
  termPutChar('\n');
  return Value::Nil();
}

Value b_print(const std::vector<Value>& a, Env*) {
  for (auto& v : a) {
    if (v.type == V_INT) {
      String s = String(v.i);
      for (char c : s) termPutChar(c);
    } else if (v.type == V_FLOAT) {
      String s = String(v.f);
      for (char c : s) termPutChar(c);
    } else if (v.type == V_STRING) {
      for (char c : v.str) termPutChar(c);
    }
  }
  return Value::Nil();
}

Value b_parse_int(const std::vector<Value>& args, Env*) {
  if (args.empty() || args[0].type != V_STRING)
    return Value::Int(0);
  return Value::Int(atoi(args[0].str.c_str()));
}

Value b_float(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Float(0.0);
  if (args[0].type == V_INT) return Value::Float((double)args[0].i);
  if (args[0].type == V_FLOAT) return args[0];
  if (args[0].type == V_STRING) return Value::Float(atof(args[0].str.c_str()));
  return Value::Float(0.0);
}

Value b_string(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::String("");
  if (args[0].type == V_STRING) return args[0];
  if (args[0].type == V_INT) return Value::String(String(args[0].i).c_str());
  if (args[0].type == V_FLOAT) return Value::String(String(args[0].f).c_str());
  return Value::String("");
}

Value b_type(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::String("nil");
  switch (args[0].type) {
    case V_INT: return Value::String("int");
    case V_FLOAT: return Value::String("float");
    case V_STRING: return Value::String("string");
    case V_LIST: return Value::String("list");
    case V_FUNC:
    case V_LAMBDA: return Value::String("function");
    default: return Value::String("nil");
  }
}

Value b_not(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Int(1);
  return Value::Int(!args[0].i);
}

Value b_and(const std::vector<Value>& args, Env*) {
  for (auto& v : args) {
    if (!v.i) return Value::Int(0);
  }
  return Value::Int(1);
}

Value b_or(const std::vector<Value>& args, Env*) {
  for (auto& v : args) {
    if (v.i) return Value::Int(1);
  }
  return Value::Int(0);
}

Value b_get(const std::vector<Value>& args, Env*) {
  if (args.size() < 2) return Value::Nil();
  const Value& arr = args[0];
  int idx = args[1].i;
  if (arr.type == V_LIST && idx >= 0 && idx < (int)arr.list.size())
    return arr.list[idx];
  return Value::Nil();
}

Value b_list(const std::vector<Value>& args, Env*) {
  return Value::List(args);
}

Value b_set(const std::vector<Value>& args, Env*) {
  if (args.size() < 3) return Value::Nil();
  Value arr = args[0];
  if (arr.type != V_LIST) return Value::Nil();
  int idx = args[1].i;
  if (idx >= 0 && idx < (int)arr.list.size()) arr.list[idx] = args[2];
  return arr;
}

Value b_len(const std::vector<Value>& args, Env*) {
  if (args.empty() || args[0].type != V_LIST) return Value::Int(0);
  return Value::Int(args[0].list.size());
}

Value b_push(const std::vector<Value>& args, Env*) {
  if (args.size() < 2 || args[0].type != V_LIST) return Value::Nil();
  Value arr = args[0];
  arr.list.push_back(args[1]);
  return arr;
}

Value b_pop(const std::vector<Value>& args, Env*) {
  if (args.empty() || args[0].type != V_LIST || args[0].list.empty())
    return Value::Nil();
  Value arr = args[0];
  arr.list.pop_back();
  return arr;
}

Value b_slice(const std::vector<Value>& args, Env*) {
  if (args.size() < 3 || args[0].type != V_LIST) return Value::Nil();
  int start = args[1].i;
  int end = args[2].i;
  const auto& list = args[0].list;
  if (start < 0) start = 0;
  if (end > (int)list.size()) end = list.size();
  std::vector<Value> result(list.begin() + start, list.begin() + end);
  return Value::List(result);
}

Value b_strlen(const std::vector<Value>& args, Env*) {
  if (args.empty() || args[0].type != V_STRING) return Value::Int(0);
  return Value::Int(args[0].str.length());
}

Value b_concat(const std::vector<Value>& args, Env*) {
  std::string result;
  for (auto& v : args) {
    if (v.type == V_STRING) result += v.str;
  }
  return Value::String(result);
}

Value b_substr(const std::vector<Value>& args, Env*) {
  if (args.size() < 3 || args[0].type != V_STRING) return Value::String("");
  int start = args[1].i;
  int len = args[2].i;
  return Value::String(args[0].str.substr(start, len));
}

Value b_charAt(const std::vector<Value>& args, Env*) {
  if (args.size() < 2 || args[0].type != V_STRING) return Value::String("");
  int idx = args[1].i;
  if (idx >= 0 && idx < (int)args[0].str.length()) {
    return Value::String(std::string(1, args[0].str[idx]));
  }
  return Value::String("");
}

Value b_split(const std::vector<Value>& args, Env*) {
  if (args.size() < 2 || args[0].type != V_STRING || args[1].type != V_STRING)
    return Value::List({});

  std::vector<Value> result;
  std::string str = args[0].str;
  std::string delim = args[1].str;

  size_t start = 0;
  size_t end = str.find(delim);

  while (end != std::string::npos) {
    result.push_back(Value::String(str.substr(start, end - start)));
    start = end + delim.length();
    end = str.find(delim, start);
  }
  result.push_back(Value::String(str.substr(start)));

  return Value::List(result);
}

void load_core_lib(Env* env) {
  env->define("+", Value::Func(b_add));
  env->define("-", Value::Func(b_sub));
  env->define("*", Value::Func(b_mul));
  env->define("/", Value::Func(b_div));
  env->define("<", Value::Func(b_lt));
  env->define("<=", Value::Func(b_lte));
  env->define(">=", Value::Func(b_gte));
  env->define("=", Value::Func(b_eq));
  env->define("int", Value::Func(b_parse_int));
  env->define("float", Value::Func(b_float));
  env->define("string", Value::Func(b_string));
  env->define("type", Value::Func(b_type));
  env->define("not", Value::Func(b_not));
  env->define("and", Value::Func(b_and));
  env->define("or", Value::Func(b_or));
  env->define("list", Value::Func(b_list));
  env->define("get", Value::Func(b_get));
  env->define("set", Value::Func(b_set));
  env->define("len", Value::Func(b_len));
  env->define("push", Value::Func(b_push));
  env->define("pop", Value::Func(b_pop));
  env->define("slice", Value::Func(b_slice));
  env->define("strlen", Value::Func(b_strlen));
  env->define("concat", Value::Func(b_concat));
  env->define("substr", Value::Func(b_substr));
  env->define("charAt", Value::Func(b_charAt));
  env->define("split", Value::Func(b_split));
  env->define("print", Value::Func(b_print));
  env->define("println", Value::Func(b_println));
}


void init_builtin_libs() {
  builtin_libs["math"] = load_math_lib;
  builtin_libs["sys"] = load_sys_lib;
  builtin_libs["fs"] = load_fs_lib;
  builtin_libs["wifi"] = load_wifi_lib;
  builtin_libs["http"] = load_http_lib;
}