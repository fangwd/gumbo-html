'use strict';

/**
 * Enhances the native gumbo-html module with high-level methods.
 * Applied to Document and Element prototypes so all instances benefit.
 */

/**
 * Try to resolve a potentially relative URL against a known base URL stored on the document.
 */
function resolveUrl(doc, href) {
  if (!href || !doc || !doc._baseUrl) return href;
  try {
    return new URL(href, doc._baseUrl).toString();
  } catch (_) {
    return href;
  }
}

function annotateWithBaseUrl(value, baseUrl) {
  if (!baseUrl || value == null) return value;

  if (Array.isArray(value)) {
    for (const item of value) {
      annotateWithBaseUrl(item, baseUrl);
    }
    return value;
  }

  if (typeof value === 'object') {
    annotateElementInstance(value, baseUrl);
  }

  return value;
}

const ELEMENT_RETURN_METHODS = [
  'find',
  'first',
  'first_s',
  'firstOrThrow',
  'only',
  'only_s',
  'onlyOrThrow',
  'next',
  'prev',
  'closest',
  'children',
  'siblings',
];

const DOCUMENT_RETURN_METHODS = [
  'find',
  'first',
  'first_s',
  'firstOrThrow',
  'only',
  'only_s',
  'onlyOrThrow',
  'forms',
  'tables',
];

function annotateReturnMethods(instance, methods) {
  if (instance._gumboHtmlReturnMethodsWrapped) return;

  for (const method of methods) {
    if (typeof instance[method] !== 'function') continue;

    const original = instance[method].bind(instance);
    Object.defineProperty(instance, method, {
      configurable: true,
      value(...args) {
        return annotateWithBaseUrl(original(...args), this._baseUrl);
      },
    });
  }

  Object.defineProperty(instance, '_gumboHtmlReturnMethodsWrapped', {
    configurable: true,
    value: true,
  });
}

function annotateElementInstance(element, baseUrl) {
  if (!element || typeof element !== 'object') return element;
  if (baseUrl) element._baseUrl = baseUrl;
  if (typeof element.attr !== 'function') return element;

  annotateReturnMethods(element, ELEMENT_RETURN_METHODS);

  if (!Object.prototype.hasOwnProperty.call(element, 'parent')) {
    const descriptor = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(element), 'parent');
    if (descriptor && typeof descriptor.get === 'function') {
      Object.defineProperty(element, 'parent', {
        configurable: true,
        get() {
          return annotateWithBaseUrl(descriptor.get.call(this), this._baseUrl);
        },
      });
    }
  }

  if (!Object.prototype.hasOwnProperty.call(element, 'childNodes')) {
    const descriptor = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(element), 'childNodes');
    if (descriptor && typeof descriptor.get === 'function') {
      Object.defineProperty(element, 'childNodes', {
        configurable: true,
        get() {
          return annotateWithBaseUrl(descriptor.get.call(this), this._baseUrl);
        },
      });
    }
  }

  if (typeof element.urlAttr !== 'function') {
    Object.defineProperty(element, 'urlAttr', {
      configurable: true,
      value(attrName) {
        const val = this.attr(attrName);
        if (!val) return val;
        return resolveUrl(this, val);
      },
    });
  }

  return element;
}

function annotateDocumentInstance(doc, baseUrl) {
  if (!doc || typeof doc !== 'object') return doc;
  if (baseUrl) doc._baseUrl = baseUrl;

  annotateReturnMethods(doc, DOCUMENT_RETURN_METHODS);

  if (!Object.prototype.hasOwnProperty.call(doc, 'documentElement')) {
    const descriptor = Object.getOwnPropertyDescriptor(Object.getPrototypeOf(doc), 'documentElement');
    if (descriptor && typeof descriptor.get === 'function') {
      Object.defineProperty(doc, 'documentElement', {
        configurable: true,
        get() {
          return annotateWithBaseUrl(descriptor.get.call(this), this._baseUrl);
        },
      });
    }
  }

  return doc;
}

function enhanceDocumentAccessors(DocumentProto) {
  if (DocumentProto._gumboHtmlAccessors) return;

  Object.defineProperties(DocumentProto, {
    innerText: {
      get() {
        return this.documentElement.innerText;
      },
    },
    textContent: {
      get() {
        return this.documentElement.textContent;
      },
    },
    outerHTML: {
      get() {
        return this.documentElement.outerHTML;
      },
    },
    tagName: {
      get() {
        return null;
      },
    },
    nodeType: {
      get() {
        return 'DOCUMENT';
      },
    },
  });

  Object.defineProperty(DocumentProto, '_gumboHtmlAccessors', { value: true });
}

// ------------------------------------------------------------------------
// Element prototype enhancements
// ------------------------------------------------------------------------
function enhanceElement(ElementProto) {
  /** URL helper: resolve a URL attribute on this element */
  if (typeof ElementProto.urlAttr !== 'function') {
    ElementProto.urlAttr = function (attrName) {
      const val = this.attr(attrName);
      if (!val) return val;
      if (this._baseUrl) return resolveUrl(this, val);
      // Walk up to find a document reference for base URL
      let el = this;
      while (el && typeof el._baseUrl === 'undefined') {
        el = el.parent;
      }
      return resolveUrl(el, val);
    };
  }
}

// ------------------------------------------------------------------------
// Document prototype enhancements
// ------------------------------------------------------------------------
function enhanceDocument(DocumentProto) {
  enhanceDocumentAccessors(DocumentProto);

  /** Structured extraction */
  DocumentProto.extract = function (schema) {
    return extractFrom(this, this, schema);
  };

  /** Meta tags as { name: content } */
  DocumentProto.meta = function () {
    const result = {};
    const metaSelectors = [
      this.find('meta[name]'),
      this.find('meta[property]'),
      this.find('meta[http-equiv]'),
    ];
    for (const metas of metaSelectors) {
      for (const m of Object.values(metas)) {
        const key = m.attr('name') || m.attr('property') || m.attr('http-equiv');
        const content = m.attr('content');
        if (key && content) {
          result[key] = content;
        }
      }
    }
    return result;
  };

  /** Links: [{ text, href }] */
  DocumentProto.links = function () {
    const result = [];
    const anchors = this.find('a[href]');
    for (const a of anchors) {
      result.push({
        text: a.text(),
        href: resolveUrl(this, a.attr('href')),
      });
    }
    return result;
  };

  /** Images: [{ alt, src }] */
  DocumentProto.images = function () {
    const result = [];
    const imgs = this.find('img[src]');
    for (const img of imgs) {
      result.push({
        alt: img.attr('alt') || '',
        src: resolveUrl(this, img.attr('src')),
      });
    }
    return result;
  };

  /** Forms: array of form elements */
  DocumentProto.forms = function () {
    return this.find('form');
  };

  /** Tables: array of table elements */
  DocumentProto.tables = function () {
    return this.find('table');
  };

  /** Table extraction: structured data from a table by selector */
  DocumentProto.table = function (selector) {
    const table = this.first(selector || 'table');
    if (!table) return [];
    return table.rows();
  };

  /** Page title from <title> element */
  DocumentProto.title = function () {
    return this.text('title');
  };

  /** Meta description from <meta name=description> */
  DocumentProto.description = function () {
    const meta = this.first('meta[name="description"]');
    return meta ? meta.attr('content') : undefined;
  };

  /** Canonical URL from <link rel=canonical> */
  DocumentProto.canonicalUrl = function () {
    const link = this.first('link[rel="canonical"]');
    return link ? link.attr('href') : undefined;
  };

  /** URL helper: get a resolved URL attribute for a selector */
  DocumentProto.url = function (selector, attrName) {
    const el = this.first(selector);
    if (!el) return undefined;
    const val = el.attr(attrName);
    if (!val) return val;
    return resolveUrl(this, val);
  };
}

// ------------------------------------------------------------------------
// Extract engine (shared between Document and Element)
// ------------------------------------------------------------------------
function extractFrom(root, context, schema) {
  if (Array.isArray(schema)) {
    return schema.map(s => extractFrom(root, context, s));
  }

  if (schema === null || typeof schema !== 'object') {
    return schema;
  }

  const result = {};
  for (const [key, value] of Object.entries(schema)) {
    if (!Array.isArray(value)) {
      result[key] = extractFrom(root, context, value);
      continue;
    }

    const [selector, action] = value;

    if (action && typeof action === 'object' && !Array.isArray(action)) {
      // Nested extraction: find all matches, extract from each
      const matches = context.find(selector);
      result[key] = matches.map(m => extractFrom(root, m, action));
      continue;
    }

    if (action === 'exists') {
      result[key] = context.exists(selector);
      continue;
    }

    if (action === 'text') {
      result[key] = context.text(selector);
      continue;
    }

    if (action === 'count') {
      result[key] = context.count(selector);
      continue;
    }

    // Treat action as attribute name
    if (typeof action === 'string') {
      const el = context.first(selector);
      result[key] = el ? el.attr(action) : undefined;
      continue;
    }
  }

  return result;
}

// ------------------------------------------------------------------------
// Main entry: applies enhancements
// ------------------------------------------------------------------------
function enhance(html) {
  enhanceElement(html.Element.prototype);
  enhanceDocument(html.Document.prototype);

  // Add extract to Element prototype too
  html.Element.prototype.extract = function (schema) {
    return extractFrom(this, this, schema);
  };

  // Intercept parse() to support options (e.g. baseUrl)
  const _originalParse = html.parse;
  html.parse = function (htmlStr, options) {
    const doc = _originalParse(htmlStr);
    if (options && options.baseUrl) {
      return annotateDocumentInstance(doc, options.baseUrl);
    }
    return annotateDocumentInstance(doc, undefined);
  };

  return html;
}

module.exports = { enhance };
