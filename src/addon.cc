#include <napi.h>

#include "html_document.h"
#include "html_element.h"

Napi::Value Parse(const Napi::CallbackInfo& info) {
  return html::Document::Parse(info);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  html::Document::Init(env);
  html::Element::Init(env);
  exports.Set(Napi::String::New(env, "parse"), Napi::Function::New(env, Parse));
  exports.Set(Napi::String::New(env, "Document"), html::Document::constructor.Value());
  exports.Set(Napi::String::New(env, "Element"), html::Element::constructor.Value());
  return exports;
}

NODE_API_MODULE(addon, Init)
