'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');

const { parse } = require('..');

test('parse validates html input', () => {
  assert.throws(() => parse(), {
    name: 'TypeError',
    message: /html must be a string/,
  });
  assert.throws(() => parse(null), {
    name: 'TypeError',
    message: /html must be a string/,
  });
});

test('root parent and non-element helpers are safe', () => {
  const doc = parse('<div>x <span>y</span></div>');
  const root = doc.documentElement;
  const text = doc.find('div')[0].childNodes[0];

  assert.equal(root.parent, null);
  assert.equal(text.nodeType, 'TEXT');
  assert.equal(text.hasClass('x'), false);
  assert.equal(text.hasAttribute('id'), false);
  assert.equal(text.attr('id'), undefined);
});

test('selectors validate input and report bad selectors', () => {
  const doc = parse('<div></div>');

  assert.throws(() => doc.find(), {
    name: 'TypeError',
    message: /selector must be a string/,
  });
  assert.throws(() => doc.find('?'), /Bad selector/);
  assert.throws(() => doc.find('[foo'), /Bad selector/);
});

test('outerHTML includes content for implicit end tags', () => {
  assert.equal(parse('<p>One').find('p')[0].outerHTML, '<p>One');
  assert.deepEqual(
    parse('<p>One<p>Two').find('p').map((p) => p.outerHTML),
    ['<p>One', '<p>Two'],
  );
  assert.equal(
    parse('<html><body><p>One</body></html>').find('p')[0].outerHTML,
    '<p>One',
  );
});

test('adjacent sibling selectors skip non-element nodes', () => {
  const doc = parse('<h1>A</h1>\n<p>B</p><h2>C</h2><p>D</p>');

  assert.equal(doc.find('h1 + p').length, 1);
  assert.equal(doc.find('h2 + p').length, 1);
  assert.equal(doc.find('h1 ~ p').length, 2);
});

test('attribute selector semantics match HTML expectations', () => {
  assert.equal(parse('<div class="foo\tbar"></div>').find('.bar').length, 1);
  assert.equal(parse('<div class="foo\nbar"></div>').find('.bar').length, 1);

  const lang = parse('<div lang="en-US"></div><div lang="US"></div>');
  assert.equal(lang.find('[lang|=en]').length, 1);
  assert.equal(lang.find('[lang|=US]').length, 1);

  assert.equal(parse('<div DATA-X="1"></div>').find('[DATA-X]').length, 1);
});

test('document exposes document-level properties', () => {
  const doc = parse('<html><body><main><h1>Hello</h1></main></body></html>');

  assert.equal(doc.nodeType, 'DOCUMENT');
  assert.equal(doc.tagName, null);
  assert.equal(doc.innerText, 'Hello');
  assert.equal(doc.textContent, 'Hello');
  assert.equal(doc.outerHTML.startsWith('<html>'), true);
});

test('matches and traversal filters use the whole selector', () => {
  const doc = parse('<section><h1>Title</h1></section>');
  const h1 = doc.first_s('h1');

  assert.equal(h1.matches('div h1'), false);
  assert.equal(h1.matches('section h1'), true);
  assert.equal(h1.closest('section').tagName, 'section');
});

test('element urlAttr resolves using parse baseUrl', () => {
  const doc = parse('<a href="/path">Link</a>', {
    baseUrl: 'https://example.com/base/',
  });

  assert.equal(doc.first_s('a').urlAttr('href'), 'https://example.com/path');
});

test('table rows ignore tbody-only header rows when headers are known', () => {
  const doc = parse(`
    <table>
      <thead><tr><th>Name</th></tr></thead>
      <tbody>
        <tr><th>Group</th></tr>
        <tr><td>Alice</td></tr>
      </tbody>
    </table>
  `);

  assert.deepEqual(doc.table(), [{ Name: 'Alice' }]);
});
