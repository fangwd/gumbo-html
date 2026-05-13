#ifndef HTML_DOCUMENT_
#define HTML_DOCUMENT_

#include <napi.h>

#include "xnode_query.h"

namespace html {

struct XDocWrapper {
  XNodeDocument *xdoc;
  size_t ref_count;

  XDocWrapper(XNodeDocument *p)
      : xdoc(p), ref_count(1) {
  }

  ~XDocWrapper() {
    if (xdoc) {
      xnode_document_free(xdoc);
    }
  }
};

class Document : public Napi::ObjectWrap<Document> {
public:
  static void Init(Napi::Env env);
  static Napi::Value Parse(const Napi::CallbackInfo& info);

  static Napi::FunctionReference constructor;

  Document(const Napi::CallbackInfo& info);
  ~Document();

  Napi::Value Find(const Napi::CallbackInfo& info);
  Napi::Value First(const Napi::CallbackInfo& info);
  Napi::Value FirstSafe(const Napi::CallbackInfo& info);
  Napi::Value Only(const Napi::CallbackInfo& info);
  Napi::Value OnlySafe(const Napi::CallbackInfo& info);
  Napi::Value GetDocumentElement(const Napi::CallbackInfo& info);

  Napi::Value FirstOrThrow(const Napi::CallbackInfo& info);
  Napi::Value OnlyOrThrow(const Napi::CallbackInfo& info);
  Napi::Value Text(const Napi::CallbackInfo& info);
  Napi::Value TextOrThrow(const Napi::CallbackInfo& info);
  Napi::Value Exists(const Napi::CallbackInfo& info);
  Napi::Value Count(const Napi::CallbackInfo& info);
  Napi::Value Attr2(const Napi::CallbackInfo& info);
  Napi::Value AttrOrThrow2(const Napi::CallbackInfo& info);

  XDocWrapper *xdoc_wrapper_;
};

}

#endif // HTML_DOCUMENT_
