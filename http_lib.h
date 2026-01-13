#ifndef HTTP_LIB_H
#define HTTP_LIB_H

#include "interpreter.h"

struct HttpState {
  int status = 0;
  String response;
};

extern HttpState httpState;

void load_http_lib(Env* env);

#endif