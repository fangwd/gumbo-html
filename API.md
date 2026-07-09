# API

`gumbo-html` is a native Node.js addon that parses HTML with Gumbo and exposes
CSS-selector helpers for querying, traversal, and common scraping tasks.

The package ships TypeScript declarations in `index.d.ts`. The examples below
use both CommonJS and TypeScript-style imports where useful.

## Importing

```js
const { parse } = require('gumbo-html');
```

```ts
import { parse, type XDocument, type XElement } from 'gumbo-html';
```

## `parse(html, options?)`

```ts
function parse(html: string, options?: { baseUrl?: string }): XDocument;
```

Parses an HTML string and returns an `XDocument`.

```js
const { parse } = require('gumbo-html');

const doc = parse(`
  <article class="post">
    <h1>Hello</h1>
    <a href="/posts/hello">Read more</a>
  </article>
`);

console.log(doc.firstOrThrow('h1').innerText); // "Hello"
```

`html` must be a string. Passing `undefined`, `null`, or another type throws a
`TypeError` with the message `html must be a string`.

### `options.baseUrl`

`baseUrl` is used by URL helper methods to resolve relative URLs.

```js
const doc = parse('<a href="/docs">Docs</a>', {
  baseUrl: 'https://example.com/app/',
});

console.log(doc.url('a', 'href')); // "https://example.com/docs"
console.log(doc.links());          // [{ text: "Docs", href: "https://example.com/docs" }]
```

## Selector Support

Selectors are CSS-like and scoped to the document or element you call from.

Supported selector features:

- Type selectors: `article`, `h1`
- Universal selector: `*`
- ID selectors: `#main`
- Class selectors: `.post`, `article.featured`
- Attribute selectors: `[href]`, `[rel="canonical"]`, `[class~="active"]`
- Attribute operators: `=`, `~=`, `|=`, `^=`, `$=`, `*=`
- Combinators: descendant `A B`, child `A > B`, adjacent sibling `A + B`,
  general sibling `A ~ B`
- Relative selectors beginning with a combinator, such as `> li`, when querying
  from a specific context

Unsupported selector features include selector lists (`a, img`), pseudo-classes
(`:first-child`, `:not(...)`, `:nth-child(...)`), pseudo-elements, namespaces,
and general CSS identifier escaping.

Invalid selectors throw `Error: Bad selector.`. Missing or non-string selector
arguments throw `TypeError: selector must be a string`.

Tag and attribute names are matched case-insensitively. Attribute values are
matched case-sensitively.

## Return Conventions

Many methods have optional and throwing forms:

- Optional element lookups return `null` when no matching element exists.
- Optional attribute lookups return `undefined` when the attribute is missing.
- Throwing lookup methods end in `OrThrow` and throw when the required match is
  missing or not unique.
- Query methods return JavaScript arrays.
- `childNodes` can include text, whitespace, comment, CDATA, and element nodes.

## Types

```ts
type NodeType =
  | 'DOCUMENT'
  | 'ELEMENT'
  | 'TEXT'
  | 'CDATA'
  | 'COMMENT'
  | 'WHITESPACE'
  | 'TEMPLATE'
  | 'UNKNOWN';

type TextOptions = {
  normalize?: boolean;
  separator?: string;
};
```

`normalize: true` trims leading/trailing whitespace and collapses ASCII
whitespace runs into single spaces. `separator` joins non-empty descendant text
nodes with the provided separator.

## `XDocument`

An `XDocument` represents the parsed document. `doc.documentElement` is the root
HTML element produced by Gumbo.

### Properties

| Property | Type | Description |
| --- | --- | --- |
| `documentElement` | `XElement` | Root element, usually `<html>`. |
| `innerText` | `string` | Text from `documentElement`. |
| `textContent` | `string` | Same text extraction as `innerText`. |
| `outerHTML` | `string` | Serialized root element HTML. |
| `tagName` | `null` | Documents do not have a tag name. |
| `nodeType` | `'DOCUMENT'` | Document node type. |

### Query Methods

| Method | Returns | Description |
| --- | --- | --- |
| `find(selector)` | `XElement[]` | All matching descendants. |
| `first(selector)` | `XElement \| null` | First matching descendant or `null`. |
| `firstOrThrow(selector)` | `XElement` | First match, or throws `No element found`. |
| `only(selector)` | `XElement \| null` | Match only when exactly one element exists; otherwise `null`. |
| `onlyOrThrow(selector)` | `XElement` | Exactly one match, or throws `Not a single element`. |

```js
const doc = parse('<main><p>A</p><p>B</p></main>');

console.log(doc.find('p').length);  // 2
console.log(doc.first('h1'));       // null
console.log(doc.only('main').tagName); // "main"
```

### Convenience Methods

| Method | Returns | Description |
| --- | --- | --- |
| `text(selector, opts?)` | `string \| null` | Text of the first match, or `null`. |
| `textOrThrow(selector)` | `string` | Text of the first match, or throws `No element found`. |
| `attr(selector, name)` | `string \| undefined` | Attribute from the first match. |
| `attrOrThrow(selector, name)` | `string` | Attribute from the first match, or throws `Attribute not found`. |
| `exists(selector)` | `boolean` | Whether at least one element matches. |
| `count(selector)` | `number` | Number of matching elements. |
| `url(selector, attr)` | `string \| undefined` | Resolved URL attribute from the first match. |

```js
const doc = parse(`
  <article>
    <h1>  Hello   World  </h1>
    <a href="/hello">Read</a>
  </article>
`, { baseUrl: 'https://example.com/' });

console.log(doc.text('h1', { normalize: true })); // "Hello World"
console.log(doc.attr('a', 'href'));               // "/hello"
console.log(doc.url('a', 'href'));                // "https://example.com/hello"
console.log(doc.exists('article'));               // true
console.log(doc.count('a'));                      // 1
```

### Scraping Helpers

| Method | Returns | Description |
| --- | --- | --- |
| `meta()` | `{ [key: string]: string }` | Metadata from `meta[name]`, `meta[property]`, and `meta[http-equiv]` using `content` values. |
| `links()` | `{ text: string; href: string }[]` | All `a[href]` elements. `href` is resolved with `baseUrl` when provided. |
| `images()` | `{ alt: string; src: string }[]` | All `img[src]` elements. Missing `alt` becomes `''`; `src` is resolved with `baseUrl` when provided. |
| `forms()` | `XElement[]` | All `form` elements. |
| `tables()` | `XElement[]` | All `table` elements. |
| `table(selector?)` | `Array<{ [header: string]: string }>` | Rows from the first matching table, or the first table when no selector is given. |
| `title()` | `string \| null` | Text from the first `title` element. |
| `description()` | `string \| undefined` | `content` from `meta[name="description"]`. |
| `canonicalUrl()` | `string \| undefined` | Raw `href` from `link[rel="canonical"]`. |

```js
const doc = parse(`
  <head>
    <title>Example</title>
    <meta name="description" content="Demo page">
    <meta property="og:title" content="Example OG">
  </head>
  <body>
    <a href="/a">A</a>
    <img src="/a.png" alt="A">
  </body>
`, { baseUrl: 'https://example.com/' });

console.log(doc.title());       // "Example"
console.log(doc.description()); // "Demo page"
console.log(doc.meta());        // { description: "Demo page", "og:title": "Example OG" }
console.log(doc.links());       // [{ text: "A", href: "https://example.com/a" }]
console.log(doc.images());      // [{ alt: "A", src: "https://example.com/a.png" }]
```

### Structured Extraction

```ts
type ExtractSchema = {
  [key: string]: [string, string | ExtractSchema | 'exists' | 'text' | 'count'];
};
```

`extract(schema)` evaluates each field relative to the current document or
element.

Schema actions:

- `[selector, 'text']` returns text from the first match, or `null`.
- `[selector, 'exists']` returns a boolean.
- `[selector, 'count']` returns a number.
- `[selector, attrName]` returns an attribute from the first match, or
  `undefined`.
- `[selector, nestedSchema]` returns an array by applying `nestedSchema` to each
  matching element.

```js
const doc = parse(`
  <article class="post">
    <h2>First</h2>
    <a href="/first">Read</a>
  </article>
  <article class="post">
    <h2>Second</h2>
  </article>
`);

const data = doc.extract({
  postCount: ['article.post', 'count'],
  posts: ['article.post', {
    heading: ['h2', 'text'],
    href: ['a', 'href'],
    hasLink: ['a', 'exists'],
  }],
});

console.log(data);
// {
//   postCount: 2,
//   posts: [
//     { heading: "First", href: "/first", hasLink: true },
//     { heading: "Second", href: undefined, hasLink: false }
//   ]
// }
```

Attribute values returned by `extract` are raw values. Use `doc.url()` or
`el.urlAttr()` when you need URL resolution.

## `XElement`

An `XElement` represents an element or another Gumbo node returned by
`childNodes`.

### Properties

| Property | Type | Description |
| --- | --- | --- |
| `childNodes` | `XElement[]` | Direct child nodes. Includes non-element nodes. |
| `nodeType` | `NodeType` | Gumbo node type. |
| `parent` | `XElement \| null` | Parent element, or `null` for the document root. |
| `outerHTML` | `string` | Source HTML for this node. |
| `innerText` | `string` | Descendant text for text/element/template nodes. |
| `textContent` | `string` | Same text extraction as `innerText`. |
| `tagName` | `string \| null` | Normalized tag name for element/template nodes; `null` for non-elements. |

```js
const doc = parse('<div><!-- note -->Text <b>bold</b></div>');
const div = doc.firstOrThrow('div');

console.log(div.childNodes.map((node) => node.nodeType));
// ["COMMENT", "TEXT", "ELEMENT"]
```

### Query Methods

Element query methods have the same behavior as document query methods, but the
search is scoped to descendants of the element.

| Method | Returns |
| --- | --- |
| `find(selector)` | `XElement[]` |
| `first(selector)` | `XElement \| null` |
| `firstOrThrow(selector)` | `XElement` |
| `only(selector)` | `XElement \| null` |
| `onlyOrThrow(selector)` | `XElement` |

```js
const article = parse(`
  <article>
    <h2>Title</h2>
    <p>Summary</p>
  </article>
`).firstOrThrow('article');

console.log(article.firstOrThrow('h2').innerText); // "Title"
```

### Attribute Methods

| Method | Returns | Description |
| --- | --- | --- |
| `attr(name)` | `string \| undefined` | Attribute value, or `undefined`. |
| `attr_s(name)` | `string` | Attribute value, or throws `Attribute not found`. |
| `attrOrThrow(name)` | `string` | Alias for `attr_s`. |
| `hasClass(name)` | `boolean` | Whether the `class` attribute contains `name`. |
| `hasAttribute(name)` | `boolean` | Whether the attribute exists. |
| `urlAttr(name)` | `string \| undefined` | Resolved URL attribute when the document was parsed with `baseUrl`. |

```js
const doc = parse('<a class="button primary" href="/start">Start</a>', {
  baseUrl: 'https://example.com/',
});

const link = doc.firstOrThrow('a');
console.log(link.attr('href'));        // "/start"
console.log(link.urlAttr('href'));     // "https://example.com/start"
console.log(link.hasClass('primary')); // true
```

### Text Methods

| Method | Returns | Description |
| --- | --- | --- |
| `text(opts?)` | `string` | Text for this element. |
| `text(selector, opts?)` | `string \| null` | Text for the first matching descendant. |
| `textOrThrow(selector)` | `string` | Text for the first matching descendant, or throws `No element found`. |

```js
const el = parse('<div><span>Hello</span><b>World</b></div>').firstOrThrow('div');

console.log(el.text());                       // "HelloWorld"
console.log(el.text({ separator: ' ' }));     // "Hello World"
console.log(el.text('span'));                 // "Hello"
```

### Traversal and Matching

| Method | Returns | Description |
| --- | --- | --- |
| `prev(selector?)` | `XElement \| null` | Previous element sibling, optionally filtered. |
| `next(selector?)` | `XElement \| null` | Next element sibling, optionally filtered. |
| `closest(selector)` | `XElement \| null` | Nearest ancestor matching `selector`. This starts at the parent, not the element itself. |
| `children(selector?)` | `XElement[]` | Direct element children, optionally filtered. Non-element nodes are excluded. |
| `siblings(selector?)` | `XElement[]` | Element siblings, optionally filtered. The current element is excluded. |
| `matches(selector)` | `boolean` | Whether this element matches the full selector. |
| `is(selector)` | `boolean` | Alias for `matches`. |

```js
const doc = parse(`
  <section>
    <article class="post featured"><h2>One</h2></article>
    <article class="post"><h2>Two</h2></article>
  </section>
`);

const first = doc.firstOrThrow('article.featured');
const second = first.next('article.post');

console.log(second.prev('.featured') === null); // false
console.log(first.children('h2').length);       // 1
console.log(first.matches('section article'));  // true
console.log(first.closest('section').tagName);  // "section"
```

### Table Rows

`rows()` extracts rows from a table-like element.

- If the first row contains `th` cells, those values become object keys and the
  first row is treated as a header row.
- With headers, data rows use `td` cells.
- Without headers, cells are assigned numeric object keys.

```js
const doc = parse(`
  <table>
    <tr><th>Name</th><th>Age</th></tr>
    <tr><td>Alice</td><td>30</td></tr>
    <tr><td>Bob</td><td>25</td></tr>
  </table>
`);

console.log(doc.firstOrThrow('table').rows());
// [
//   { Name: "Alice", Age: "30" },
//   { Name: "Bob", Age: "25" }
// ]
```

`doc.table(selector?)` is a convenience wrapper around
`doc.first(selector || 'table')?.rows()`, returning `[]` when no table matches.

### Element Extraction

`el.extract(schema)` uses the same structured extraction rules as
`doc.extract(schema)`, scoped to the element.

```js
const cards = parse(`
  <div class="card"><h3>A</h3><span>New</span></div>
  <div class="card"><h3>B</h3></div>
`).find('.card');

const data = cards.map((card) => card.extract({
  title: ['h3', 'text'],
  hasBadge: ['span', 'exists'],
}));

console.log(data);
// [{ title: "A", hasBadge: true }, { title: "B", hasBadge: false }]
```

## Error Summary

| Operation | Error |
| --- | --- |
| `parse()` without a string | `TypeError: html must be a string` |
| Query method without a string selector | `TypeError: selector must be a string` |
| Invalid selector syntax | `Error: Bad selector.` |
| `firstOrThrow()` with no match | `Error: No element found` |
| `onlyOrThrow()` with zero or multiple matches | `Error: Not a single element` |
| `attr_s()` / `attrOrThrow()` with missing attribute | `Error: Attribute not found` |
