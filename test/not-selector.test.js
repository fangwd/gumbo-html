'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');
const { parse } = require('..');

const ids = (elements) => elements.map((el) => el.attr('id'));

test('Tilecloud specification values match when data-name is absent', () => {
  const row = parse(`<table><tr>
    <td data-name="attribute-title">Material</td>
    <td>Porcelain</td>
  </tr></table>`).firstOrThrow('tr');
  const value = row.only('td:not([data-name="attribute-title"])');
  assert.equal(value.text({ normalize: true }), 'Porcelain');
  assert.equal(value.hasAttribute('data-name'), false);
});

test(':not supports type, universal, class, id, attribute and compound selectors', () => {
  const doc = parse(`<main>
    <p id="a" class="item skip" data-kind="tile">A</p>
    <p id="b" class="item" data-kind="stone">B</p>
    <span id="c" class="item">C</span>
  </main>`);
  assert.deepEqual(ids(doc.find('.item:not(p)')), ['c']);
  assert.deepEqual(ids(doc.find('.item:not(*)')), []);
  assert.deepEqual(ids(doc.find('.item:not(.skip)')), ['b', 'c']);
  assert.deepEqual(ids(doc.find('.item:not(#b)')), ['a', 'c']);
  assert.deepEqual(ids(doc.find('.item:not([data-kind])')), ['c']);
  assert.deepEqual(ids(doc.find('.item:not([data-kind^="ti"])')), ['b', 'c']);
  assert.deepEqual(ids(doc.find('.item:not(p.item[data-kind="tile"])')), ['b', 'c']);
  assert.deepEqual(ids(doc.find('main > :not(p)')), ['c']);
});

test('negations compose with outer selectors, combinators and other negations', () => {
  const doc = parse(`<main>
    <p id="a" class="item skip">A</p>
    <p id="b" class="item">B</p>
    <p id="c" class="item last">C</p>
  </main>`);
  assert.deepEqual(ids(doc.find('p:not(.skip).item:not(.last)')), ['b']);
  assert.deepEqual(ids(doc.find('main:not(.skip) > p:not(.skip) + p')), ['c']);
  assert.deepEqual(ids(doc.find('p:not(:not(.skip))')), ['a']);
  assert.deepEqual(ids(doc.find('p:NOT( .skip )')), ['b', 'c']);
});

test('a :not selector list excludes the union of its arguments', () => {
  const doc = parse('<p id="a" class="skip">A</p><p id="b">B</p><p id="c">C</p>');
  assert.deepEqual(ids(doc.find('p:not( .skip, #c )')), ['b']);
  assert.deepEqual(ids(doc.find('p:not(:not(.skip, #c))')), ['a', 'c']);
  assert.throws(() => doc.find('p, span'), /Bad selector/);
});

test(':not arguments match complete selector chains against the candidate element', () => {
  const doc = parse(`<section class="excluded"><p id="a">A</p></section>
    <main><p id="b">B</p><span></span><p id="c">C</p></main>`);
  assert.deepEqual(ids(doc.find('p:not(section p)')), ['b', 'c']);
  assert.deepEqual(ids(doc.find('p:not(section > p)')), ['b', 'c']);
  assert.deepEqual(ids(doc.find('p:not(span + p)')), ['a', 'b']);
  assert.deepEqual(ids(doc.find('p:not(p ~ p)')), ['a', 'b']);
  assert.deepEqual(ids(doc.find('p:not(section p, span + p)')), ['b']);
  // Ancestors outside the element query's context still determine a match.
  assert.deepEqual(doc.firstOrThrow('section').find('p:not(section p)'), []);
});

test('quoted attribute punctuation is not parsed as :not delimiters', () => {
  const doc = parse('<p id="a" data-label="a), (b">A</p><p id="b">B</p>');
  assert.deepEqual(ids(doc.find('p:not([data-label="a), (b"])')), ['b']);
});

test('matches, closest and traversal filters share negation semantics', () => {
  const doc = parse(`<main id="root"><section class="skip">
    <p id="a" class="skip">A</p>text<!-- comment --><p id="b">B</p><p id="c">C</p>
  </section></main>`);
  const b = doc.firstOrThrow('#b');
  const section = doc.firstOrThrow('section');
  assert.equal(b.matches('section > p:not(.skip)'), true);
  assert.equal(b.matches(':not(section > p)'), false);
  // closest() starts at the parent in gumbo-html.
  assert.equal(b.closest(':not(.skip)').attr('id'), 'root');
  assert.equal(b.closest('main:not(.skip)').attr('id'), 'root');
  assert.deepEqual(ids(section.children(':not(.skip)')), ['b', 'c']);
  assert.deepEqual(ids(b.siblings(':not(.skip)')), ['c']);
  assert.equal(doc.firstOrThrow('#a').next(':not(.skip)').attr('id'), 'b');
  assert.equal(doc.firstOrThrow('#c').prev(':not(.skip)').attr('id'), 'b');
});

test('empty, malformed and unsupported :not arguments throw', () => {
  const doc = parse('<p>Text</p>');
  for (const selector of [
    ':not', ':not(', ':not()', ':not( )', ':not(.a', ':not(.a))',
    ':not(, .a)', ':not(.a,)', ':not(.a,,.b)', ':not(> p)',
    ':not(p >)', ':not(p + )', ':not([x)', ':not([x="y])',
    ':not(:first-child)', ':not(::before)', ':is(p)', ':not (p)',
    ':not(.a) > :not()',
  ]) {
    assert.throws(() => doc.find(selector), /Bad selector/, selector);
  }
});

test('deeply nested negations are rejected without exhausting the native stack', () => {
  const doc = parse('<p id="a">A</p>');
  const nested = (depth) => ':not('.repeat(depth) + 'p' + ')'.repeat(depth);
  assert.deepEqual(ids(doc.find(`p${nested(32)}`)), ['a']);
  assert.throws(() => doc.find(nested(33)), /Bad selector/);
  // Parsing and cleanup remain usable after a rejected recursive selector.
  assert.equal(doc.find('p:not(.missing)').length, 1);
});

test('chained :not arguments match in polynomial time', { timeout: 5000 }, () => {
  const doc = parse('<div>'.repeat(50) + '<p id="a">A</p>' + '</div>'.repeat(50));
  const chain = (length) => Array(length).fill('div').join(' ');
  assert.deepEqual(ids(doc.find(`p:not(${chain(12)} p)`)), []);
  assert.deepEqual(ids(doc.find(`p:not(${chain(51)} p)`)), ['a']);
  // Depths 2-5 match the middle argument, so depth 1 and depths 6-50 remain.
  assert.equal(doc.find(`div:not(div div:not(${chain(6)}))`).length, 46);
  assert.equal(doc.firstOrThrow('#a').matches(`${chain(20)} p`), true);
  assert.equal(doc.firstOrThrow('#a').matches(`${chain(51)} p`), false);
});

test('long selector chains do not exhaust the native stack', { timeout: 5000 }, () => {
  const doc = parse('<main>' + '<p></p>'.repeat(10) + '<p id="last"></p></main>');
  const chain = 'p ~ '.repeat(100000) + 'p';
  assert.equal(doc.find(`p:not(${chain})`).length, 11);
  assert.equal(doc.firstOrThrow('#last').matches(chain), false);
  assert.equal(doc.firstOrThrow('#last').matches('p ~ '.repeat(10) + 'p'), true);
});

test('sibling chains over many siblings stay within result limits', { timeout: 5000 }, () => {
  const doc = parse('<main>' + '<p></p>'.repeat(2000) + '<p id="last"></p></main>');
  const last = doc.firstOrThrow('#last');
  assert.equal(last.matches('p ~ p ~ p'), true);
  assert.equal(last.matches('p:not(p ~ p ~ p)'), false);
  assert.equal(doc.find('p ~ p ~ p').length, 1999);
  assert.equal(doc.find('p:not(p ~ p ~ p)').length, 2);
  assert.equal(last.siblings(':not(p + p)').length, 1);
});

test('descendant chains over deep documents stay fast', { timeout: 5000 }, () => {
  const doc = parse('<div>'.repeat(300) + '<p id="a">A</p>' + '</div>'.repeat(300));
  const a = doc.firstOrThrow('#a');
  assert.equal(a.matches('div '.repeat(20) + 'p'), true);
  assert.equal(doc.find('div '.repeat(20) + 'p').length, 1);
  assert.equal(a.closest(':not(div div)').tagName, 'div');
  assert.equal(doc.find('div:not(div div div)').length, 2);
});

test('document queries and :not arguments see the same document tree', () => {
  const doc = parse('<p id="a">A</p>');
  const body = doc.firstOrThrow('body');
  assert.deepEqual(doc.find('html > body'), [body]);
  assert.equal(doc.first('html').tagName, 'html');
  assert.equal(body.matches('html > body'), true);
  assert.deepEqual(ids(doc.find('p:not(html > body p)')), []);
  assert.deepEqual(ids(doc.find('p:not(html > p)')), ['a']);
});

test('dangling combinators throw at the top level as inside :not', () => {
  const doc = parse('<p>Text</p>');
  for (const selector of ['p >', 'p > ', 'p +', 'p ~', ':not(p >)']) {
    assert.throws(() => doc.find(selector), /Bad selector/, selector);
  }
  assert.equal(doc.find('p ').length, 1);
});
