export declare type NodeType = 'DOCUMENT' | 'ELEMENT' | 'TEXT' | 'CDATA' | 'COMMENT' | 'WHITESPACE' | 'TEMPLATE' | 'UNKNOWN';

export declare type TextOptions = {
  normalize?: boolean;
  separator?: string;
};

export declare type ExtractSchema = {
  [key: string]: [string, string | ExtractSchema | 'exists' | 'text' | 'count'];
};

export declare type XElement = {
  childNodes: XElement[];
  nodeType: NodeType;
  parent: XElement | null;
  outerHTML: string;
  innerText: string;
  textContent: string;
  tagName: string | null;

  attr: (name: string) => string | undefined;
  attr_s: (name: string) => string;
  find: (selector: string) => XElement[];
  first: (selector: string) => XElement | null;
  first_s: (selector: string) => XElement;
  firstOrThrow: (selector: string) => XElement;
  only: (selector: string) => XElement | null;
  only_s: (selector: string) => XElement;
  onlyOrThrow: (selector: string) => XElement;
  hasClass: (name: string) => boolean;
  hasAttribute: (name: string) => boolean;
  prev: (selector?: string) => XElement | null;
  next: (selector?: string) => XElement | null;

  // New methods
  attrOrThrow: (name: string) => string;
  text: ((opts?: TextOptions) => string) | ((selector: string, opts?: TextOptions) => string | null);
  textOrThrow: (selector: string) => string;
  exists: (selector: string) => boolean;
  count: (selector: string) => number;
  closest: (selector: string) => XElement | null;
  children: (selector?: string) => XElement[];
  siblings: (selector?: string) => XElement[];
  matches: (selector: string) => boolean;
  is: (selector: string) => boolean;
  rows: () => Array<{ [header: string]: string }>;
  urlAttr: (attrName: string) => string | undefined;
  extract: (schema: ExtractSchema) => any;
};

export declare type XDocument = {
  documentElement: XElement;
  innerText: string;
  textContent: string;
  outerHTML: string;
  tagName: string | null;
  nodeType: NodeType;

  find: (selector: string) => XElement[];
  first: (selector: string) => XElement | null;
  first_s: (selector: string) => XElement;
  firstOrThrow: (selector: string) => XElement;
  only: (selector: string) => XElement | null;
  only_s: (selector: string) => XElement;
  onlyOrThrow: (selector: string) => XElement;

  // New convenience methods
  text: (selector: string, opts?: TextOptions) => string | null;
  textOrThrow: (selector: string) => string;
  attr: (selector: string, name: string) => string | undefined;
  attrOrThrow: (selector: string, name: string) => string;
  exists: (selector: string) => boolean;
  count: (selector: string) => number;

  // URL helpers
  url: (selector: string, attr: string) => string | undefined;

  // High-level extractors
  extract: (schema: ExtractSchema) => any;
  meta: () => { [key: string]: string };
  links: () => Array<{ text: string; href: string }>;
  images: () => Array<{ alt: string; src: string }>;
  forms: () => XElement[];
  tables: () => XElement[];
  table: (selector?: string) => Array<{ [header: string]: string }>;
  title: () => string | null;
  description: () => string | undefined;
  canonicalUrl: () => string | undefined;
};

export declare function parse(html: string, options?: { baseUrl?: string }): XDocument;
