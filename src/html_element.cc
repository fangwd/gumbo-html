#include "html_element.h"
#include "html_document.h"

#include <cstring>
#include <functional>
#include <vector>

namespace html {

Napi::FunctionReference Element::constructor;

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

bool GetOptionalStringArg(const Napi::CallbackInfo& info, size_t index,
                          const char *name, std::string *value,
                          bool *has_value) {
  if (info.Length() <= index || info[index].IsUndefined() || info[index].IsNull()) {
    *has_value = false;
    return true;
  }
  *has_value = true;
  return GetStringArg(info, index, name, value);
}

bool HasNode(const Napi::CallbackInfo& info, Element *element) {
  if (element->xnode_) {
    return true;
  }
  Napi::Error::New(info.Env(), "Invalid element").ThrowAsJavaScriptException();
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

bool MatchQueryAt(XNode *node, XNodeQuery *query, int index) {
  if (!node || !query || index < 0) {
    return false;
  }

  XNodeSelector *selector = xnode_query_selector(query, index);
  if (!xnode_match_node(node, selector)) {
    return false;
  }

  if (index == 0) {
    return true;
  }

  switch (selector->combinator) {
  case XQSEL_DESCENDANT: {
    for (XNode *parent = xnode_parent(node); parent; parent = xnode_parent(parent)) {
      if (MatchQueryAt(parent, query, index - 1)) {
        return true;
      }
    }
    return false;
  }
  case XQSEL_CHILD:
    return MatchQueryAt(xnode_parent(node), query, index - 1);
  case XQSEL_ADJACENT_SIBLING:
    return MatchQueryAt(xnode_prev(node, NULL), query, index - 1);
  case XQSEL_GENERAL_SIBLING:
    for (XNode *prev = xnode_prev(node, NULL); prev; prev = xnode_prev(prev, NULL)) {
      if (MatchQueryAt(prev, query, index - 1)) {
        return true;
      }
    }
    return false;
  default:
    return false;
  }
}

bool MatchQuery(XNode *node, XNodeQuery *query) {
  uint32_t count = jsa_size(&query->selectors);
  return count > 0 && MatchQueryAt(node, query, static_cast<int>(count - 1));
}

} // namespace

Element::Element(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Element>(info), xnode_(nullptr), xdoc_(nullptr) {
}

Element::~Element() {
  if (xdoc_ && --xdoc_->ref_count == 0) {
    delete xdoc_;
  }
}

std::string get_inner_text(GumboNode *node) {
  if (node->type == GUMBO_NODE_TEXT) {
    return node->v.text.text;
  } else if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
    std::string result;
    GumboVector *children = xnode_children(node);
    if (children) {
      for (unsigned i = 0; i < children->length; ++i) {
        GumboNode* child = (GumboNode*) children->data[i];
        result += get_inner_text(child);
      }
    }
    return result;
  }
  return "";
}

std::string normalize_whitespace(const std::string &s) {
  std::string result;
  result.reserve(s.size());
  bool inSpace = false;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
      if (!inSpace && !result.empty()) {
        result += ' ';
      }
      inSpace = true;
    } else {
      result += c;
      inSpace = false;
    }
  }
  if (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

std::string join_text_with_separator(XNode *node, const std::string &sep) {
  std::string result;
  std::function<void(XNode*)> walk = [&](XNode *n) {
    if (n->type == GUMBO_NODE_TEXT || n->type == GUMBO_NODE_WHITESPACE) {
      std::string t(n->v.text.text);
      size_t start = 0, end = t.size();
      while (start < end && (t[start] == ' ' || t[start] == '\t' || t[start] == '\n' || t[start] == '\r')) start++;
      while (end > start && (t[end-1] == ' ' || t[end-1] == '\t' || t[end-1] == '\n' || t[end-1] == '\r')) end--;
      if (start < end) {
        if (!result.empty()) result += sep;
        result += t.substr(start, end - start);
      }
    } else if (n->type == GUMBO_NODE_ELEMENT || n->type == GUMBO_NODE_TEMPLATE) {
      GumboVector *kids = xnode_children(n);
      if (kids) {
        for (unsigned i = 0; i < kids->length; i++) {
          walk(static_cast<XNode*>(kids->data[i]));
        }
      }
    }
  };
  walk(node);
  return result;
}

Napi::Value Element::GetTagName(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  if (element->xnode_->type == GUMBO_NODE_ELEMENT ||
      element->xnode_->type == GUMBO_NODE_TEMPLATE) {
    size_t len;
    const char *s = xnode_type(element->xnode_, &len);
    return Napi::String::New(env, s, len);
  }
  return env.Null();
}

Napi::Value Element::GetInnerText(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  if (element->xnode_->type == GUMBO_NODE_TEXT ||
      element->xnode_->type == GUMBO_NODE_ELEMENT ||
      element->xnode_->type == GUMBO_NODE_TEMPLATE) {
    std::string text = get_inner_text(element->xnode_);
    return Napi::String::New(env, text);
  }
  return env.Undefined();
}

Napi::Value Element::GetNodeType(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  const char *type;
  switch (element->xnode_->type) {
    case GUMBO_NODE_DOCUMENT:   type = "DOCUMENT";   break;
    case GUMBO_NODE_ELEMENT:    type = "ELEMENT";    break;
    case GUMBO_NODE_TEXT:       type = "TEXT";       break;
    case GUMBO_NODE_CDATA:      type = "CDATA";      break;
    case GUMBO_NODE_COMMENT:    type = "COMMENT";    break;
    case GUMBO_NODE_WHITESPACE: type = "WHITESPACE"; break;
    case GUMBO_NODE_TEMPLATE:   type = "TEMPLATE";   break;
    default:                    type = "UNKNOWN";     break;
  }
  return Napi::String::New(env, type);
}

Napi::Value Element::GetChildNodes(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element *element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  GumboNodeType type = element->xnode_->type;
  if ((type == GUMBO_NODE_ELEMENT) || (type == GUMBO_NODE_TEMPLATE)) {
    GumboVector* children = &element->xnode_->v.element.children;
    Napi::Array array = Napi::Array::New(env, children->length);
    for (unsigned int i = 0; i < children->length; ++i) {
      XNode* child = static_cast<XNode*> (children->data[i]);
      array.Set(i, Create(env, element->xdoc_, child));
    }
    return array;
  }
  return Napi::Array::New(env, 0);
}

Napi::Value Element::GetOuterHTML(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  size_t len;
  const char *s = xnode_html(element->xnode_, &len);
  return Napi::String::New(env, s, len);
}

Napi::Value Element::Create(Napi::Env env, XDocWrapper *xdoc, XNode *xnode) {
  if (xdoc && xnode) {
    Napi::EscapableHandleScope scope(env);
    Napi::Object obj = Element::constructor.New({});
    Element *element = Element::Unwrap(obj);
    element->xnode_ = xnode;
    element->xdoc_ = xdoc;
    xdoc->ref_count++;
    return scope.Escape(obj);
  }
  return env.Null();
}

Napi::Value Element::GetParent(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element *element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  XNode *parent = xnode_parent(element->xnode_);
  if (!parent || parent->type == GUMBO_NODE_DOCUMENT) {
    return env.Null();
  }
  return Create(env, element->xdoc_, parent);
}

void Element::Init(Napi::Env env) {
  Napi::Function func = DefineClass(env, "Element", {
    InstanceMethod("attr", &Element::Attr),
    InstanceMethod("attr_s", &Element::AttrSafe),
    InstanceMethod("find", &Element::Find),
    InstanceMethod("first", &Element::First),
    InstanceMethod("first_s", &Element::FirstSafe),
    InstanceMethod("only", &Element::Only),
    InstanceMethod("only_s", &Element::OnlySafe),
    InstanceMethod("hasClass", &Element::HasClass),
    InstanceMethod("hasAttribute", &Element::HasAttribute),
    InstanceMethod("next", &Element::Next),
    InstanceMethod("prev", &Element::Prev),
    InstanceAccessor("tagName", &Element::GetTagName, nullptr),
    InstanceAccessor("innerText", &Element::GetInnerText, nullptr),
    InstanceAccessor("outerHTML", &Element::GetOuterHTML, nullptr),
    InstanceAccessor("parent", &Element::GetParent, nullptr),
    InstanceAccessor("nodeType", &Element::GetNodeType, nullptr),
    InstanceAccessor("childNodes", &Element::GetChildNodes, nullptr),

    InstanceMethod("firstOrThrow", &Element::FirstOrThrow),
    InstanceMethod("onlyOrThrow", &Element::OnlyOrThrow),
    InstanceMethod("attrOrThrow", &Element::AttrOrThrow),
    InstanceMethod("text", &Element::Text),
    InstanceMethod("textOrThrow", &Element::TextOrThrow),
    InstanceMethod("exists", &Element::Exists),
    InstanceMethod("count", &Element::Count),
    InstanceMethod("closest", &Element::Closest),
    InstanceMethod("children", &Element::Children),
    InstanceMethod("siblings", &Element::Siblings),
    InstanceMethod("matches", &Element::Matches),
    InstanceMethod("is", &Element::Is),
    InstanceMethod("rows", &Element::Rows),
    InstanceAccessor("textContent", &Element::GetTextContent, nullptr),
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
}

Napi::Value Element::Attr(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() != 1) {
    Napi::Error::New(env, "Empty name.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::HandleScope scope(env);
  Element* element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  std::string name;
  if (!GetStringArg(info, 0, "name", &name)) {
    return env.Undefined();
  }
  const char *value = xnode_attr(element->xnode_, name.c_str());
  if (value) {
    return Napi::String::New(env, value);
  }
  return env.Undefined();
}

Napi::Value Element::AttrSafe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  if (info.Length() != 1) {
    Napi::Error::New(env, "Empty name.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::HandleScope scope(env);
  Element* element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  std::string name;
  if (!GetStringArg(info, 0, "name", &name)) {
    return env.Undefined();
  }
  const char *value = xnode_attr(element->xnode_, name.c_str());
  if (value) {
    return Napi::String::New(env, value);
  }
  Napi::Error::New(env, "Attribute not found").ThrowAsJavaScriptException();
  return env.Undefined();
}

Napi::Value Element::Find(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  return Element::Query(env, elem->xdoc_, elem->xnode_, selector.c_str());
}

Napi::Value Element::First(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  Napi::Value value = Element::Query(env, elem->xdoc_, elem->xnode_, selector.c_str());
  if (!value.IsArray()) {
    return env.Undefined();
  }
  Napi::Array array = value.As<Napi::Array>();
  if (array.Length() > 0) {
    return array.Get(uint32_t(0));
  }
  return env.Null();
}

Napi::Value Element::FirstSafe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  Napi::Value value = Element::Query(env, elem->xdoc_, elem->xnode_, selector.c_str());
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

Napi::Value Element::Only(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  Napi::Value value = Element::Query(env, elem->xdoc_, elem->xnode_, selector.c_str());
  if (!value.IsArray()) {
    return env.Undefined();
  }
  Napi::Array array = value.As<Napi::Array>();
  if (array.Length() == 1) {
    return array.Get(uint32_t(0));
  }
  return env.Null();
}

Napi::Value Element::OnlySafe(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  Napi::Value value = Element::Query(env, elem->xdoc_, elem->xnode_, selector.c_str());
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

Napi::Value Element::HasClass(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1) {
    Napi::Error::New(env, "No class name.").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string name;
  if (!GetStringArg(info, 0, "name", &name)) {
    return env.Undefined();
  }
  bool value = xnode_has_class(elem->xnode_, name.c_str());
  return Napi::Boolean::New(env, value);
}

Napi::Value Element::HasAttribute(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1) {
    Napi::Error::New(env, "No attribute name.").ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string name;
  if (!GetStringArg(info, 0, "name", &name)) {
    return env.Undefined();
  }
  bool value = xnode_has_attr(elem->xnode_, name.c_str());
  return Napi::Boolean::New(env, value);
}

Napi::Value Element::Next(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  bool has_selector = false;
  if (!GetOptionalStringArg(info, 0, "selector", &selector, &has_selector)) {
    return env.Undefined();
  }

  if (has_selector) {
    QueryHandle query(selector.c_str());
    if (!query.get()) {
      Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
      return env.Undefined();
    }
    for (XNode *next = xnode_next(elem->xnode_, NULL); next; next = xnode_next(next, NULL)) {
      if (MatchQuery(next, query.get())) {
        return Create(env, elem->xdoc_, next);
      }
    }
    return env.Null();
  }

  XNode *next = xnode_next(elem->xnode_, NULL);
  return Create(env, elem->xdoc_, next);
}

Napi::Value Element::Prev(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  bool has_selector = false;
  if (!GetOptionalStringArg(info, 0, "selector", &selector, &has_selector)) {
    return env.Undefined();
  }

  if (has_selector) {
    QueryHandle query(selector.c_str());
    if (!query.get()) {
      Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
      return env.Undefined();
    }
    for (XNode *prev = xnode_prev(elem->xnode_, NULL); prev; prev = xnode_prev(prev, NULL)) {
      if (MatchQuery(prev, query.get())) {
        return Create(env, elem->xdoc_, prev);
      }
    }
    return env.Null();
  }

  XNode *prev = xnode_prev(elem->xnode_, NULL);
  return Create(env, elem->xdoc_, prev);
}

Napi::Value Element::Query(Napi::Env env, XDocWrapper *wrapper,
                           const XNode *node, const char *selector) {
  QueryHandle query(selector);
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::EscapableHandleScope scope(env);
  const XNodeArray *result = xnode_query_execute(query.get(), node);
  Napi::Array array = Napi::Array::New(
      env, result ? xnode_array_size(result) : 0);
  if (result) {
    for (uint32_t i = 0; i < xnode_array_size(result); i++) {
      Napi::Value obj = Create(env, wrapper, xnode_array_get(result, i));
      array.Set(i, obj);
    }
  }

  return scope.Escape(array);
}

Napi::Value Element::FirstOrThrow(const Napi::CallbackInfo& info) {
  return FirstSafe(info);
}

Napi::Value Element::OnlyOrThrow(const Napi::CallbackInfo& info) {
  return OnlySafe(info);
}

Napi::Value Element::AttrOrThrow(const Napi::CallbackInfo& info) {
  return AttrSafe(info);
}

Napi::Value Element::Text(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }

  bool isSelector = (info.Length() > 0 && info[0].IsString());
  bool isOptions = (info.Length() > 0 && info[0].IsObject());

  auto applyOptions = [env](XNode *node, const Napi::Object &opts) -> Napi::Value {
    std::string text;
    if (opts.Has("separator") && opts.Get("separator").IsString()) {
      std::string sep = opts.Get("separator").As<Napi::String>();
      text = join_text_with_separator(node, sep);
    } else {
      text = get_inner_text(node);
    }
    if (opts.Has("normalize") && opts.Get("normalize").As<Napi::Boolean>()) {
      text = normalize_whitespace(text);
    }
    return Napi::String::New(env, text);
  };

  if (isSelector) {
    std::string selector = info[0].As<Napi::String>();
    Napi::Value value = Element::Query(env, elem->xdoc_, elem->xnode_, selector.c_str());
    if (!value.IsArray()) {
      return env.Undefined();
    }
    Napi::Array array = value.As<Napi::Array>();
    if (array.Length() > 0) {
      Napi::Value first = array.Get(uint32_t(0));
      Element *match = Element::Unwrap(first.As<Napi::Object>());
      if (info.Length() > 1 && info[1].IsObject()) {
        return applyOptions(match->xnode_, info[1].As<Napi::Object>());
      }
      return Napi::String::New(env, get_inner_text(match->xnode_));
    }
    return env.Null();
  }

  if (isOptions) {
    return applyOptions(elem->xnode_, info[0].As<Napi::Object>());
  }

  if (info.Length() > 0 && !info[0].IsUndefined() && !info[0].IsNull()) {
    Napi::TypeError::New(env, "selector or options object expected")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return Napi::String::New(env, get_inner_text(elem->xnode_));
}

Napi::Value Element::TextOrThrow(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), elem->xnode_);
  if (result && xnode_array_size(result) > 0) {
    XNode *node = xnode_array_get(result, 0);
    return Napi::String::New(env, get_inner_text(node));
  }

  Napi::Error::New(env, "No element found").ThrowAsJavaScriptException();
  return env.Undefined();
}

Napi::Value Element::Exists(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), elem->xnode_);
  return Napi::Boolean::New(env, result && xnode_array_size(result) > 0);
}

Napi::Value Element::Count(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }
  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }

  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  const XNodeArray *result = xnode_query_execute(query.get(), elem->xnode_);
  return Napi::Number::New(env, result ? xnode_array_size(result) : 0);
}

Napi::Value Element::Closest(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }

  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  XNode *current = xnode_parent(elem->xnode_);
  while (current) {
    if (MatchQuery(current, query.get())) {
      return Create(env, elem->xdoc_, current);
    }
    current = xnode_parent(current);
  }

  return env.Null();
}

Napi::Value Element::Matches(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }

  std::string selector;
  if (!GetStringArg(info, 0, "selector", &selector)) {
    return env.Undefined();
  }
  QueryHandle query(selector.c_str());
  if (!query.get()) {
    Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  return Napi::Boolean::New(env, MatchQuery(elem->xnode_, query.get()));
}

Napi::Value Element::Is(const Napi::CallbackInfo& info) {
  return Matches(info);
}

Napi::Value Element::Children(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }

  GumboVector *kids = xnode_children(elem->xnode_);
  if (!kids) {
    return Napi::Array::New(env, 0);
  }

  XNodeQuery *filter = nullptr;
  std::string selStr;
  if (info.Length() > 0 && info[0].IsString() && !info[0].IsUndefined() && !info[0].IsNull()) {
    selStr = info[0].As<Napi::String>();
    filter = xnode_query_create(selStr.c_str(), NULL, NULL);
    if (!filter) {
      Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
      return Napi::Array::New(env, 0);
    }
  } else if (info.Length() > 0 && !info[0].IsUndefined() && !info[0].IsNull()) {
    Napi::TypeError::New(env, "selector must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }
  Napi::Array result = Napi::Array::New(env);
  uint32_t count = 0;
  for (unsigned i = 0; i < kids->length; ++i) {
    XNode *child = static_cast<XNode*>(kids->data[i]);
    if (child->type != GUMBO_NODE_ELEMENT && child->type != GUMBO_NODE_TEMPLATE) continue;
    if (filter && !MatchQuery(child, filter)) continue;
    result.Set(count++, Create(env, elem->xdoc_, child));
  }

  if (filter) {
    xnode_query_free(filter);
  }
  return result;
}

Napi::Value Element::Siblings(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }

  XNode *parent = xnode_parent(elem->xnode_);
  if (!parent) {
    return Napi::Array::New(env, 0);
  }

  GumboVector *kids = xnode_children(parent);
  if (!kids) {
    return Napi::Array::New(env, 0);
  }

  XNodeQuery *filter = nullptr;
  std::string selStr;
  if (info.Length() > 0 && info[0].IsString() && !info[0].IsUndefined() && !info[0].IsNull()) {
    selStr = info[0].As<Napi::String>();
    filter = xnode_query_create(selStr.c_str(), NULL, NULL);
    if (!filter) {
      Napi::Error::New(env, "Bad selector.").ThrowAsJavaScriptException();
      return Napi::Array::New(env, 0);
    }
  } else if (info.Length() > 0 && !info[0].IsUndefined() && !info[0].IsNull()) {
    Napi::TypeError::New(env, "selector must be a string")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  Napi::Array result = Napi::Array::New(env);
  uint32_t count = 0;
  for (unsigned i = 0; i < kids->length; ++i) {
    XNode *sibling = static_cast<XNode*>(kids->data[i]);
    if (sibling == elem->xnode_) continue;
    if (sibling->type != GUMBO_NODE_ELEMENT && sibling->type != GUMBO_NODE_TEMPLATE) continue;
    if (filter && !MatchQuery(sibling, filter)) continue;
    result.Set(count++, Create(env, elem->xdoc_, sibling));
  }

  if (filter) {
    xnode_query_free(filter);
  }
  return result;
}

Napi::Value Element::Rows(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::HandleScope scope(env);
  Element* elem = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, elem)) {
    return env.Undefined();
  }

  QueryHandle trQuery("tr");
  if (!trQuery.get()) {
    return Napi::Array::New(env, 0);
  }

  const XNodeArray *rows = xnode_query_execute(trQuery.get(), elem->xnode_);
  if (!rows || xnode_array_size(rows) == 0) {
    return Napi::Array::New(env, 0);
  }

  uint32_t rowCount = xnode_array_size(rows);

  // Collect headers from the first row when it contains header cells.
  std::vector<std::string> headers;
  if (rowCount > 0) {
    XNode *firstRow = xnode_array_get(rows, 0);
    GumboVector *firstRowChildren = xnode_children(firstRow);
    if (firstRowChildren) {
      for (unsigned i = 0; i < firstRowChildren->length; i++) {
        XNode *cell = static_cast<XNode*>(firstRowChildren->data[i]);
        if ((cell->type == GUMBO_NODE_ELEMENT || cell->type == GUMBO_NODE_TEMPLATE) &&
            cell->v.element.tag == GUMBO_TAG_TH) {
          headers.push_back(get_inner_text(cell));
        }
      }
    }
  }

  uint32_t startRow = headers.empty() ? 0 : 1;
  Napi::Array result = Napi::Array::New(env);
  uint32_t resultIdx = 0;

  for (uint32_t r = startRow; r < rowCount; r++) {
    XNode *row = xnode_array_get(rows, r);
    GumboVector *rowChildren = xnode_children(row);
    if (!rowChildren) continue;

    Napi::Object obj = Napi::Object::New(env);
    uint32_t colIdx = 0;

    for (unsigned c = 0; c < rowChildren->length; c++) {
      XNode *cell = static_cast<XNode*>(rowChildren->data[c]);
      if (cell->type != GUMBO_NODE_ELEMENT && cell->type != GUMBO_NODE_TEMPLATE) continue;
      if (!headers.empty() && cell->v.element.tag != GUMBO_TAG_TD) continue;
      if (headers.empty() &&
          cell->v.element.tag != GUMBO_TAG_TD &&
          cell->v.element.tag != GUMBO_TAG_TH) continue;

      std::string text = get_inner_text(cell);

      if (colIdx < headers.size()) {
        obj.Set(headers[colIdx], Napi::String::New(env, text));
      } else {
        obj.Set(colIdx, Napi::String::New(env, text));
      }
      colIdx++;
    }

    if (colIdx > 0) {
      result.Set(resultIdx++, obj);
    }
  }

  return result;
}

Napi::Value Element::GetTextContent(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Element* element = Element::Unwrap(info.This().As<Napi::Object>());
  if (!HasNode(info, element)) {
    return env.Undefined();
  }
  return Napi::String::New(env, get_inner_text(element->xnode_));
}

} // namespace html
