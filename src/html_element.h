#ifndef HTML_ELEMENT_H_
#define HTML_ELEMENT_H_

#include <napi.h>

#include <string>

#include "xnode_query.h"

namespace html {

class Document;
struct XDocWrapper;

class Element : public Napi::ObjectWrap<Element> {
public:
  static void Init(Napi::Env env);
  static Napi::FunctionReference constructor;

  static Napi::Value Query(Napi::Env env, XDocWrapper *wrapper,
                           const XNode *node, const char *selector);
  static Napi::Value Create(Napi::Env env, XDocWrapper *xdoc, XNode *xnode);
  Element(const Napi::CallbackInfo& info);
  ~Element();

  Napi::Value Attr(const Napi::CallbackInfo& info);
  Napi::Value AttrSafe(const Napi::CallbackInfo& info);
  Napi::Value Find(const Napi::CallbackInfo& info);
  Napi::Value First(const Napi::CallbackInfo& info);
  Napi::Value FirstSafe(const Napi::CallbackInfo& info);
  Napi::Value Only(const Napi::CallbackInfo& info);
  Napi::Value OnlySafe(const Napi::CallbackInfo& info);
  Napi::Value HasClass(const Napi::CallbackInfo& info);
  Napi::Value HasAttribute(const Napi::CallbackInfo& info);
  Napi::Value Next(const Napi::CallbackInfo& info);
  Napi::Value Prev(const Napi::CallbackInfo& info);

  Napi::Value GetTagName(const Napi::CallbackInfo& info);
  Napi::Value GetInnerText(const Napi::CallbackInfo& info);
  Napi::Value GetOuterHTML(const Napi::CallbackInfo& info);
  Napi::Value GetParent(const Napi::CallbackInfo& info);
  Napi::Value GetNodeType(const Napi::CallbackInfo& info);
  Napi::Value GetChildNodes(const Napi::CallbackInfo& info);

  Napi::Value FirstOrThrow(const Napi::CallbackInfo& info);
  Napi::Value OnlyOrThrow(const Napi::CallbackInfo& info);
  Napi::Value AttrOrThrow(const Napi::CallbackInfo& info);
  Napi::Value Text(const Napi::CallbackInfo& info);
  Napi::Value TextOrThrow(const Napi::CallbackInfo& info);
  Napi::Value Exists(const Napi::CallbackInfo& info);
  Napi::Value Count(const Napi::CallbackInfo& info);
  Napi::Value Closest(const Napi::CallbackInfo& info);
  Napi::Value Children(const Napi::CallbackInfo& info);
  Napi::Value Siblings(const Napi::CallbackInfo& info);
  Napi::Value Matches(const Napi::CallbackInfo& info);
  Napi::Value Is(const Napi::CallbackInfo& info);
  Napi::Value Rows(const Napi::CallbackInfo& info);
  Napi::Value GetTextContent(const Napi::CallbackInfo& info);

  XNode *xnode_;
  XDocWrapper *xdoc_;
};

std::string get_inner_text(GumboNode *node);
std::string normalize_whitespace(const std::string &s);
std::string join_text_with_separator(XNode *node, const std::string &sep);

}

#endif // HTML_ELEMENT_H_
