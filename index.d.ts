export declare type NodeType = 'DOCUMENT' | 'ELEMENT' | 'TEXT' | 'CDATA' | 'COMMENT' | 'WHITESPACE' | 'TEMPLATE' | 'UNKNOWN';

export declare type XElement = {
  childNodes: XElement[];
  nodeType: NodeType;
  parent: Element | null;
  outerHTML: string;
  innerText: string;
  tagName: string | null;

  attr: (name: string) => string | undefined;
  attr_s: (name: string) => string;
  find: (selector: string) => XElement[];
  first: (selector: string) => XElement | null;
  first_s: (selector: string) => XElement;
  hasClass: (name: string) => boolean;
  hasAttribute: (name: string) => boolean;
  prev: (selector: string) => XElement | null;
  next: (selector: string) => XElement | null;
};

export declare type XDocument = {
  documentElement: XElement;
  outerHTML: string;
  innerText: string;
  tagName: string | null;
  find: (selector: string) => XElement[]
  first: (selector: string) => XElement | null;
  first_s: (selector: string) => XElement;
};

export declare function parse(html: string): XDocument;
