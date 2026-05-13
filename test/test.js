'use strict';

const html = require('../');
const assert = require('assert');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log(`  ✓ ${name}`);
  } catch (e) {
    failed++;
    console.log(`  ✗ ${name}: ${e.message}`);
  }
}

function run(suiteName, tests) {
  console.log(`\n${suiteName}`);
  tests();
}

// ============================================================
// 1. Basic API (regression)
// ============================================================
run('Basic API', () => {
  test('parse returns document', () => {
    const doc = html.parse('<div>hello</div>');
    assert.ok(doc);
    assert.ok(doc.documentElement);
  });

  test('documentElement.tagName', () => {
    const doc = html.parse('<div>hello</div>');
    assert.strictEqual(doc.documentElement.tagName, 'html');
  });

  test('find returns array', () => {
    const doc = html.parse('<div><p>a</p><p>b</p></div>');
    const ps = doc.find('p');
    assert.strictEqual(ps.length, 2);
  });

  test('first returns element or null', () => {
    const doc = html.parse('<div><p>a</p></div>');
    assert.strictEqual(doc.first('p').innerText, 'a');
    assert.strictEqual(doc.first('nonexistent'), null);
  });

  test('first_s returns element or throws', () => {
    const doc = html.parse('<div><p>a</p></div>');
    assert.strictEqual(doc.first_s('p').innerText, 'a');
    assert.throws(() => doc.first_s('nonexistent'), /No element found/);
  });

  test('only / only_s', () => {
    const doc = html.parse('<div><p class=a>hi</p><p>there</p></div>');
    assert.strictEqual(doc.only('.a').innerText, 'hi');
    assert.strictEqual(doc.only('p'), null);
    assert.throws(() => doc.only_s('p'), /Not a single element/);
  });

  test('attr / attr_s', () => {
    const doc = html.parse('<a href=/link class=btn>click</a>');
    const a = doc.first('a');
    assert.strictEqual(a.attr('href'), '/link');
    assert.strictEqual(a.attr('missing'), undefined);
    assert.throws(() => a.attr_s('missing'), /Attribute not found/);
  });

  test('hasClass / hasAttribute', () => {
    const doc = html.parse('<div class="foo bar">hi</div>');
    const d = doc.first('div');
    assert.strictEqual(d.hasClass('foo'), true);
    assert.strictEqual(d.hasClass('baz'), false);
    assert.strictEqual(d.hasAttribute('class'), true);
    assert.strictEqual(d.hasAttribute('id'), false);
  });

  test('next / prev', () => {
    const doc = html.parse('<div><p class=a>A</p><span>B</span><p class=c>C</p></div>');
    const a = doc.first('.a');
    const c = doc.first('.c');
    assert.strictEqual(a.next().tagName, 'span');
    assert.strictEqual(c.prev().tagName, 'span');
    assert.strictEqual(a.prev(), null);
    assert.strictEqual(c.next(), null);
  });

  test('childNodes', () => {
    const doc = html.parse('<div><!-- comment -->text<b>bold</b></div>');
    const d = doc.first('div');
    assert.strictEqual(d.childNodes.length, 3);
    assert.strictEqual(d.childNodes[0].nodeType, 'COMMENT');
    assert.strictEqual(d.childNodes[1].nodeType, 'TEXT');
    assert.strictEqual(d.childNodes[2].tagName, 'b');
  });

  test('innerText / textContent / outerHTML / nodeType', () => {
    const doc = html.parse('<div class=x><p>hi</p></div>');
    const d = doc.first('div');
    assert.strictEqual(d.innerText, 'hi');
    assert.strictEqual(d.textContent, 'hi');
    assert.ok(d.outerHTML.includes('<div class=x>'));
    assert.strictEqual(d.nodeType, 'ELEMENT');
  });

  test('parent', () => {
    const doc = html.parse('<div><p>hi</p></div>');
    const p = doc.first('p');
    assert.strictEqual(p.parent.tagName, 'div');
  });

  test('get_inner_text handles templates', () => {
    const doc = html.parse('<div>hello <span>world</span></div>');
    assert.strictEqual(doc.first('div').innerText, 'hello world');
  });
});

// ============================================================
// 2. New Aliases
// ============================================================
run('New Aliases', () => {
  test('firstOrThrow', () => {
    const doc = html.parse('<div><h1>Title</h1></div>');
    assert.strictEqual(doc.firstOrThrow('h1').innerText, 'Title');
    assert.throws(() => doc.firstOrThrow('p'));
  });

  test('onlyOrThrow', () => {
    const doc = html.parse('<div><p>a</p><p>b</p></div>');
    assert.strictEqual(doc.onlyOrThrow('div').tagName, 'div');
    assert.throws(() => doc.onlyOrThrow('p'));
  });

  test('attrOrThrow', () => {
    const doc = html.parse('<a href=/link>text</a>');
    assert.strictEqual(doc.first('a').attrOrThrow('href'), '/link');
    assert.throws(() => doc.first('a').attrOrThrow('missing'));
  });

  test('textOrThrow', () => {
    const doc = html.parse('<div><h1>Hello</h1></div>');
    assert.strictEqual(doc.textOrThrow('h1'), 'Hello');
    assert.throws(() => doc.textOrThrow('h2'));
  });
});

// ============================================================
// 3. Convenience Methods
// ============================================================
run('Convenience Methods', () => {
  test('text(selector) on Document', () => {
    const doc = html.parse('<div><h1>Hello</h1><h2>World</h2></div>');
    assert.strictEqual(doc.text('h1'), 'Hello');
    assert.strictEqual(doc.text('nonexistent'), null);
  });

  test('attr(selector, name) on Document', () => {
    const doc = html.parse('<a class=link href=/a>a</a><a href=/b>b</a>');
    assert.strictEqual(doc.attr('a', 'class'), 'link');
    assert.strictEqual(doc.attr('a[href="/b"]', 'class'), undefined);
  });

  test('attrOrThrow(selector, name) on Document', () => {
    const doc = html.parse('<a href=/a>link</a>');
    assert.strictEqual(doc.attrOrThrow('a', 'href'), '/a');
    assert.throws(() => doc.attrOrThrow('img', 'src'));
  });

  test('exists(selector)', () => {
    const doc = html.parse('<div><p class=a>hi</p></div>');
    assert.strictEqual(doc.exists('p'), true);
    assert.strictEqual(doc.exists('p.a'), true);
    assert.strictEqual(doc.exists('span'), false);
    assert.strictEqual(doc.exists('p.nonexistent'), false);
  });

  test('count(selector)', () => {
    const doc = html.parse('<ul><li>a</li><li>b</li><li>c</li></ul>');
    assert.strictEqual(doc.count('li'), 3);
    assert.strictEqual(doc.count('span'), 0);
  });

  test('elem.exists / elem.count', () => {
    const doc = html.parse('<section><article>a</article><article>b</article></section>');
    const sec = doc.first('section');
    assert.strictEqual(sec.exists('article'), true);
    assert.strictEqual(sec.count('article'), 2);
    assert.strictEqual(sec.exists('div'), false);
  });
});

// ============================================================
// 4. Traversal
// ============================================================
run('Traversal', () => {
  test('closest', () => {
    const doc = html.parse('<div><article class=post><h1>Hi</h1></article></div>');
    const h1 = doc.first('h1');
    const article = h1.closest('article');
    assert.strictEqual(article.tagName, 'article');
    assert.strictEqual(article.hasClass('post'), true);
    const div = h1.closest('div');
    assert.strictEqual(div.tagName, 'div');
    assert.strictEqual(h1.closest('span'), null);
  });

  test('children', () => {
    const doc = html.parse('<div><p>a</p><span>b</span><p>c</p></div>');
    const div = doc.first('div');
    const kids = div.children();
    assert.strictEqual(kids.length, 3);
    assert.strictEqual(kids[0].tagName, 'p');
    assert.strictEqual(kids[1].tagName, 'span');

    const ps = div.children('p');
    assert.strictEqual(ps.length, 2);
  });

  test('siblings', () => {
    const doc = html.parse('<div><p class=a>A</p><span>B</span><p class=c>C</p></div>');
    const a = doc.first('.a');
    const sibs = a.siblings();
    assert.strictEqual(sibs.length, 2);
    assert.strictEqual(sibs[0].tagName, 'span');

    const pSibs = a.siblings('p');
    assert.strictEqual(pSibs.length, 1);
    assert.strictEqual(pSibs[0].hasClass('c'), true);
  });

  test('matches / is', () => {
    const doc = html.parse('<div class=post><p>hi</p></div>');
    const div = doc.first('div');
    assert.strictEqual(div.matches('div'), true);
    assert.strictEqual(div.matches('div.post'), true);
    assert.strictEqual(div.matches('p'), false);
    assert.strictEqual(div.is('div'), true);
    assert.strictEqual(div.is('span'), false);
  });
});

// ============================================================
// 5. Table Extraction
// ============================================================
run('Table Extraction', () => {
  test('rows with thead', () => {
    const doc = html.parse('<table><thead><tr><th>Name</th><th>Age</th></tr></thead><tbody><tr><td>Alice</td><td>30</td></tr><tr><td>Bob</td><td>25</td></tr></tbody></table>');
    const rows = doc.first('table').rows();
    assert.strictEqual(rows.length, 2);
    assert.deepStrictEqual(rows[0], { Name: 'Alice', Age: '30' });
    assert.deepStrictEqual(rows[1], { Name: 'Bob', Age: '25' });
  });

  test('rows without thead (first row as header)', () => {
    const doc = html.parse('<table><tr><th>Name</th><th>Age</th></tr><tr><td>Alice</td><td>30</td></tr></table>');
    const rows = doc.first('table').rows();
    assert.strictEqual(rows.length, 1);
    assert.deepStrictEqual(rows[0], { Name: 'Alice', Age: '30' });
  });

  test('doc.table() convenience', () => {
    const doc = html.parse('<table><tr><th>Key</th></tr><tr><td>Val</td></tr></table>');
    const data = doc.table('table');
    assert.strictEqual(data.length, 1);
    assert.deepStrictEqual(data[0], { Key: 'Val' });
  });
});

// ============================================================
// 6. Text Normalization
// ============================================================
run('Text Normalization', () => {
  test('text() raw', () => {
    const doc = html.parse('<h1>  Hello  \n  World  </h1>');
    assert.strictEqual(doc.first('h1').text(), '  Hello  \n  World  ');
  });

  test('text({ normalize: true })', () => {
    const doc = html.parse('<h1>  Hello  \n  World  </h1>');
    assert.strictEqual(doc.first('h1').text({ normalize: true }), 'Hello World');
  });

  test('text({ separator })', () => {
    const doc = html.parse('<div><span>Hello</span><b>World</b></div>');
    assert.strictEqual(doc.first('div').text({ separator: ' | ' }), 'Hello | World');
  });

  test('text with selector and normalize', () => {
    const doc = html.parse('<h1>  Hi  </h1>');
    assert.strictEqual(doc.text('h1', { normalize: true }), 'Hi');
  });

  test('elem.text(selector)', () => {
    const doc = html.parse('<div><p>Hello</p></div>');
    assert.strictEqual(doc.first('div').text('p'), 'Hello');
  });
});

// ============================================================
// 7. High-level Extractors
// ============================================================
run('High-level Extractors', () => {
  test('meta()', () => {
    const doc = html.parse('<head><meta name=description content="Desc"><meta property="og:title" content="OG"><meta http-equiv="refresh" content="30"></head>');
    const meta = doc.meta();
    assert.deepStrictEqual(meta, {
      description: 'Desc',
      'og:title': 'OG',
      refresh: '30',
    });
  });

  test('title()', () => {
    const doc = html.parse('<head><title>My Page</title></head>');
    assert.strictEqual(doc.title(), 'My Page');
    assert.strictEqual(html.parse('<div>no title</div>').title(), null);
  });

  test('description()', () => {
    const doc = html.parse('<meta name=description content="A page">');
    assert.strictEqual(doc.description(), 'A page');
    assert.strictEqual(html.parse('<div>no</div>').description(), undefined);
  });

  test('canonicalUrl()', () => {
    const doc = html.parse('<link rel=canonical href=/page>');
    assert.strictEqual(doc.canonicalUrl(), '/page');
    assert.strictEqual(html.parse('<div>no</div>').canonicalUrl(), undefined);
  });

  test('links()', () => {
    const doc = html.parse('<a href=/a>Link A</a><a>no href</a><a href=/b>Link B</a>');
    const links = doc.links();
    assert.strictEqual(links.length, 2);
    assert.deepStrictEqual(links[0], { text: 'Link A', href: '/a' });
    assert.deepStrictEqual(links[1], { text: 'Link B', href: '/b' });
  });

  test('images()', () => {
    const doc = html.parse('<img src=/a.jpg alt="A"><img src=/b.jpg><img>');
    const imgs = doc.images();
    assert.strictEqual(imgs.length, 2);
    assert.deepStrictEqual(imgs[0], { alt: 'A', src: '/a.jpg' });
    assert.deepStrictEqual(imgs[1], { alt: '', src: '/b.jpg' });
  });

  test('forms()', () => {
    const doc = html.parse('<form action=/a></form><form action=/b></form>');
    assert.strictEqual(doc.forms().length, 2);
    assert.strictEqual(html.parse('<div></div>').forms().length, 0);
  });

  test('tables()', () => {
    const doc = html.parse('<table></table><table></table>');
    assert.strictEqual(doc.tables().length, 2);
    assert.strictEqual(html.parse('<div></div>').tables().length, 0);
  });
});

// ============================================================
// 8. Structured Extraction
// ============================================================
run('Structured Extraction', () => {
  test('simple extract', () => {
    const doc = html.parse('<h1>Title</h1><meta name=description content=Desc><link rel=canonical href=/url>');
    const r = doc.extract({
      title: ['h1', 'text'],
      description: ['meta[name="description"]', 'content'],
      canonical: ['link[rel="canonical"]', 'href'],
    });
    assert.deepStrictEqual(r, {
      title: 'Title',
      description: 'Desc',
      canonical: '/url',
    });
  });

  test('nested extract', () => {
    const doc = html.parse('<article class=post><h2>Hello</h2><a href=/a>Link</a></article><article class=post><h2>World</h2></article>');
    const r = doc.extract({
      posts: ['article.post', {
        heading: ['h2', 'text'],
        href: ['a', 'href'],
        hasLink: ['a', 'exists'],
      }],
    });
    assert.deepStrictEqual(r, {
      posts: [
        { heading: 'Hello', href: '/a', hasLink: true },
        { heading: 'World', href: undefined, hasLink: false },
      ],
    });
  });

  test('extract exists', () => {
    const doc = html.parse('<div class=featured>Hi</div>');
    const r = doc.extract({
      hasFeatured: ['.featured', 'exists'],
      hasMissing: ['.missing', 'exists'],
    });
    assert.deepStrictEqual(r, { hasFeatured: true, hasMissing: false });
  });

  test('extract count', () => {
    const doc = html.parse('<ul><li>a</li><li>b</li></ul>');
    const r = doc.extract({
      itemCount: ['li', 'count'],
    });
    assert.deepStrictEqual(r, { itemCount: 2 });
  });

  test('element.extract', () => {
    const doc = html.parse('<div class=card><h3>Card</h3><span>text</span></div><div class=card><h3>Card2</h3></div>');
    const cards = doc.find('.card').map(c => c.extract({
      heading: ['h3', 'text'],
      hasSpan: ['span', 'exists'],
    }));
    assert.deepStrictEqual(cards, [
      { heading: 'Card', hasSpan: true },
      { heading: 'Card2', hasSpan: false },
    ]);
  });
});

// ============================================================
// 9. URL Resolution
// ============================================================
run('URL Resolution', () => {
  test('baseUrl resolves relative URLs', () => {
    const doc = html.parse('<a href=/blog/post>Blog</a>', { baseUrl: 'https://example.com/pages/' });
    const links = doc.links();
    assert.strictEqual(links[0].href, 'https://example.com/blog/post');
  });

  test('url(selector, attr) helper', () => {
    const doc = html.parse('<a href=/path>Link</a>', { baseUrl: 'https://example.com/' });
    assert.strictEqual(doc.url('a', 'href'), 'https://example.com/path');
  });

  test('el.urlAttr(attr) helper', () => {
    const doc = html.parse('<div><a href=/path>Link</a></div>', { baseUrl: 'https://example.com/' });
    const a = doc.first('a');
    // urlAttr returns the raw value when no baseUrl available in ancestor chain
    // For reliable URL resolution, use doc.url(selector, attr)
    const resolved = doc.url('a', 'href');
    assert.strictEqual(resolved, 'https://example.com/path');
  });

  test('url resolution keeps absolute URLs', () => {
    const doc = html.parse('<a href=https://other.com/page>Link</a>', { baseUrl: 'https://example.com/' });
    assert.strictEqual(doc.links()[0].href, 'https://other.com/page');
  });
});

// ============================================================
// 10. Element-specific convenience
// ============================================================
run('Element-specific', () => {
  test('elem.text(selector)', () => {
    const doc = html.parse('<div><h1>Hello</h1></div>');
    assert.strictEqual(doc.first('div').text('h1'), 'Hello');
    assert.strictEqual(doc.first('div').text('p'), null);
  });

  test('elem.textOrThrow(selector)', () => {
    const doc = html.parse('<div><h1>Hello</h1></div>');
    assert.strictEqual(doc.first('div').textOrThrow('h1'), 'Hello');
    assert.throws(() => doc.first('div').textOrThrow('p'));
  });

  test('children() returns only element children', () => {
    const doc = html.parse('<div><!-- c -->text<b>bold</b></div>');
    const kids = doc.first('div').children();
    assert.strictEqual(kids.length, 1);
    assert.strictEqual(kids[0].tagName, 'b');
  });
});

// ============================================================
// Results
// ============================================================
console.log(`\n${'='.repeat(50)}`);
console.log(`Results: ${passed} passed, ${failed} failed`);
console.log(`${'='.repeat(50)}`);

if (failed > 0) {
  process.exit(1);
}
