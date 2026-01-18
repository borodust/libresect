#include "../resect.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "resect_private.h"
#include "uthash.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

/*
 * STRING
 */
struct P_resect_string {
    char *value;
    size_t capacity;
};

resect_string resect_string_create(unsigned int initial_capacity) {
    resect_string result = malloc(sizeof(struct P_resect_string));
    result->capacity = initial_capacity > 0 ? initial_capacity : 1;
    result->value = malloc(result->capacity * sizeof(char));
    result->value[0] = 0;
    return result;
}

void resect_string_free(resect_string string) {
    free(string->value);
    free(string);
}

const char *resect_string_to_c(resect_string string) {
    assert(string != NULL);
    return string->value;
}

void resect_ensure_string_capacity(resect_string string, unsigned long new_capacity) {
    if (string->capacity < new_capacity) {
        char *old_string = string->value;
        size_t old_capacity = string->capacity;


        char *new_value = malloc(sizeof(char) * new_capacity);
        assert(new_value);
        memcpy(new_value, old_string, sizeof(char) * old_capacity);
        free(old_string);

        string->capacity = new_capacity;
        string->value = new_value;
    }
    string->value[string->capacity - 1] = 0;
}

resect_string resect_string_update_by_length(resect_string string, const char *new_value, long long length) {
    assert(string != NULL);
    const char *ensured_new_value = new_value == NULL ? "" : new_value;

    size_t new_string_size = strlen(ensured_new_value);
    if (length >= 0 && new_string_size >= length) {
        new_string_size = length;
    }

    resect_ensure_string_capacity(string, new_string_size + 1);

    memcpy(string->value, ensured_new_value, sizeof(char) * new_string_size);
    string->value[new_string_size] = 0;
    return string;
}

resect_string resect_string_update_c(resect_string string, const char *new_value) {
    return resect_string_update_by_length(string, new_value, -1);
}

resect_string resect_string_append(resect_string string, resect_string postfix) {
    return resect_string_append_c(string, resect_string_to_c(postfix));
}

resect_string resect_string_append_c(resect_string string, const char *postfix) {
    assert(string != NULL);
    size_t add_len = strlen(postfix);
    if (add_len == 0) {
        return string;
    }

    size_t old_len = strlen(string->value);
    size_t required_capacity = old_len + add_len + 1;
    resect_ensure_string_capacity(string, required_capacity);

    memcpy(string->value + old_len, postfix, sizeof(char) * add_len);

    string->value[required_capacity - 1] = 0;

    return string;
}

size_t resect_string_length(resect_string string) { return strlen(string->value); }

resect_string resect_string_copy(resect_string string) {
    assert(string != NULL);
    return resect_string_from_c(resect_string_to_c(string));
}

resect_string resect_substring(resect_string string, long long start, long long end) {
    assert(string != NULL);
    assert(start > 0);
    assert(end < 0 || start <= end);
    const char *c_str = resect_string_to_c(string);
    long long len = end < 0 ? -1 : end - start;
    return resect_string_update_by_length(resect_string_create(len), c_str + start, len);
}

resect_string resect_ensure_string(resect_string string) { return string == NULL ? resect_string_create(0) : string; }

resect_string resect_ensure_string_default_length(resect_string string, int length) {
    return string == NULL ? resect_string_create(length + 1) : string;
}

resect_string resect_ensure_string_default_value(resect_string string, const char *default_value) {
    if (string == NULL) {
        resect_string result;
        result = resect_string_create(0);
        resect_string_update_c(result, default_value);
        return result;
    } else {
        return string;
    }
}

resect_string resect_string_from_c(const char *string) { return resect_ensure_string_default_value(NULL, string); }

resect_string resect_ensure_string_from_clang(resect_string provided, CXString from) {
    resect_string result = resect_ensure_string(provided);
    resect_string_update_c(result, clang_getCString(from));
    clang_disposeString(from);
    return result;
}

resect_string resect_string_from_clang(CXString from) { return resect_ensure_string_from_clang(NULL, from); }

resect_string resect_string_format(const char *format, ...) {
    if (format == NULL) {
        return resect_ensure_string(NULL);
    }

    va_list args;
    int len = 0;
    va_start(args, format);
    len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    resect_string result = resect_ensure_string_default_length(NULL, len);
    va_start(args, format);
    vsnprintf(result->value, len + 1, format, args);
    va_end(args);

    return result;
}

resect_bool resect_string_equal_c(resect_string this, const char *that) { return strcmp(this->value, that) == 0; }

resect_bool resect_string_starts_with_c(resect_string this, const char *prefix) {
    return strncmp(prefix, this->value, strlen(prefix)) == 0;
}

resect_bool resect_string_equal(resect_string this, resect_string that) {
    return strcmp(this->value, that->value) == 0;
}

/*
 * COLLECTION
 */
struct P_resect_collection_element {
    void *value;
    struct P_resect_collection_element *next;
    struct P_resect_collection_element *prev;
};

struct P_resect_collection {
    unsigned int size;
    struct P_resect_collection_element *head, *tail;
};

resect_collection resect_collection_create() {
    resect_collection collection = malloc(sizeof(struct P_resect_collection));
    collection->head = NULL;
    collection->tail = NULL;
    collection->size = 0;
    return collection;
}

void resect_collection_free(resect_collection collection) {
    struct P_resect_collection_element *el, *next;
    el = collection->head;
    while (el) {
        next = el->next;
        free(el);
        el = next;
    }
    free(collection);
}

void resect_collection_add(resect_collection collection, void *value) {
    struct P_resect_collection_element *element = malloc(sizeof(struct P_resect_collection_element));
    element->value = value;
    element->next = NULL;
    element->prev = NULL;

    if (collection->head == NULL) {
        collection->head = element;
        collection->tail = element;
    } else {
        element->prev = collection->tail;

        collection->tail->next = element;
        collection->tail = element;
    }
    collection->size += 1;
}

void *resect_collection_pop_last(resect_collection collection) {
    if (collection->tail == NULL) {
        return NULL;
    }
    void *result = collection->tail->value;
    if (collection->tail->prev == NULL) {
        free(collection->tail);

        collection->head = NULL;
        collection->tail = NULL;
    } else {
        struct P_resect_collection_element *new_tail = collection->tail->prev;
        free(collection->tail);

        new_tail->next = NULL;
        collection->tail = new_tail;
    }
    collection->size -= 1;
    return result;
}

void *resect_collection_peek_last(resect_collection collection) {
    if (collection->tail == NULL) {
        return NULL;
    }
    return collection->tail->value;
}

unsigned int resect_collection_size(resect_collection collection) { return collection->size; }

/*
 * ITERATOR
 */
struct P_resect_iterator {
    struct P_resect_collection_element *head;
    struct P_resect_collection_element *current;
};

resect_iterator resect_collection_iterator(resect_collection collection) {
    resect_iterator iterator = malloc(sizeof(struct P_resect_iterator));
    iterator->head = collection->head;
    iterator->current = NULL;
    return iterator;
}

resect_bool resect_iterator_next(resect_iterator iter) {
    if (iter->head == NULL || (iter->current != NULL && iter->current->next == NULL)) {
        return resect_false;
    }

    if (iter->current == NULL) {
        iter->current = iter->head;
    } else {
        iter->current = iter->current->next;
    }

    return resect_true;
}

void *resect_iterator_value(resect_iterator iter) {
    assert(iter->current != NULL);

    return iter->current->value;
}

void resect_iterator_free(resect_iterator iter) { free(iter); }

/*
 * SET
 */
typedef struct P_resect_set_item {
    void *key;

    UT_hash_handle hh;
} *resect_set_item;

struct P_resect_set {
    resect_set_item head;
};

resect_set resect_set_create() {
    resect_set set = malloc(sizeof(struct P_resect_set));
    set->head = NULL;
    return set;
}

unsigned int resect_set_size(resect_set set) { return HASH_COUNT(set->head); }

resect_bool resect_set_contains(resect_set set, void *value) {
    resect_set_item entry;
    HASH_FIND_PTR(set->head, &value, entry);
    return entry != NULL ? resect_true : resect_false;
}

resect_bool resect_set_add(resect_set set, void *value) {
    if (!resect_set_contains(set, value)) {
        resect_set_item entry = malloc(sizeof(struct P_resect_set_item));
        entry->key = value;
        HASH_ADD_PTR(set->head, key, entry);
        return resect_true;
    }
    return resect_false;
}

void resect_set_free(resect_set set) {
    struct P_resect_set_item *element, *tmp;
    HASH_ITER(hh, set->head, element, tmp) {
        HASH_DEL(set->head, element);
        free(element);
    }
    free(set);
}

void resect_set_add_to_collection(resect_set set, resect_collection collection) {
    struct P_resect_set_item *element, *tmp;
    HASH_ITER(hh, set->head, element, tmp) { resect_collection_add(collection, element->key); }
}

/**
 *
 * @param item_visitor return false to unterrupt visit process
 * @param context visit data passed to item_visitor
 */
void resect_visit_set(resect_set set, resect_bool (*item_visitor)(void *ctx, void *item), void *context) {
    assert(item_visitor != NULL);
    struct P_resect_set_item *entry, *tmp;
    HASH_ITER(hh, set->head, entry, tmp) {
        if (!item_visitor(context, entry->key)) {
            break;
        }
    }
}

/*
 * STRING HASH TABLE
 */
struct P_resect_table_entry {
    char *key;
    void *value;

    UT_hash_handle hh;
};

struct P_resect_table {
    struct P_resect_table_entry *head;
};

resect_table resect_table_create() {
    resect_table table = malloc(sizeof(struct P_resect_table));
    table->head = NULL;
    return table;
}

resect_bool resect_table_put_if_absent(resect_table table, const char *key, void *value) {
    struct P_resect_table_entry *entry = NULL;
    HASH_FIND_STR(table->head, key, entry);
    if (entry != NULL) {
        return resect_false;
    }

    entry = malloc(sizeof(struct P_resect_table_entry));
    entry->value = value;

    size_t key_len = strlen(key);
    entry->key = malloc(sizeof(char) * (key_len + 1));
    strcpy(entry->key, key);

    HASH_ADD_STR(table->head, key, entry);
    return resect_true;
}

void *resect_table_put(resect_table table, const char *key, void *value) {
    struct P_resect_table_entry *new_entry = NULL;
    new_entry = malloc(sizeof(struct P_resect_table_entry));
    new_entry->value = value;

    size_t key_len = strlen(key);
    new_entry->key = malloc(sizeof(char) * (key_len + 1));
    strcpy(new_entry->key, key);

    struct P_resect_table_entry *prev_entry = NULL;
    HASH_REPLACE_STR(table->head, key, new_entry, prev_entry);

    void *prev_value = NULL;
    if (prev_entry != NULL) {
        prev_value = prev_entry->value;
        free(prev_entry->key);
        free(prev_entry);
    }

    return prev_value;
}

bool resect_table_remove(resect_table table, const char *key) {
    struct P_resect_table_entry *entry = NULL;
    HASH_FIND_STR(table->head, key, entry);
    if (entry == NULL) {
        return false;
    }

    HASH_DEL(table->head, entry);
    free(entry->key);
    free(entry);
    return true;
}

void *resect_table_get(resect_table table, const char *key) {
    struct P_resect_table_entry *entry = NULL;
    HASH_FIND_STR(table->head, key, entry);

    return entry != NULL ? entry->value : NULL;
}

/**
 *
 * @param entry_visitor return false to unterrupt visit process
 * @param context visit data passed to entry_visitor
 */
void resect_visit_table(resect_table table, resect_bool (*entry_visitor)(void *, const char *, void *), void *context) {
    assert(entry_visitor != NULL);
    struct P_resect_table_entry *entry, *tmp;
    HASH_ITER(hh, table->head, entry, tmp) {
        if (!entry_visitor(context, entry->key, entry->value)) {
            break;
        }
    }
}

static resect_bool printf_table_entry(void *ctx, const char *key, void *value) {
    fprintf(stderr, "  \"%s\": %p\n", key, value);
}

void resect_print_table(resect_table table) {
    fprintf(stderr, "Table: %p\n", table);
    resect_visit_table(table, printf_table_entry, NULL);
}

unsigned int resect_table_size(resect_table table) { return HASH_COUNT(table->head); }

void resect_table_free(resect_table table, void (*value_destructor)(void *, void *), void *context) {
    struct P_resect_table_entry *entry, *tmp;
    HASH_ITER(hh, table->head, entry, tmp) {
        HASH_DEL(table->head, entry);
        if (value_destructor != NULL) {
            value_destructor(context, entry->value);
        }
        free(entry->key);
        free(entry);
    }
    free(table);
}

/*
 * PATTERN
 */
typedef struct P_resect_pattern {
    pcre2_code *compiled;
} *resect_pattern;

void print_pcre_error(int errornumber, size_t erroroffset) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
    fprintf(stderr, "PCRE2 compilation failed at offset %d: %s\n", (int) erroroffset, buffer);
}

resect_pattern resect_pattern_create(resect_string pattern) {
    return resect_pattern_create_c(resect_string_to_c(pattern));
}

resect_pattern resect_pattern_create_c(const char *pattern) {
    resect_pattern result = malloc(sizeof(struct P_resect_pattern));

    int errornumber;
    PCRE2_SIZE erroroffset;
    pcre2_code *compiled = pcre2_compile((PCRE2_SPTR) (pattern), PCRE2_ZERO_TERMINATED, PCRE2_UTF,
                                         &errornumber, &erroroffset, NULL);
    if (compiled == NULL) {
        print_pcre_error(errornumber, erroroffset);
        // FIXME: add better error reporting
        assert(!"Failed to parse inclusion/exclusion pattern");
    }
    result->compiled = compiled;
    return result;
}

bool resect_pattern_match(resect_pattern pattern, resect_string subject) {
    return resect_pattern_match_c(pattern, resect_string_to_c(subject));
}

bool resect_pattern_match_c(resect_pattern pattern, const char *subject) {
    pcre2_code *compiled = pattern->compiled;
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(compiled, NULL);

    int rc = pcre2_match(compiled, (PCRE2_SPTR) subject, PCRE2_ZERO_TERMINATED, 0, 0, match_data, NULL);
    pcre2_match_data_free(match_data);

    if (rc >= 0) {
        return true;
    }

    return false;
}

bool resect_pattern_find(resect_pattern pattern, resect_string subject, resect_string out) {
    return resect_pattern_find_c(pattern, resect_string_to_c(subject), out);
}

bool resect_pattern_find_c(resect_pattern pattern, const char *subject, resect_string substring_result) {
    bool result = false;
    pcre2_code *compiled = pattern->compiled;
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(compiled, NULL);


    int rc = pcre2_match(compiled, (PCRE2_SPTR) subject, PCRE2_ZERO_TERMINATED, 0, 0, match_data, NULL);

    if (rc < 0) {
        if (rc != PCRE2_ERROR_NOMATCH) {
            fprintf(stderr, "PCRE2 error when finding substring in %s\n", subject);
        }
        goto done;
    }

    PCRE2_UCHAR *substring;
    PCRE2_SIZE substring_length = -1;
    rc = pcre2_substring_get_bynumber(match_data, 0, &substring, &substring_length);
    if (rc != 0) {
        fprintf(stderr, "PCRE2 failed to get substring from %s with error %d\n", subject, rc);
        goto done;
    }

    result = true;
    resect_string_update_by_length(substring_result, substring, substring_length);
    pcre2_substring_free(substring);

done:
    pcre2_match_data_free(match_data);
    return result;
}

void resect_pattern_free(resect_pattern pattern) {
    pcre2_code_free(pattern->compiled);
    free(pattern);
}

/*
 * UTIL
 */
long long filter_valid_value(long long value) { return value < 0 ? 0 : value; }

void resect_string_collection_free(resect_collection collection) {
    resect_iterator arg_iter = resect_collection_iterator(collection);
    while (resect_iterator_next(arg_iter)) {
        resect_string_free(resect_iterator_value(arg_iter));
    }
    resect_iterator_free(arg_iter);
    resect_collection_free(collection);
}

resect_string extract_type_fqn(CXCursor cursor, CXType type) {
    CXPrintingPolicy pp = clang_getCursorPrintingPolicy(cursor);
    clang_PrintingPolicy_setProperty(pp,
                                     CXPrintingPolicy_FullyQualifiedName,
                                     resect_true);
    resect_string fqn = resect_string_from_clang(clang_getFullyQualifiedName(type, pp, 0));
    clang_PrintingPolicy_dispose(pp);
    return fqn;
}


static void append_function_proto(resect_string name, CXCursor cursor);
static void append_cursor_parent(resect_string name, CXCursor cursor, CXCursor parent);

static void append_cursor_spelling(resect_string name, CXCursor cursor) {
    resect_string spelling = resect_string_from_clang(clang_getCursorSpelling(cursor));
    resect_string_append(name, spelling);
    resect_string_free(spelling);
}

static resect_string extract_full_name(CXCursor cursor) {
    resect_string full_name = resect_string_from_c("");
    append_cursor_parent(full_name, cursor, clang_getCursorSemanticParent(cursor));

    switch (convert_cursor_kind(cursor)) {
        case RESECT_DECL_KIND_FUNCTION:
        case RESECT_DECL_KIND_METHOD:
            append_function_proto(full_name, cursor);
            break;
        case RESECT_DECL_KIND_FIELD: {
            CXType type = clang_getCursorType(cursor);
            CXCursor type_decl_cursor = clang_getTypeDeclaration(type);
            if (!is_cursor_anonymous(type_decl_cursor)) {
                append_cursor_spelling(full_name, cursor);
            }
        }
        break;
        default: {
            if (!is_cursor_anonymous(cursor)) {
                append_cursor_spelling(full_name, cursor);
            }
        }
    }

    return full_name;
}

static void append_function_proto(resect_string name, CXCursor cursor) {
    CXCursor parent = clang_getCursorSemanticParent(cursor);

    resect_string proto;
    switch (clang_getCursorKind(cursor)) {
        case CXCursor_Constructor:
        case CXCursor_Destructor:
            if (is_cursor_anonymous(parent)) {
                proto = resect_string_from_c(clang_getCursorKind(cursor) == CXCursor_Destructor ? "~" : "");
            } else {
                proto = resect_string_from_clang(clang_getCursorSpelling(cursor));
            }
            break;

        case CXCursor_ConversionFunction:
        case CXCursor_CXXMethod:
        case CXCursor_FunctionDecl:
        case CXCursor_FunctionTemplate:
                proto = resect_string_from_clang(clang_getCursorSpelling(cursor));
            break;

        default:
            assert(!"Unhandled function kind");
    }

    resect_string_append_c(proto, "(");

    CXType type = clang_getCursorType(cursor);
    int arg_count = clang_getNumArgTypes(type);
    for (int i = 0; i < arg_count; ++i) {
        CXType arg_type = clang_getArgType(type, i);
        resect_string arg_type_name = extract_type_fqn(cursor, arg_type);
        resect_string_append(proto, arg_type_name);
        if (i < arg_count - 1) {
            resect_string_append_c(proto, ", ");
        }
        resect_string_free(arg_type_name);
    }
    resect_string_append_c(proto, ")");

    resect_string_append(name, proto);
    resect_string_free(proto);
}

void append_cursor_namespace(resect_string name, CXCursor cursor) {
    resect_string namespace = resect_format_cursor_namespace(cursor);
    if (resect_string_length(namespace) > 0) {
        resect_string_append_c(name, resect_string_to_c(namespace));
        resect_string_append_c(name, "::");
    }
    resect_string_free(namespace);
}

void append_cursor_parent(resect_string name, CXCursor cursor, CXCursor parent) {
    resect_string parent_full_name = resect_format_cursor_full_name(parent);
    if (!resect_string_equal_c(parent_full_name, "")) {
        resect_string_append(name, parent_full_name);
        resect_string_append_c(name, "::");
    } else {
        append_cursor_namespace(name, cursor);
    }
    resect_string_free(parent_full_name);
}

resect_string resect_format_cursor_full_name(CXCursor cursor) {
    if (clang_Cursor_isNull(cursor) || clang_getCursorKind(cursor) == CXCursor_TranslationUnit) {
        return resect_string_from_c("");
    }

    if (is_cursor_anonymous(cursor)) {
        return resect_format_cursor_full_name(clang_getCursorSemanticParent(cursor));
    }

    switch (convert_cursor_kind(cursor)) {
        case RESECT_DECL_KIND_UNDECLARED:
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_UNION:
        case RESECT_DECL_KIND_CLASS:
        case RESECT_DECL_KIND_ENUM:
        case RESECT_DECL_KIND_TYPEDEF:
            return extract_type_fqn(cursor, clang_getCursorType(cursor));

        case RESECT_DECL_KIND_FUNCTION:
        case RESECT_DECL_KIND_METHOD:
        case RESECT_DECL_KIND_FIELD:
        case RESECT_DECL_KIND_PARAMETER:
        case RESECT_DECL_KIND_ENUM_CONSTANT:
        case RESECT_DECL_KIND_VARIABLE:
        case RESECT_DECL_KIND_TEMPLATE_PARAMETER:
            return extract_full_name(cursor);

        case RESECT_DECL_KIND_MACRO:
        case RESECT_DECL_KIND_UNKNOWN: {
            return resect_string_from_clang(clang_getCursorDisplayName(cursor));
        }
        default:
            assert(!"All decl kinds shall be covered for full cursor name reconstruction");
    }
}

static void append_anonymous_decl_id(resect_string id, const char *infix, CXCursor cursor) {
    // nameless param with no USR?
    CXCursor parent = clang_getCursorSemanticParent(cursor);
    if (!clang_Cursor_isNull(parent)) {
        resect_string parent_id = resect_extract_decl_id(parent);

        resect_location loc = resect_location_from_cursor(cursor);
        resect_string postfix =
                resect_string_format(":%s:%d:%d", infix, resect_location_line(loc), resect_location_column(loc));

        resect_string_append_c(id, resect_string_to_c(parent_id));
        resect_string_append_c(id, resect_string_to_c(postfix));

        resect_string_free(postfix);
        resect_string_free(parent_id);
        resect_location_free(loc);
    }
}

static void append_cursor_full_name(resect_string id, CXCursor cursor) {
    resect_string full_name = resect_format_cursor_full_name(cursor);
    if (resect_string_length(full_name) == 0) {
        goto done;
    }

    resect_string cursor_kind = resect_string_from_clang(clang_getCursorKindSpelling(clang_getCursorKind(cursor)));
    resect_string decl_id =
            resect_string_format("claw_did$%s$%s", resect_string_to_c(cursor_kind), resect_string_to_c(full_name));
    resect_string_append_c(id, resect_string_to_c(decl_id));

    resect_string_free(decl_id);
    resect_string_free(cursor_kind);

done:
    resect_string_free(full_name);
}


resect_string resect_extract_decl_id(CXCursor cursor) {
    resect_string id = resect_string_from_clang(clang_getCursorUSR(cursor));

    if (resect_string_length(id) > 0) {
        return id;
    }

    switch (clang_getCursorKind(cursor)) {
        case CXCursor_ParmDecl: // nameless param with no USR?
            append_anonymous_decl_id(id, "parm", cursor);
            return id;
        case CXCursor_FieldDecl: // anonymous struct/union?
            append_anonymous_decl_id(id, "field", cursor);
            return id;
        default: // as a last resort, lets try extracting cursor's full type name
        {
            append_cursor_full_name(id, cursor);
            if (resect_string_length(id) > 0) {
                return id;
            }
        }
    }

    // FIXME: add proper error handling
    assert(!"Declaration identifier must not be empty");
    return id;
}

/**
 * http://www.cse.yorku.ca/~oz/hash.html
 */
unsigned long resect_hash(const char *str) {
    unsigned long hash = 0;
    int c;

    while ((c = (unsigned char) *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

resect_bool convert_bool_from_uint(unsigned int val) { return val ? resect_true : resect_false; }
