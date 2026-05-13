#include "html_document.h"
#include "html_element.h"

#include <cstring>
#include <string>

namespace html {

Napi::FunctionReference Document::constructor;

namespace {

bool GetStringArg(const Napi::CallbackInfo& info, size_t index,
                  const char *name, std::string *value) {
  Napi::Env env = info.Env();
  if (info.Length() <= index || !info[index].IsString()) {
    Napi::TypeError::New(env, std::string(name) + " must be a string")
        .ThrowAsJavaScriptException();
    return false;
  }
  *value = info[index].As<Napi::String>().Utf8Value();
  return true;
}

bool HasDocument(const Napi::CallbackInfo& info, Document *doc) {
  if (doc->xdoc_wrapper_ && doc->xdoc_wrapper_->xdoc) {
    return true;
  }
  Napi::Error::New(info.Env(), "Invalid document").ThrowAsJavaScriptException();
  return false;
}

class QueryHandle {
public:
  explicit QueryHandle(const char *selector)
      : query_(selector ? xnode_query_create(selector, NULL, NULL) : nullptr) {}

  ~QueryHandle() {
    if (query_) {
      xnode_query_free(query_);
    }
  }

  XNodeQuery *get() const { return query_; }

private:
  XNodeQuery *query_;
};

} // namespace

Document::Document(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Document>(info), xdoc_wrapper_(nullptr) {

  std::string html;
  if (!GetStringArg(info, 0, "html", &html)) {
    return;
  }
  XNodeDocument *xdoc = xnode_parse_html(html.c_str(), html.size());
  xdoc_wrapper_ = new XDocWrapper(xdoc);
}

Document::~Document() {
  if (xdoc_wrapper_ && (--xdoc_wrapper_->ref_count == 0)) {
    delete xdoc_wrapper_;
  }
}

Napi::Value Document::GetDocumentElement(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);
  return Element::Create(env, xdoc->xdoc_wrapper_, root);
}

void Document::Init(Napi::Env env) {
  Napi::Function func = DefineClass(env, "Document", {
    InstanceMethod("find", &Document::Find),
    InstanceMethod("first", &Document::First),
    InstanceMethod("first_s", &Document::FirstSafe),
    InstanceMethod("only", &Document::Only),
    InstanceMethod("only_s", &Document::OnlySafe),
    InstanceAccessor("documentElement", &Document::GetDocumentElement, nullptr),

    InstanceMethod("firstOrThrow", &Document::FirstOrThrow),
    InstanceMethod("onlyOrThrow", &Document::OnlyOrThrow),
    InstanceMethod("text", &Document::Text),
    InstanceMethod("textOrThrow", &Document::TextOrThrow),
    InstanceMethod("exists", &Document::Exists),
    InstanceMethod("count", &Document::Count),
    InstanceMethod("attr", &Document::Attr2),
    InstanceMethod("attrOrThrow", &Document::AttrOrThrow2),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
}

Napi::Value Document::Find(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);
  return Element::Query(env, xdoc->xdoc_wrapper_, root, selector.c_str());
}

Napi::Value Document::First(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);
  Napi::Value value = Element::Query(env, xdoc->xdoc_wrapper_, root, selector.c_str());
  if (!value.IsArray()) {
    return env.Undefined();
  }
  Napi::Array array = value.As<Napi::Array>();
  if (array.Length() > 0) {
    return array.Get(uint32_t(0));
  }
  return env.Null();
}

Napi::Value Document::FirstSafe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);
  Napi::Value value = Element::Query(env, xdoc->xdoc_wrapper_, root, selector.c_str());
  if (!value.IsArray()) {
    return env.Undefined();
  }
  Napi::Array array = value.As<Napi::Array>();
  if (array.Length() > 0) {
    return array.Get(uint32_t(0));
  }
  Napi::Error::New(env, "No element found").ThrowAsJavaScriptException();
  return env.Undefined();
}

Napi::Value Document::Only(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);
  Napi::Value value = Element::Query(env, xdoc->xdoc_wrapper_, root, selector.c_str());
  if (!value.IsArray()) {
    return env.Undefined();
  }
  Napi::Array array = value.As<Napi::Array>();
  if (array.Length() == 1) {
    return array.Get(uint32_t(0));
  }
  return env.Null();
}

Napi::Value Document::OnlySafe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);
  Napi::Value value = Element::Query(env, xdoc->xdoc_wrapper_, root, selector.c_str());
  if (!value.IsArray()) {
    return env.Undefined();
  }
  Napi::Array array = value.As<Napi::Array>();
  if (array.Length() == 1) {
    return array.Get(uint32_t(0));
  }
  Napi::Error::New(env, "Not a single element").ThrowAsJavaScriptException();
  return env.Undefined();
}

Napi::Value Document::FirstOrThrow(const Napi::CallbackInfo& info) {
  return FirstSafe(info);
}

Napi::Value Document::OnlyOrThrow(const Napi::CallbackInfo& info) {
  return OnlySafe(info);
}

Napi::Value Document::Text(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), root);
  if (result && xnode_array_size(result) > 0) {
    XNode *node = xnode_array_get(result, 0);
    std::string text;
    if (info.Length() > 1 && info[1].IsObject()) {
      Napi::Object opts = info[1].As<Napi::Object>();
      if (opts.Has("separator") && opts.Get("separator").IsString()) {
        std::string sep = opts.Get("separator").As<Napi::String>();
        text = join_text_with_separator(node, sep);
      } else {
        text = get_inner_text(node);
      }
      if (opts.Has("normalize") && opts.Get("normalize").As<Napi::Boolean>()) {
        text = normalize_whitespace(text);
      }
    } else {
      text = get_inner_text(node);
    }
    return Napi::String::New(env, text);
  }

  return env.Null();
}

Napi::Value Document::TextOrThrow(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), root);
  if (result && xnode_array_size(result) > 0) {
    XNode *node = xnode_array_get(result, 0);
    return Napi::String::New(env, get_inner_text(node));
  }

  Napi::Error::New(env, "No element found").ThrowAsJavaScriptException();
  return env.Undefined();
}

Napi::Value Document::Exists(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), root);
  return Napi::Boolean::New(env, result && xnode_array_size(result) > 0);
}

Napi::Value Document::Count(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), root);
  return Napi::Number::New(env, result ? xnode_array_size(result) : 0);
}

Napi::Value Document::Attr2(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  std::string attrName;
  if (!GetStringArg(info, 1, "name", &attrName)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), root);
  if (result && xnode_array_size(result) > 0) {
    XNode *node = xnode_array_get(result, 0);
    const char *value = xnode_attr(node, attrName.c_str());
    if (value) {
      return Napi::String::New(env, value);
    }
  }

  return env.Undefined();
}

Napi::Value Document::AttrOrThrow2(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Document* xdoc = Document::Unwrap(info.This().As<Napi::Object>());
  if (!HasDocument(info, xdoc)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  std::string attrName;
  if (!GetStringArg(info, 1, "name", &attrName)) {
    return env.Undefined();
  }
  XNode *root = (XNode*) xnode_document_root(xdoc->xdoc_wrapper_->xdoc);

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), root);
  if (result && xnode_array_size(result) > 0) {
    XNode *node = xnode_array_get(result, 0);
    const char *value = xnode_attr(node, attrName.c_str());
    if (value) {
      return Napi::String::New(env, value);
    }
  }

  Napi::Error::New(env, "Attribute not found").ThrowAsJavaScriptException();
  return env.Undefined();
}

Napi::Value Document::Parse(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() == 0 || !info[0].IsString()) {
    Napi::TypeError::New(env, "html must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  return Document::constructor.New({ info[0] });
}

} // namespace html
