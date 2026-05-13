'use strict';

/**
 * gumbo-html examples
 *
 * Demonstrates all the new features including:
 * - Friendly aliases (firstOrThrow, onlyOrThrow, attrOrThrow)
 * - Convenience methods (exists, count, text, attr with selector)
 * - Traversal (closest, children, siblings, matches, is)
 * - Table extraction (rows, table)
 * - Text normalization
 * - Structured extraction
 * - URL resolution with baseUrl
 * - Common extractors (meta, links, images, title, etc.)
 */

const html = require('..');

// ============================================================
// Parse a HTML page
// ============================================================
const HTML = `
<!DOCTYPE html>
<html>
<head>
  <title>Example Blog</title>
  <meta name="description" content="A blog about web scraping">
  <meta property="og:title" content="Example Blog OG">
  <meta property="og:image" content="/images/og.png">
  <link rel="canonical" href="https://example.com/blog/">
</head>
<body>
  <article class="post featured">
    <h1>  Getting Started with Web Scraping  </h1>
    <p>First paragraph of content.</p>
    <a href="/post/getting-started">Read More</a>
    <img src="/images/scraping.png" alt="Web Scraping">
    <ul class="tags">
      <li>scraping</li>
      <li>html</li>
      <li>tutorial</li>
    </ul>
  </article>

  <article class="post">
    <h1>Advanced CSS Selectors</h1>
    <p>Learn about complex selectors.</p>
    <a href="/post/advanced-selectors">Read More</a>
    <ul class="tags">
      <li>css</li>
      <li>selectors</li>
    </ul>
  </article>

  <section class="sidebar">
    <h2>Popular Posts</h2>
    <ul>
      <li><a href="/popular/1">How to Use CSS Selectors</a></li>
      <li><a href="/popular/2">HTML Parsing Guide</a></li>
    </ul>
  </section>

  <table class="pricing">
    <thead>
      <tr><th>Plan</th><th>Price</th><th>Features</th></tr>
    </thead>
    <tbody>
      <tr><td>Basic</td><td>$10/mo</td><td>100 requests</td></tr>
      <tr><td>Pro</td><td>$30/mo</td><td>1000 requests</td></tr>
      <tr><td>Enterprise</td><td>$100/mo</td><td>Unlimited</td></tr>
    </tbody>
  </table>

  <form action="/search" method="get">
    <input type="text" name="q" placeholder="Search...">
    <button type="submit">Go</button>
  </form>
</body>
</html>
`;

const doc = html.parse(HTML, { baseUrl: 'https://example.com/blog/' });

// ============================================================
// 1. Friendly Required/Optional Aliases
// ============================================================
console.log('=== 1. Friendly Aliases ===');

// firstOrThrow - like first_s but more readable
const firstArticle = doc.firstOrThrow('article');
console.log('firstOrThrow article text:', firstArticle.text('h1'));

// onlyOrThrow - returns single element or throws
const sidebar = doc.onlyOrThrow('.sidebar');
console.log('onlyOrThrow .sidebar heading:', sidebar.text('h2'));

// attrOrThrow - get attribute or throw
const firstLink = doc.firstOrThrow('a');
console.log('attrOrThrow href:', firstLink.attrOrThrow('href'));

// textOrThrow - find element and get text or throw
console.log('textOrThrow h1:', doc.textOrThrow('h1'));

// ============================================================
// 2. Selector-Scoped Convenience Methods
// ============================================================
console.log('\n=== 2. Convenience Methods ===');

// exists(selector) - check if any element matches
console.log('exists .featured:', doc.exists('.featured'));         // true
console.log('exists .missing:', doc.exists('.missing'));           // false

// count(selector) - count matching elements
console.log('count article:', doc.count('article'));               // 2
console.log('count li:', doc.count('li'));                         // 5

// text(selector) - get text of first match (null if not found)
console.log('text h1:', JSON.stringify(doc.text('h1')));           // "Getting Started..."

// attr(selector, name) - get attribute of first match
console.log('attr meta[property] content:', doc.attr('meta[property="og:title"]', 'content'));

// attrOrThrow(selector, name) - get attribute or throw
console.log('attrOrThrow a href:', doc.attrOrThrow('a', 'href'));

// ============================================================
// 3. Text Normalization
// ============================================================
console.log('\n=== 3. Text Normalization ===');

const h1 = doc.firstOrThrow('h1');

// raw text (default)
console.log('raw text:', JSON.stringify(h1.text()));

// normalized (trimmed + collapsed whitespace)
console.log('normalized:', JSON.stringify(h1.text({ normalize: true })));

// separator (join descendant text with custom separator)
console.log('separator:', JSON.stringify(h1.text({ separator: ' | ' })));

// ============================================================
// 4. Traversal
// ============================================================
console.log('\n=== 4. Traversal ===');

// closest - find nearest ancestor matching selector
const firstH1 = doc.firstOrThrow('h1');
const article = firstH1.closest('article');
console.log('h1.closest(article) tag:', article.tagName);
console.log('h1.closest(article) class:', article.attr('class'));

// children - get direct element children (optionally filtered)
const allChildren = article.children();
console.log('article children count:', allChildren.length);

const headingChildren = article.children('h1');
console.log('article children(h1):', headingChildren.length);

// siblings - get sibling elements (optionally filtered)
const firstP = article.firstOrThrow('p');
const siblingElements = firstP.siblings();
console.log('p siblings count:', siblingElements.length);

const linkSiblings = firstP.siblings('a');
console.log('p siblings(a):', linkSiblings.length);

// matches / is - check if element matches a selector
console.log('h1 matches article h1:', firstH1.matches('article h1'));
console.log('h1 matches div:', firstH1.matches('div'));
console.log('h1 is(h1):', firstH1.is('h1'));

// ============================================================
// 5. Table Extraction
// ============================================================
console.log('\n=== 5. Table Extraction ===');

// rows() - extract table rows as objects
const pricingRows = doc.firstOrThrow('.pricing').rows();
console.log('pricing table rows:');
for (const row of pricingRows) {
  console.log(`  ${row.Plan}: ${row.Price} (${row.Features})`);
}

// doc.table() - convenience: find table and extract rows
const tableData = doc.table('.pricing');
console.log('doc.table direct:', JSON.stringify(tableData));

// ============================================================
// 6. Structured Extraction
// ============================================================
console.log('\n=== 6. Structured Extraction ===');

const extracted = doc.extract({
  title: ['h1', 'text'],
  canonicalUrl: ['link[rel="canonical"]', 'href'],
  hasFeatured: ['.featured', 'exists'],

  articles: ['article.post', {
    heading: ['h1', 'text'],
    link: ['a', 'href'],
    hasImage: ['img', 'exists'],
    tags: ['ul.tags li', 'text'],
  }],
});

console.log(JSON.stringify(extracted, null, 2));

// Element-level extract
const articleEl = doc.firstOrThrow('article');
const articleData = articleEl.extract({
  heading: ['h1', 'text'],
  tagCount: ['li', 'count'],
});
console.log('elem.extract:', JSON.stringify(articleData));

// ============================================================
// 7. URL Resolution
// ============================================================
console.log('\n=== 7. URL Resolution ===');

// baseUrl resolves relative URLs in links() and images()
const links = doc.links();
console.log('links (with baseUrl):');
for (const l of links) {
  console.log(`  "${l.text}" -> ${l.href}`);
}

const imgs = doc.images();
console.log('images (with baseUrl):');
for (const img of imgs) {
  console.log(`  alt="${img.alt}" src="${img.src}"`);
}

// doc.url(selector, attr) - resolve a specific URL
console.log('url(a, href):', doc.url('a', 'href'));

// ============================================================
// 8. Common Extractors
// ============================================================
console.log('\n=== 8. Common Extractors ===');

// Page title
console.log('title():', doc.title());

// Meta description
console.log('description():', doc.description());

// Canonical URL
console.log('canonicalUrl():', doc.canonicalUrl());

// Meta tags object
console.log('meta():', JSON.stringify(doc.meta()));

// Forms
console.log('forms count:', doc.forms().length);

// Tables
console.log('tables count:', doc.tables().length);

// ============================================================
// 9. Worked Example: Real-World Scraping
// ============================================================
console.log('\n=== 9. Real-World Scraping Example ===');

/**
 * Extract structured data from a blog page with one call.
 */
function scrapeBlogPage(htmlContent, pageUrl) {
  const doc = html.parse(htmlContent, { baseUrl: pageUrl });

  return doc.extract({
    // Page-level info
    pageTitle: ['title', 'text'],
    metaDescription: ['meta[name="description"]', 'content'],
    ogImage: ['meta[property="og:image"]', 'content'],
    canonicalUrl: ['link[rel="canonical"]', 'href'],

    // Content
    posts: ['article.post', {
      heading: ['h1', 'text'],
      link: ['a', 'href'],
      hasImage: ['img', 'exists'],
      isFeatured: ['.featured', 'exists'],
      tagCount: ['ul.tags li', 'count'],
    }],

    // Sidebar links
    sidebarLinks: ['.sidebar a', 'text'],

    // Stats
    totalArticles: ['article.post', 'count'],
    hasForm: ['form', 'exists'],
    hasPricing: ['.pricing', 'exists'],
  });
}

const result = scrapeBlogPage(HTML, 'https://example.com/blog/');
console.log(JSON.stringify(result, null, 2));

console.log('\nAll examples completed successfully!');
