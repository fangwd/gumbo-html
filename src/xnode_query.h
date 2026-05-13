#ifndef XNODE_QUERY_H_
#define XNODE_QUERY_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    // 6.1. Type selector
    // h1 element in the document tree: h1
    XQSEL_TYPE,

    // 6.2. Universal selector
    // Represented by asterisk (*) any document type. '*' may be omitted in some cases.
    XQSEL_UNIVERSAL,

    // 6.3.1. Attribute presence and value selectors
    // An E element with a "foo" attribute: E[foo]
    XQSEL_ATTR_PRESENCE,

    // An E element whose "foo" attribute value is exactly equal to "bar":
    // E[foo="bar"]
    XQSEL_ATTR_EQUAL,

    // An E element whose "foo" attribute value is a list of whitespace-
    // separated values, one of which is exactly equal to "bar": E[foo~="bar"]
    XQSEL_ATTR_SPACE_MATCH,

    // 6.3.2. Substring matching attribute selectors
    // An E element whose "foo" attribute value begins exactly with the string
    // "bar": E[foo^="bar"]
    XQSEL_ATTR_PREFIX_MATCH,

    // An E element whose "foo" attribute value ends exactly with the string
    // "bar": E[foo$="bar"]
    XQSEL_ATTR_SUFFIX_MATCH,

    // An E element whose "foo" attribute value contains the substring "bar":
    // E[foo*="bar"]
    XQSEL_ATTR_SUBSTRING_MATCH,

    // An E element whose "foo" attribute has a hyphen-separated list of values
    // beginning (from the left) with "en": E[foo|="en"]
    XQSEL_ATTR_DASH_MATCH,

    // 6.4. Class selectors
    // All elements with class~=pastoral: *.pastoral or just .pastoral
    XQSEL_CLASS,

    // 6.5. ID selectors
    // Examples: h1#chapter1, #chapter1
    XQSEL_ID,

    // 6.6. Pseudo-classes (NOT SUPPORTED)
    // A pseudo-class always consists of a "colon" (:) followed by the name of
    // the pseudo-class and optionally by a value between parentheses.
    // E.g. a:link, tr:nth-child(2n+1), :last-child, :first-of-type, etc.
    XQSEL_PSEUDO_CLASS,

    // 7. Pseudo-elements (NOT SUPPORTED)
    // E.g. ::first-line, ::first-letter, ::before, ::after
    XQSEL_PSEUDO_ELEMENT,

    // 8. Combinators
    // 8.1. Descendant combinator
    // E.g. h1 em, div * p
    XQSEL_DESCENDANT,

    // 8.2. Child combinators
    // E.g. body > p
    XQSEL_CHILD,

    // 8.3. Sibling combinators
    // 8.3.1. Adjacent sibling combinator
    // E.g. math + p (p element immediately following a math element)
    XQSEL_ADJACENT_SIBLING,

    // 8.3.2. General sibling combinator
    // The elements represented by the two sequences share the same parent
    // in the document tree and the element represented by the first sequence
    // precedes (not necessarily immediately) the element represented by the
    // second one. E.g.  h1 ~ pre (pre element following an h1)
    XQSEL_GENERAL_SIBLING,

    XQSEL_UNKNOWN_TYPE
} XNodeQueryType;

#include "gumbo.h"
#include "jsa.h"

typedef struct {
    // One of the XQSEL_ATTR_* types
    XNodeQueryType type;

    // Attribute name
    const char *name;

    // Attribute value, if applicable
    const char *value;
} XNodeAttributeSelector;

// 4. Selector syntax
//
// A selector is a chain of one or more sequences of simple selectors separated
// by combinators. One pseudo-element may be appended to the last sequence of
// simple selectors in a selector.
//
// A sequence of simple selectors is a chain of simple selectors that are not
// separated by a combinator. It always begins with a type selector or a
// universal selector. No other type selector or universal selector is allowed
// in the sequence.
//
// A simple selector is either a type selector, universal selector, attribute
// selector, class selector, ID selector, or pseudo-class.
//
typedef struct {
    // Tag name for type selector, "*" for universal selector
    const char *type;

    // A list of attribute selectors; class selectors are translated as
    // a list of {"class", "name", XQSEL_ATTR_SPACE_MATCH} selectors.
    jsa_t attributes;

    // Relation of this selector and the previous one, if any.
    XNodeQueryType combinator;
} XNodeSelector;

typedef struct {
    jsa_t selectors;
    char *data;
    jsa_t result[2];
} XNodeQuery;

typedef GumboNode XNode;

typedef jsa_t XNodeArray;
#define xnode_array_first(array)   ((XNode*)jsa_get((array),0))
#define xnode_array_get(array,n)   ((XNode*)jsa_get((array),(n)))
#define xnode_array_size(array)    jsa_size(array)

typedef struct {
    GumboOutput *output;
    GumboOptions parse_options;
    char *html;
    size_t html_length;
} XNodeDocument;

XNodeQuery       *xnode_query_create(const char *, const char **, int *);
const XNodeArray *xnode_query_execute(XNodeQuery *, const XNode*);
void              xnode_query_free(XNodeQuery *);
XNodeSelector    *xnode_query_selector(XNodeQuery*, uint32_t);

#define xnode_query_get_selector(q,i) ((XNodeSelector*)jsa_get(&q->selectors,i))

XNodeDocument *xnode_parse_html(const char *html, size_t length);
const XNode   *xnode_document_root(const XNodeDocument*);
void           xnode_document_free(XNodeDocument*);

const char    *xnode_attr(const XNode*, const char *name);

XNode *xnode_first_child(XNode *node, XNodeSelector *selector);
XNode *xnode_parent(XNode *node);
XNode *xnode_prev(XNode *node, XNodeSelector *selector);
XNode *xnode_prev_check(XNode *node, XNodeSelector *selector);
XNode *xnode_next(XNode *node, XNodeSelector *selector);
XNode *xnode_next_check(XNode *node, XNodeSelector *selector);
const char *xnode_html(XNode *node, size_t *length);
const char *xnode_text(XNode *xnode);
const char *xnode_type(XNode *node, size_t *length);
int xnode_has_class(XNode *xnode, const char *class_name);
int xnode_has_attr(XNode *xnode, const char *name);

int xnode_match_node(GumboNode *node, XNodeSelector *selector);
GumboVector *xnode_children(XNode *node);

// utils
char *strtok_simple(const char *str, const char *tok, char sep);
#define xmalloc malloc
#define xnew(type) ((type*)xmalloc(sizeof (type)))
#define xfree free

#ifdef __cplusplus
}
#endif

#endif // XNODE_QUERY_H_
