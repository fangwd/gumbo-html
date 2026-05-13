'use strict';

const { parse } = require('..');

const html = `
<!doctype html>
<html>
  <body>
    <main id="content">
      <article class="post featured" data-slug="hello-world">
        <h1>Hello world</h1>
        <p class="summary">A short introduction.</p>
        <a class="cta primary" href="/hello">Read more</a>
      </article>

      <article class="post" data-slug="second-post">
        <h1>Second post</h1>
        <p class="summary">A follow-up note.</p>
        <a class="cta" href="/second">Open post</a>
      </article>
    </main>
  </body>
</html>
`;

const doc = parse(html);

// documentElement returns the parsed <html> element.
console.log('Root tag:', doc.documentElement.tagName);

// find(selector) returns every matching element under the document or element.
const posts = doc.find('article.post');
console.log('Post count:', posts.length);

posts.forEach((post, index) => {
  // attr(name) returns undefined when the attribute is missing.
  console.log(`Post ${index + 1}:`, post.attr('data-slug'));

  // first(selector) returns the first match or null.
  const title = post.first('h1');
  console.log('  title:', title ? title.innerText : '(missing)');

  // hasClass(name) and hasAttribute(name) are convenience checks.
  console.log('  featured:', post.hasClass('featured'));
  console.log('  has slug:', post.hasAttribute('data-slug'));
});

// first_s(selector) is the throwing version of first(selector).
// Use it when the element is required for the rest of your code.
const content = doc.first_s('#content');
console.log('Main outerHTML starts with:', content.outerHTML.slice(0, 20));

// only(selector) returns the match only when exactly one element is found.
const featuredPost = doc.only('article.featured');
console.log('Featured slug:', featuredPost.attr_s('data-slug'));

// only_s(selector) throws unless exactly one element is found.
try {
  doc.only_s('article.post');
} catch (error) {
  console.log('only_s on many posts:', error.message);
}

// Element-scoped queries search only inside that element.
const firstPost = doc.first_s('article.post');
console.log('CTA href:', firstPost.first_s('a.cta').attr_s('href'));

// next(selector) and prev(selector) walk element siblings.
const secondPost = firstPost.next('article.post');
console.log('Next post slug:', secondPost.attr_s('data-slug'));
console.log('Previous post slug:', secondPost.prev('article.post').attr_s('data-slug'));

// childNodes includes text/whitespace nodes as well as element nodes.
// nodeType helps distinguish them.
const childTypes = content.childNodes.map((node) => node.nodeType);
console.log('Main child node types:', childTypes.join(', '));

// parent returns the parent element, or null for the document root.
console.log('First post parent:', firstPost.parent.tagName);
console.log('Document root parent:', doc.documentElement.parent);

// attr_s(name) throws when an attribute is required but missing.
try {
  firstPost.attr_s('missing');
} catch (error) {
  console.log('attr_s on missing attribute:', error.message);
}
