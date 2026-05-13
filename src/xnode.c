#include "xnode_query.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

GumboVector *xnode_children(XNode *node) {
    if (!node) {
        return NULL;
    }

    switch (node->type) {
    case GUMBO_NODE_DOCUMENT:
        return &node->v.document.children;
    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE:
        return &node->v.element.children;
    default:
        return NULL;
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

static XNodeDocument *xnode_parse_html_real(const char *html, size_t length,
        int copy_html) {
    XNodeDocument *doc = xnew(XNodeDocument);

    if (copy_html) {
        doc->html = (char*) xmalloc(length + 1);
        memcpy(doc->html, html, length);
        doc->html[length] = '\0';
    } else {
        doc->html = (char*) html;
    }

    doc->html_length = length;
    doc->parse_options = kGumboDefaultOptions;
    doc->output = gumbo_parse_with_options(&doc->parse_options, doc->html, length);

    return doc;
}

XNodeDocument *xnode_parse_html(const char *html, size_t length) {
    return xnode_parse_html_real(html, length, 1);
}

const XNode *xnode_document_root(const XNodeDocument *document) {
    return document->output->root;
}

void xnode_document_free(XNodeDocument *doc) {
    gumbo_destroy_output(&doc->parse_options, doc->output);
    xfree(doc->html);
    xfree(doc);
}

const char *xnode_attr(const XNode *node, const char *name) {
    GumboAttribute *attr;
    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        if ((attr = gumbo_get_attribute(&node->v.element.attributes, name))) {
            return attr->value;
        }
    }
    return NULL;
}

int xnode_match_node(GumboNode *node, XNodeSelector *selector);

XNode *xnode_first_child(XNode *node, XNodeSelector *selector) {
    uint32_t i;
    GumboVector *children = xnode_children(node);

    if (!children) {
        return NULL;
    }

    for (i = 0; i < children->length; ++i) {
        GumboNode* child = (GumboNode*) children->data[i];
        if (child->type != GUMBO_NODE_ELEMENT &&
                child->type != GUMBO_NODE_TEMPLATE) continue;
        if (!selector || xnode_match_node(child, selector)) {
            return child;
        }
    }

    return NULL;
}

XNode *xnode_first_child_check(XNode *node, XNodeSelector *selector) {
    XNode *child = xnode_first_child(node, NULL);
    return child != NULL && xnode_match_node(child, selector) ? child : NULL;
}

XNode *xnode_parent(XNode *node) {
    return node->parent;
}

XNode *xnode_prev(XNode *node, XNodeSelector *selector) {
    GumboNode *parent = node->parent;
    GumboVector *children = xnode_children(parent);
    size_t i;

    if (!children) {
        return NULL;
    }

    for (i = node->index_within_parent; i > 0; i--) {
        node = (GumboNode*) children->data[i - 1];
        if (node->type != GUMBO_NODE_ELEMENT &&
                node->type != GUMBO_NODE_TEMPLATE) continue;
        if (!selector || xnode_match_node(node, selector)) {
            return node;
        }
    }

    return NULL;
}

XNode *xnode_prev_check(XNode *node, XNodeSelector *selector) {
    XNode *prev = xnode_prev(node, NULL);
    return prev != NULL && xnode_match_node(prev, selector) ? prev : NULL;
}

XNode *xnode_next(XNode *node, XNodeSelector *selector) {
    GumboNode *parent = node->parent;
    GumboVector *children = xnode_children(parent);
    size_t i = node->index_within_parent;

    if (!children) {
        return NULL;
    }

    for (; i + 1 < children->length; i++) {
        node = (GumboNode*) children->data[i + 1];
        if (node->type != GUMBO_NODE_ELEMENT &&
                node->type != GUMBO_NODE_TEMPLATE) continue;
        if (!selector || xnode_match_node(node, selector)) {
            return node;
        }
    }

    return NULL;
}

XNode *xnode_next_check(XNode *node, XNodeSelector *selector) {
    XNode *next = xnode_next(node, NULL);
    return next != NULL && xnode_match_node(next, selector) ? next : NULL;
}

static const char *max_ptr(const char *a, const char *b) {
    if (!a) {
        return b;
    }
    if (!b) {
        return a;
    }
    return b > a ? b : a;
}

static const char *min_ptr(const char *a, const char *b) {
    if (!a) {
        return b;
    }
    if (!b) {
        return a;
    }
    return b < a ? b : a;
}

static XNode *xnode_next_any(XNode *node) {
    GumboVector *children;

    if (!node || !node->parent) {
        return NULL;
    }

    children = xnode_children(node->parent);
    if (!children || node->index_within_parent + 1 >= children->length) {
        return NULL;
    }

    return (XNode*) children->data[node->index_within_parent + 1];
}

static const char *xnode_source_start(XNode *node) {
    if (!node) {
        return NULL;
    }

    switch (node->type) {
    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE:
        return node->v.element.original_tag.data;
    default:
        return node->v.text.original_text.data;
    }
}

static const char *xnode_implicit_end_boundary(XNode *node) {
    const char *start = node->v.element.original_tag.data;
    const char *boundary = xnode_source_start(xnode_next_any(node));
    XNode *parent = node->parent;

    while (parent) {
        if ((parent->type == GUMBO_NODE_ELEMENT ||
                    parent->type == GUMBO_NODE_TEMPLATE) &&
                parent->v.element.original_end_tag.data &&
                parent->v.element.original_end_tag.data > start) {
            boundary = min_ptr(boundary, parent->v.element.original_end_tag.data);
        }
        parent = parent->parent;
    }

    return boundary;
}

static const char *xnode_subtree_end(XNode *node) {
    const char *end = NULL;

    if (!node) {
        return NULL;
    }

    switch (node->type) {
    case GUMBO_NODE_DOCUMENT: {
        GumboVector *children = xnode_children(node);
        uint32_t i;
        for (i = 0; children && i < children->length; ++i) {
            end = max_ptr(end, xnode_subtree_end((XNode*) children->data[i]));
        }
        return end;
    }
    case GUMBO_NODE_ELEMENT:
    case GUMBO_NODE_TEMPLATE: {
        GumboElement *element = &node->v.element;
        GumboVector *children = xnode_children(node);
        uint32_t i;

        if (element->original_tag.data) {
            end = element->original_tag.data + element->original_tag.length;
        }

        for (i = 0; children && i < children->length; ++i) {
            end = max_ptr(end, xnode_subtree_end((XNode*) children->data[i]));
        }

        if (element->original_end_tag.data) {
            end = max_ptr(end,
                    element->original_end_tag.data + element->original_end_tag.length);
        }

        return end;
    }
    default:
        if (node->v.text.original_text.data) {
            return node->v.text.original_text.data + node->v.text.original_text.length;
        }
        return NULL;
    }
}

static const char *xnode_html_element(XNode *node, size_t *length) {
    GumboElement *element = &node->v.element;
    *length = 0;
    if (element->original_tag.length > 0) {
        const char *end = NULL;
        if (element->end_pos.offset > element->start_pos.offset) {
            end = element->original_tag.data +
                  (element->end_pos.offset - element->start_pos.offset);
            if (element->original_end_tag.length > 0) {
                end += element->original_end_tag.length;
            }
        }
        else {
            end = xnode_subtree_end(node);
        }
        if (element->original_end_tag.length == 0) {
            end = min_ptr(end, xnode_implicit_end_boundary(node));
        }
        if (end && end >= element->original_tag.data) {
            *length = end - element->original_tag.data;
        }
        if (*length == 0) {
            *length = element->original_tag.length;
        }
        return element->original_tag.data;
    }
    else {
        return "";
    }
}

const char *xnode_html(XNode *node, size_t *length) {
    switch(node->type) {
    case GUMBO_NODE_DOCUMENT:
        *length = 0;
        return "";
    case GUMBO_NODE_ELEMENT:
        return xnode_html_element(node, length);
    default:
        *length = node->v.text.original_text.length;
        return node->v.text.original_text.data;
    }
}

/**
 * Returns the text content of the first GUMBO_NODE_TEXT child.
 */
const char *xnode_text(XNode *node) {
    uint32_t i;

    for (i = 0; i < node->v.element.children.length; ++i) {
        GumboNode* child = (GumboNode*) node->v.element.children.data[i];
        if (child->type == GUMBO_NODE_TEXT) {
            return child->v.text.text;
        }
    }

    return NULL;
}

const char *xnode_get_tag_name(GumboNode *node, size_t *length);

const char *xnode_type(XNode *node, size_t *length) {
    return xnode_get_tag_name(node, length);
}

int xnode_has_class(XNode *node, const char *class_name) {
    GumboAttribute *attr;
    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        if ((attr = gumbo_get_attribute(&node->v.element.attributes, "class"))) {
            return string_in_space_list(attr->value, class_name) ? 1 : 0;
        }
    }
    return 0;
}

int xnode_has_attr(XNode *node, const char *name) {
    if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_TEMPLATE) {
        return gumbo_get_attribute(&node->v.element.attributes, name) ? 1 : 0;
    }
    return 0;
}
