#include "math_lib.h"
#include <cmath>

Value b_abs(const std::vector<Value>& a, Env*) {
  if (a.empty()) return Value::Int(0);
  const Value& v = a[0];
  if (v.type == V_INT) {
    return Value::Int(v.i < 0 ? -v.i : v.i);
  }
  if (v.type == V_FLOAT) {
    return Value::Float(v.f < 0.0 ? -v.f : v.f);
  }
  return Value::Nil();
}

Value b_sqrt(const std::vector<Value>& a, Env*) {
  if (a.empty()) return Value::Int(0);
  const Value& v = a[0];
  if (v.type == V_INT && v.i >= 0) {
    return Value::Float(sqrt(v.i));
  }
  if (v.type == V_FLOAT && v.f >= 0.0) {
    return Value::Float(sqrt(v.f));
  }
  return Value::Nil();
}

Value b_pow(const std::vector<Value>& args, Env*) {
  if (args.size() < 2) return Value::Float(0.0);
  double base = (args[0].type == V_INT) ? args[0].i : args[0].f;
  double exp = (args[1].type == V_INT) ? args[1].i : args[1].f;
  return Value::Float(pow(base, exp));
}

Value b_min(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Nil();
  double minVal = (args[0].type == V_INT) ? args[0].i : args[0].f;
  for (size_t i = 1; i < args.size(); i++) {
    double val = (args[i].type == V_INT) ? args[i].i : args[i].f;
    if (val < minVal) minVal = val;
  }
  return Value::Float(minVal);
}

Value b_max(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Nil();
  double maxVal = (args[0].type == V_INT) ? args[0].i : args[0].f;
  for (size_t i = 1; i < args.size(); i++) {
    double val = (args[i].type == V_INT) ? args[i].i : args[i].f;
    if (val > maxVal) maxVal = val;
  }
  return Value::Float(maxVal);
}

Value b_sin(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Float(0.0);
  double val = (args[0].type == V_INT) ? args[0].i : args[0].f;
  return Value::Float(sin(val));
}

Value b_cos(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Float(0.0);
  double val = (args[0].type == V_INT) ? args[0].i : args[0].f;
  return Value::Float(cos(val));
}

Value b_tan(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Float(0.0);
  double val = (args[0].type == V_INT) ? args[0].i : args[0].f;
  return Value::Float(tan(val));
}

Value b_asin(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Float(0.0);
  double val = (args[0].type == V_INT) ? args[0].i : args[0].f;
  return Value::Float(asin(val));
}

Value b_acos(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Float(0.0);
  double val = (args[0].type == V_INT) ? args[0].i : args[0].f;
  return Value::Float(acos(val));
}

Value b_atan(const std::vector<Value>& args, Env*) {
  if (args.empty()) return Value::Float(0.0);
  double val = (args[0].type == V_INT) ? args[0].i : args[0].f;
  return Value::Float(atan(val));
}

void load_math_lib(Env* env) {
  env->define("sqrt", Value::Func(b_sqrt));
  env->define("abs", Value::Func(b_abs));
  env->define("pow", Value::Func(b_pow));
  env->define("min", Value::Func(b_min));
  env->define("max", Value::Func(b_max));
  env->define("sin", Value::Func(b_sin));
  env->define("cos", Value::Func(b_cos));
  env->define("tan", Value::Func(b_tan));
  env->define("arcsin", Value::Func(b_asin));
  env->define("arccos", Value::Func(b_acos));
  env->define("arctan", Value::Func(b_atan));
  env->define("pi", Value::Float(M_PI));
  env->define("e", Value::Float(M_E));
}