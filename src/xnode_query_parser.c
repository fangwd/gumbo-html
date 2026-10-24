#include "xnode_query.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define XNODE_BAD_NAME_CHARACTER       "Illegal name character"
#define XNODE_BAD_COMBINATOR           "Illegal combinator"
#define XNODE_EMPTY_SELECTOR           "Empty selector"
#define XNODE_UNEXPECTED_CHAR          "Illegal character"
#define XNODE_QUOTATION_MARK_EXPECTED  "Missing quotation mark"
#define XNODE_UNSUPPORTED_PSEUDO_CLASS "Unsupported pseudo-class"
#define XNODE_NEGATION_TOO_DEEP        "Too many nested :not()"
#define XNODE_MAX_NEGATION_DEPTH       32

#define xnew_zero(type) ((type*)calloc(1, sizeof (type)))

typedef struct {
    const char    *next;    // Next input char
    char          *data;    // Data buffer
    const char    *error;   // Error message
    unsigned int   negation_depth;
} XNodeQueryParser;

// w  [ \t\r\n\f]
static inline int is_space(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static inline char skip_space(XNodeQueryParser *parser) {
    while (*parser->next && is_space(*parser->next)) {
        parser->next++;
    }
    return *parser->next;
}

// combinator
//   /* combinators can be surrounded by whitespace */
//   : PLUS S* | GREATER S* | TILDE S* | S+
//   ;
XNodeQueryType xnode_parse_combinator(XNodeQueryParser *parser) {
    XNodeQueryType result = XQSEL_UNKNOWN_TYPE;

    if (is_space(*parser->next)) {
        // E F: an F element descendant of an E element
        result = XQSEL_DESCENDANT;
        skip_space(parser);
    }

    switch (*parser->next) {
    case '>': // E > F: an F element child of an E element
        result = XQSEL_CHILD;
        break;
    case '+': // E + F: an F element immediately preceded by an E element
        result = XQSEL_ADJACENT_SIBLING;
        break;
    case '~': // E ~ F:   an F element preceded by an E element
        result = XQSEL_GENERAL_SIBLING;
        break;
    default:
        break;
    }

    if (result == XQSEL_UNKNOWN_TYPE) {
        parser->error = XNODE_BAD_COMBINATOR;
        return XQSEL_UNKNOWN_TYPE;
    }

    if (result != XQSEL_DESCENDANT) {
        parser->next++;
        skip_space(parser);
    }

    return result;
}

// nmstart   [_a-z]|{nonascii}|{escape}
static inline int is_name_start_char(int c) {
    return (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z');
}

// nmchar    [_a-z0-9-]|{nonascii}|{escape}
static inline int is_name_char(int c) {
    return is_name_start_char(c) || c == '-' || (c >= '0' && c <= '9');
}

// ident     [-]?{nmstart}{nmchar}*
static char *xnode_parse_ident(XNodeQueryParser *parser) {
    char *name = parser->data;

    if (*parser->next == '-') {
        *parser->data++ = *parser->next++;
    }

    if (!is_name_start_char(*parser->next)) {
        parser->error = XNODE_BAD_NAME_CHARACTER;
        return NULL;
    }

    *parser->data++ = *parser->next++;

    while (is_name_char(*parser->next)) {
        *parser->data++ = *parser->next++;
    }

    *parser->data++ = '\0';

    return name;
}

// name      {nmchar}+
static char *xnode_parse_name(XNodeQueryParser *parser) {
    char *name = parser->data;

    if (!is_name_char(*parser->next)) {
        parser->error = XNODE_BAD_NAME_CHARACTER;
        return NULL;
    }

    while (is_name_char(*parser->next)) {
        *parser->data++ = *parser->next++;
    }

    *parser->data++ = '\0';

    return name;
}

static char *xnode_parse_value(XNodeQueryParser *parser) {
    char *value = parser->data;
    char  quote = *parser->next;

    // [ IDENT | STRING ]
    if (quote != '\'' && quote != '"') {
        return xnode_parse_ident(parser);
    }

    // Skip '\'' or '"'
    parser->next++;

    while (*parser->next && *parser->next != quote) {
        if (*parser->next == '\\' && *(parser->next + 1) == quote) {
            *parser->data++ = quote;
            parser->next += 2;
        }
        else {
            *parser->data++ = *parser->next++;
        }
    }

    if (*parser->next != quote) {
        parser->error = XNODE_QUOTATION_MARK_EXPECTED;
        return NULL;
    }

    // Skip '\'' or '"'
    parser->next++;

    *parser->data++ = '\0';

    return value;
}

static void xnode_parse_attribute_selector(XNodeQueryParser *parser,
        XNodeSelector *selector) {

    parser->next++; // skip '['

    while (*parser->next != ']' && !parser->error) {
        XNodeAttributeSelector *attr = xnew_zero(XNodeAttributeSelector);

        jsa_push(&selector->attributes, attr);

        if (!(attr->name = xnode_parse_ident(parser))) {
            return;
        }

        skip_space(parser);

        switch (*parser->next) {
        case ']': // [... foo]
            attr->type = XQSEL_ATTR_PRESENCE;
            break;
        case ',': // [foo, bar...]
            attr->type = XQSEL_ATTR_PRESENCE;
            parser->next++;
            skip_space(parser);
            continue;
        case '=':
            attr->type = XQSEL_ATTR_EQUAL;
            break;
        case '~':
            attr->type = XQSEL_ATTR_SPACE_MATCH;
            parser->next++;
            break;
        case '|':
            attr->type = XQSEL_ATTR_DASH_MATCH;
            parser->next++;
            break;
        case '^':
            attr->type = XQSEL_ATTR_PREFIX_MATCH;
            parser->next++;
            break;
        case '$':
            attr->type = XQSEL_ATTR_SUFFIX_MATCH;
            parser->next++;
            break;
        case '*':
            attr->type = XQSEL_ATTR_SUBSTRING_MATCH;
            parser->next++;
            break;
        default:
            parser->error = XNODE_UNEXPECTED_CHAR;
            return;
        }

        if (*parser->next != ']') {
            if (*parser->next == '=') {
                // skip the '=' char
                parser->next++;

                // space before value
                skip_space(parser);

                // Selector needs a value
                if (!(attr->value = xnode_parse_value(parser))) {
                    return;
                }

                // space after value
                skip_space(parser);
            }

            switch (*parser->next) {
            case ']': // [... foo]
                break;
            case ',': // [foo, bar...]
                parser->next++;
                skip_space(parser);
            default:
                continue;
            }
        }
    }

    parser->next++; // skip ']'
}

static void xnode_parse_class_selector(XNodeQueryParser *parser,
        XNodeSelector *selector) {
    // class: '.' IDENT
    const char *class_name = xnode_parse_ident(parser);
    if (class_name) {
        XNodeAttributeSelector *attr = xnew_zero(XNodeAttributeSelector);
        attr->name = "class";
        attr->value = class_name;
        attr->type = XQSEL_ATTR_SPACE_MATCH;
        jsa_push(&selector->attributes, attr);
    }
}

static void xnode_parse_id_selector(XNodeQueryParser *parser,
        XNodeSelector *selector) {
    // "#"{name}
    const char *id = xnode_parse_name(parser);
    if (id) {
        XNodeAttributeSelector *attr = xnew_zero(XNodeAttributeSelector);
        attr->name = "id";
        attr->value = id;
        attr->type = XQSEL_ATTR_EQUAL;
        jsa_push(&selector->attributes, attr);
    }
}

static XNodeSelector *xnode_selector_alloc() {
    XNodeSelector *result = (XNodeSelector *) malloc(sizeof(XNodeSelector));

    result->type = NULL;
    jsa_init(&result->attributes);
    jsa_init(&result->negations);
    result->combinator = XQSEL_DESCENDANT;

    return result;
}

static void xnode_selector_free(XNodeSelector *selector) {
    uint32_t i;

    for (i = 0; i < selector->attributes.size; i++) {
        XNodeAttributeSelector *attr;
        if ((attr = (XNodeAttributeSelector*) selector->attributes.item[i])) {
            xfree(attr);
        }
    }
    jsa_clean(&selector->attributes);
    for (i = 0; i < selector->negations.size; i++) {
        xnode_query_free((XNodeQuery *)jsa_get(&selector->negations, i));
    }
    jsa_clean(&selector->negations);
    xfree(selector);
}

static XNodeSelector *xnode_parse_selector(XNodeQueryParser *parser);
static XNodeQuery *xnode_query_alloc(char *data);

// Parses `selector (combinator selector)*` up to the end of the input or,
// inside :not(), up to the closing ')' or the ',' before the next argument.
static void xnode_parse_chain(XNodeQueryParser *parser, XNodeQuery *query,
        XNodeQueryType combinator) {
    int nested = parser->negation_depth > 0;

    while (!parser->error) {
        XNodeSelector *selector = xnode_parse_selector(parser);
        const char *end;
        if (parser->error) {
            if (selector) xnode_selector_free(selector);
            return;
        }
        if (!selector) {
            parser->error = XNODE_EMPTY_SELECTOR;
            return;
        }
        selector->combinator = combinator;
        jsa_push(&query->selectors, selector);

        end = parser->next;
        if (!skip_space(parser)) {
            if (nested) parser->error = XNODE_UNEXPECTED_CHAR;
            return;
        }
        if (nested && (*parser->next == ')' || *parser->next == ',')) return;
        parser->next = end;
        combinator = xnode_parse_combinator(parser);
    }
}

static void xnode_parse_negation(XNodeQueryParser *parser, XNodeSelector *owner) {
    char *name;

    parser->next++; // ':'
    name = xnode_parse_ident(parser);
    if (!name) return;
    if (strcasecmp(name, "not") != 0) {
        parser->error = XNODE_UNSUPPORTED_PSEUDO_CLASS;
        return;
    }
    if (*parser->next != '(') {
        parser->error = XNODE_UNEXPECTED_CHAR;
        return;
    }
    if (parser->negation_depth >= XNODE_MAX_NEGATION_DEPTH) {
        parser->error = XNODE_NEGATION_TOO_DEEP;
        return;
    }

    parser->next++; // '('
    parser->negation_depth++;
    do {
        // Argument tokens share the outer query's data buffer and lifetime.
        XNodeQuery *query = xnode_query_alloc(NULL);
        jsa_push(&owner->negations, query);
        xnode_parse_chain(parser, query, XQSEL_DESCENDANT);
    } while (!parser->error && *parser->next++ == ',');
    parser->negation_depth--;
}

static XNodeSelector *xnode_parse_selector(XNodeQueryParser *parser) {
    XNodeSelector *selector;

    if (!skip_space(parser)) {
        return NULL;
    }

    selector = xnode_selector_alloc();

    // If a universal selector represented by * (i.e. without a namespace
    // prefix) is not the only component of a sequence of simple selectors or
    // is immediately followed by a pseudo-element, then the * may be omitted
    // and the universal selector's presence implied.
    while (*parser->next && !parser->error) {
        switch (*parser->next) {
        case '*':
            if (!selector->type) {
                selector->type = "*";
                parser->next++;
                continue;
            }
            else {
                parser->error = XNODE_UNEXPECTED_CHAR;
                break;
            }
        case '[':
            if (!selector->type) selector->type = "*";
            xnode_parse_attribute_selector(parser, selector);
            break;
        case '.':
            parser->next++;
            if (!selector->type) selector->type = "*";
            xnode_parse_class_selector(parser, selector);
            break;
        case '#':
            parser->next++;
            if (!selector->type) selector->type = "*";
            xnode_parse_id_selector(parser, selector);
            break;
        case ':':
            if (!selector->type) selector->type = "*";
            xnode_parse_negation(parser, selector);
            break;
        default:
            if (!selector->type) {
                // element_name: IDENT
                selector->type = xnode_parse_ident(parser);
                break;
            }
            return selector;
        }
    }

    return selector;
}

static XNodeQuery* xnode_query_alloc(char *data) {
    XNodeQuery *query = xnew(XNodeQuery);
    query->data = data;
    jsa_init(&query->selectors);
    jsa_init(&query->result[0]);
    jsa_init(&query->result[1]);
    query->prepared_document = NULL;
    query->matched_document = NULL;
    query->matched = NULL;
    return query;
}

void xnode_query_free(XNodeQuery *query) {
    uint32_t i;
    for (i = 0; i < jsa_size(&query->selectors); i++) {
        xnode_selector_free((XNodeSelector *)jsa_get(&query->selectors, i));
    }
    jsa_clean(&query->selectors);
    jsa_clean(&query->result[0]);
    jsa_clean(&query->result[1]);
    xfree(query->data);
    xfree(query);
}

XNodeQuery *xnode_query_create(const char *query_text, const char **errptr,
        int *erroffset) {
    XNodeQueryParser  parser;
    XNodeQuery       *query;
    XNodeQueryType    combinator;

    parser.next  = query_text;
    parser.data  = (char*) xmalloc(strlen(query_text) + 1);
    parser.error = NULL;
    parser.negation_depth = 0;

    // To support queries like node:find('>span.hidden')
    if ((combinator = xnode_parse_combinator(&parser)) == XQSEL_UNKNOWN_TYPE) {
        combinator = XQSEL_DESCENDANT;
        parser.error = NULL;
    }

    query = xnode_query_alloc(parser.data);
    xnode_parse_chain(&parser, query, combinator);

    if (!parser.error && query->selectors.size == 0) {
        parser.error = XNODE_EMPTY_SELECTOR;
    }

    if (parser.error) {
        xnode_query_free(query);
        if (errptr) *errptr = parser.error;
        if (erroffset) *erroffset = (int)(parser.next - query_text);
        return NULL;
    }

    return query;
}
