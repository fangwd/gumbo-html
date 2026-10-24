#include "xnode_query.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp(x,y) _stricmp(x,y)
#define strncasecmp(x,y,z) _strnicmp(x,y,z)
#else
#include <strings.h>
#endif

static int string_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) {
        return 0;
    } else {
        size_t lenstr = strlen(str);
        size_t lensuffix = strlen(suffix);
        if (lensuffix > lenstr) return 0;
        return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
    }
}

static int is_html_space(char c) {
    return isspace((unsigned char)c);
}

static int string_in_space_list(const char *str, const char *tok) {
    size_t tok_len;

    if (!str || !tok || *tok == '\0') {
        return 0;
    }

    tok_len = strlen(tok);
    while (*str) {
        while (*str && is_html_space(*str)) {
            str++;
        }

        if (!strncmp(str, tok, tok_len) &&
                (str[tok_len] == '\0' || is_html_space(str[tok_len]))) {
            return 1;
        }

        while (*str && !is_html_space(*str)) {
            str++;
        }
    }

    return 0;
}

static int string_dash_match(const char *str, const char *tok) {
    size_t tok_len;

    if (!str || !tok || *tok == '\0') {
        return 0;
    }

    tok_len = strlen(tok);
    return !strcmp(str, tok) || (!strncmp(str, tok, tok_len) && str[tok_len] == '-');
}

/**
 * Evaluate an attribute expression against an attribute.
 */
static int xnode_match_attribute(GumboAttribute *attribute,
        XNodeAttributeSelector *selector) {

    if (strcasecmp(attribute->name, selector->name) == 0) {
        switch (selector->type) {
        case XQSEL_ATTR_PRESENCE:
            return 1;
        case XQSEL_ATTR_EQUAL:
            return !strcmp(attribute->value, selector->value);
        case XQSEL_ATTR_SPACE_MATCH:     // ~=
            return string_in_space_list(attribute->value, selector->value);
        case XQSEL_ATTR_DASH_MATCH:      // |=
            return string_dash_match(attribute->value, selector->value);
        case XQSEL_ATTR_PREFIX_MATCH:    // ^=
            return !strncmp(selector->value, attribute->value, strlen(selector->value));
        case XQSEL_ATTR_SUFFIX_MATCH:    // $=
            return string_ends_with(attribute->value, selector->value);
        case XQSEL_ATTR_SUBSTRING_MATCH: // *=
            return strstr(attribute->value, selector->value) != NULL;
        default:
            assert(0);
            break;
        }
    }

    return 0;
}

const char *xnode_get_tag_name(GumboNode *node, size_t *length) {
    const char *data;

    if (node->type == GUMBO_NODE_DOCUMENT) {
        data = "document";
        *length = sizeof("document") - 1;
    }
    else {
        data = gumbo_normalized_tagname(node->v.element.tag);
        *length = strlen(data);
    }

    if (*length == 0) {
        GumboStringPiece tag = node->v.element.original_tag;
        if (tag.data != NULL) {
            gumbo_tag_from_original_text(&tag);
            data = tag.data;
            *length = tag.length;
        }
        else {
            data = NULL;
            *length = 0;
        }
    }

    return data;
}

static int xnode_match_node_type(GumboNode *node, const char *type) {
    const char *name;
    size_t length;

    if ((name = xnode_get_tag_name(node, &length))) {
        if (!strcmp(type, "*")) {
            return 1;
        }
        else {
            if (strlen(type) == length) {
                return !strncasecmp(name, type, length);
            }
        }
    }

    return 0;
}

static int xnode_negation_matches(XNodeQuery *query, GumboNode *node);

int xnode_match_node(GumboNode *node, XNodeSelector *node_selector) {
    GumboNodeType     type = node->type;
    GumboVector      *attributes;
    uint32_t          i, j;

    if (type != GUMBO_NODE_ELEMENT && type != GUMBO_NODE_TEMPLATE) {
        return 0;
    }

    if (!xnode_match_node_type(node, node_selector->type)) {
        return 0;
    }

    attributes = &node->v.element.attributes;

    for (i = 0; i < jsa_size(&node_selector->attributes); i++) {
        XNodeAttributeSelector *selector;
        selector = (XNodeAttributeSelector*) jsa_get(&node_selector->attributes, i);
        for (j = 0; j < attributes->length; ++j) {
            GumboAttribute* attribute = (GumboAttribute*) attributes->data[j];
            if (xnode_match_attribute(attribute, selector)) break;
        }
        if (j == attributes->length) {
            return 0;
        }
    }

    for (i = 0; i < jsa_size(&node_selector->negations); i++) {
        XNodeQuery *query = (XNodeQuery *)jsa_get(&node_selector->negations, i);
        if (xnode_negation_matches(query, node)) return 0;
    }

    return 1;
}

static int compare_nodes(const void *a, const void *b) {
    JSA_TYPE x = *(const JSA_TYPE *)a, y = *(const JSA_TYPE *)b;
    return (x > y) - (x < y);
}

static GumboNode *xnode_document_of(GumboNode *node) {
    while (node->parent) node = node->parent;
    return node;
}

// Chained queries are matched once against the whole document and cached;
// matching them node by node grows exponentially with the chain length.
static void xnode_query_match_document(XNodeQuery *query, GumboNode *document) {
    if (query->matched_document == document) return;
    query->matched = (jsa_t *)xnode_query_execute(query, document);
    if (query->matched) {
        qsort(query->matched->item, query->matched->size, sizeof(JSA_TYPE),
            compare_nodes);
    }
    query->matched_document = document;
}

static int xnode_query_contains(XNodeQuery *query, GumboNode *node) {
    JSA_TYPE key = (JSA_TYPE)node;
    xnode_query_match_document(query, xnode_document_of(node));
    return query->matched && bsearch(&key, query->matched->item,
        query->matched->size, sizeof(JSA_TYPE), compare_nodes) != NULL;
}

static int xnode_negation_matches(XNodeQuery *query, GumboNode *node) {
    return query->selectors.size == 1 ?
        xnode_match_node(node, xnode_query_selector(query, 0)) :
        xnode_query_contains(query, node);
}

// Matches chained :not() arguments before a walk, so that walks never nest.
static void xnode_query_prepare(XNodeQuery *query, GumboNode *document) {
    uint32_t i, j;
    if (query->prepared_document == document) return;
    for (i = 0; i < query->selectors.size; i++) {
        XNodeSelector *selector = xnode_query_selector(query, i);
        for (j = 0; j < selector->negations.size; j++) {
            XNodeQuery *negation = (XNodeQuery *)jsa_get(&selector->negations, j);
            if (negation->selectors.size == 1) {
                xnode_query_prepare(negation, document);
            } else {
                xnode_query_match_document(negation, document);
            }
        }
    }
    query->prepared_document = document;
}

int xnode_query_matches(XNodeQuery *query, XNode *node) {
    if (!node || query->selectors.size == 0) return 0;
    if (query->selectors.size > 1) return xnode_query_contains(query, node);
    xnode_query_prepare(query, xnode_document_of(node));
    return xnode_match_node(node, xnode_query_selector(query, 0));
}

// Open-addressing set of nodes; slot value 0 marks an empty slot.
typedef struct {
    JSA_TYPE *slots;
    uint32_t  capacity;
    uint32_t  size;
} XNodeSet;

static uint32_t xnode_set_slot(const XNodeSet *set, JSA_TYPE key) {
    uint32_t mask = set->capacity - 1;
    uint32_t i = (uint32_t)(key >> 4) * 2654435761u & mask;
    while (set->slots[i] && set->slots[i] != key) i = (i + 1) & mask;
    return i;
}

// Returns 0 if the node is already in the set.
static int xnode_set_add(XNodeSet *set, GumboNode *node) {
    JSA_TYPE key = (JSA_TYPE)node;
    uint32_t i;

    if (set->size * 2 >= set->capacity) {
        XNodeSet grown;
        grown.capacity = set->capacity ? set->capacity * 2 : 64;
        grown.slots = (JSA_TYPE *)calloc(grown.capacity, sizeof(JSA_TYPE));
        grown.size = set->size;
        for (i = 0; i < set->capacity; i++) {
            if (set->slots[i]) grown.slots[xnode_set_slot(&grown, set->slots[i])] = set->slots[i];
        }
        xfree(set->slots);
        *set = grown;
    }

    i = xnode_set_slot(set, key);
    if (set->slots[i]) return 0;
    set->slots[i] = key;
    set->size++;
    return 1;
}

static void xnode_set_clear(XNodeSet *set) {
    if (set->slots) memset(set->slots, 0, set->capacity * sizeof(JSA_TYPE));
    set->size = 0;
}

static void xnode_match_node_descendant(GumboNode* node, jsa_t *next_nodes,
        XNodeSet *seen, XNodeSelector *selector) {
    GumboVector *children = xnode_children(node);
    uint32_t i;
    for (i = 0; children && i < children->length; ++i) {
        GumboNode* child = (GumboNode*) children->data[i];
        if (xnode_match_node(child, selector)) {
            // An earlier walk already covered this subtree.
            if (!xnode_set_add(seen, child)) continue;
            jsa_push(next_nodes, child);
        }
        xnode_match_node_descendant(child, next_nodes, seen, selector);
    }
}

static void xnode_match_node_child(GumboNode* node, jsa_t *next_nodes,
        XNodeSet *seen, XNodeSelector *selector) {
    GumboVector *children = xnode_children(node);
    uint32_t i;
    for (i = 0; children && i < children->length; ++i) {
        GumboNode* child = (GumboNode*) children->data[i];
        if (xnode_match_node(child, selector) && xnode_set_add(seen, child)) {
            jsa_push(next_nodes, child);
        }
    }
}

static void xnode_match_node_adjacent_sibling(GumboNode* node,
        jsa_t *next_nodes, XNodeSet *seen, XNodeSelector *selector) {
    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        GumboNode *next = xnode_next(node, NULL);
        if (next && xnode_match_node(next, selector) && xnode_set_add(seen, next)) {
            jsa_push(next_nodes, next);
        }
    }
}

static void xnode_match_node_general_sibling(GumboNode* node, jsa_t *next_nodes,
        XNodeSet *seen, XNodeSelector *selector) {

    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        while ((node = xnode_next(node, NULL))) {
            if (xnode_match_node(node, selector)) {
                // An earlier walk already covered the remaining siblings.
                if (!xnode_set_add(seen, node)) break;
                jsa_push(next_nodes, node);
            }
        }
    }
}

/*
#include <stdio.h>
static void xnode_dump(XNode *node) {
    size_t name_size, html_size;
    const char *name = xnode_type(node, &name_size);
    const char *html = xnode_html(node, &html_size);
    const char *id = xnode_attr(node, "id");
    const char *Class = xnode_attr(node, "class");
    printf("%s\t%s\t%s\t%.*s\n", name, id ? id : "", Class ? Class : "",
        (int) html_size, html);
}

static void dump_query_result(const XNodeArray *result) {
    if (result == NULL) {
        printf("<null>\n");
        return;
    }
    else {
        uint32_t i;
        printf("%u element(s):\n", result->size);
        for (i = 0; i < result->size; i++) {
            xnode_dump((XNode*) jsa_get(result, i));
        }
    }
}
*/
#define SWAP(x, y) do { jsa_t *SWAP = x; x = y; y = SWAP; } while (0)

const XNodeArray *xnode_query_execute(XNodeQuery *query, const XNode *node) {
    uint32_t i, j;
    jsa_t *prev_nodes = &query->result[0];
    jsa_t *next_nodes = &query->result[1];
    XNodeSet seen = { NULL, 0, 0 };

    xnode_query_prepare(query, xnode_document_of((GumboNode *)node));
    // The result buffers are about to be reused.
    query->matched_document = NULL;
    prev_nodes->size = 0;
    jsa_push(prev_nodes, node);

    for (i = 0; i < query->selectors.size; i++) {
        XNodeSelector *selector = xnode_query_get_selector(query, i);

        next_nodes->size = 0;
        xnode_set_clear(&seen);

        for (j = 0; j < jsa_size(prev_nodes); j++) {
            GumboNode *node = (GumboNode*) jsa_get(prev_nodes, j);
            switch (selector->combinator) {
            case XQSEL_DESCENDANT:
                xnode_match_node_descendant(node, next_nodes, &seen, selector);
                break;
            case XQSEL_CHILD:
                xnode_match_node_child(node, next_nodes, &seen, selector);
                break;
            case XQSEL_ADJACENT_SIBLING:
                xnode_match_node_adjacent_sibling(node, next_nodes, &seen, selector);
                break;
            case XQSEL_GENERAL_SIBLING:
                xnode_match_node_general_sibling(node, next_nodes, &seen, selector);
                break;
            default:
                assert(0);
                break;
            }
        }

        // dump_query_result(next_nodes);
        SWAP(prev_nodes, next_nodes);
    }

    xfree(seen.slots);

    return jsa_size(prev_nodes) > 0 ? prev_nodes : NULL;
}

XNodeSelector *xnode_query_selector(XNodeQuery *query, uint32_t n) {
    return (XNodeSelector*) jsa_get(&query->selectors, n);
}

/**
 * Locate the first instance of \btok\b in str, where \b = sep.
 */
char *strtok_simple(const char *str, const char *tok, char sep) {
    const char *a, *b;
    int flag;

    b = tok;

    if (*b == 0) {
        return (char*) str;
    }

    for (flag = 1; *str != 0; str += 1) {
        if (*str != *b || !flag) {
            flag = sep ? (*str == sep) : (isspace(*str) || ispunct(*str));
            continue;
        }
        a = str;
        while (1) {
            if (*b == 0) {
                if (*a == 0 || (sep ? (*a == sep) : (isspace(*a)) || ispunct(*a))) {
                    return (char*) str;
                } else {
                    break;
                }
            }
            if (*a++ != *b++) {
                break;
            }
        }
        b = tok;
    }
    return (char *) 0;
}
