#include "../resect.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "uthash.h"
#include "resect_private.h"

/*
 * SET
 */
typedef struct resect_set_item {
    void *key;

    UT_hash_handle hh;
} *resect_set_item;

struct resect_set {
    resect_set_item head;
};

resect_set resect_set_create() {
    resect_set set = malloc(sizeof(struct resect_set));
    set->head = NULL;
    return set;
}

resect_bool resect_set_contains(resect_set set, void *value) {
    resect_set_item entry;
    HASH_FIND_PTR(set->head, &value, entry);
    return entry != NULL ? resect_true : resect_false;
}

resect_bool resect_set_add(resect_set set, void *value) {
    if (!resect_set_contains(set, value)) {
        resect_set_item entry = malloc(sizeof(struct resect_set_item));
        entry->key = value;
        HASH_ADD_PTR(set->head, key, entry);
        return resect_true;
    }
    return resect_false;
}

void resect_set_free(resect_set set) {
    struct resect_set_item *element, *tmp;
    HASH_ITER(hh, set->head, element, tmp) {
        HASH_DEL(set->head, element);
        free(element);
    }
    free(set);
}

/*
 * STRING
 */
struct resect_string {
    char *value;
    unsigned int capacity;
};

resect_string resect_string_create(unsigned int initial_capacity) {
    resect_string result = malloc(sizeof(struct resect_string));
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
        free(string->value);

        string->capacity = new_capacity;

        char *value = malloc(sizeof(char) * string->capacity);
        assert(value);
        string->value = value;
    }
    string->value[string->capacity - 1] = 0;
}

resect_string resect_update_string(resect_string string, const char *new_value) {
    assert(string != NULL);
    const char *ensured_new_value = new_value == NULL ? "" : new_value;
    size_t new_string_size = strlen(ensured_new_value);

    resect_ensure_string_capacity(string, new_string_size + 1);

    memcpy(string->value, ensured_new_value, sizeof(char) * new_string_size);
    string->value[new_string_size] = 0;
    return string;
}

char *_resect_prepare_c_string(resect_string string, int length) {
    assert(string != NULL);
    resect_ensure_string_capacity(string, length + 1);

    string->value[length] = 0;
    return string->value;
}

resect_string resect_ensure_string(resect_string string) {
    return string == NULL ? resect_string_create(0) : string;
}


resect_string resect_ensure_string_default_length(resect_string string, int length) {
    return string == NULL ? resect_string_create(length + 1) : string;
}

resect_string resect_ensure_string_default_value(resect_string string, const char *default_value) {
    if (string == NULL) {
        resect_string result;
        result = resect_string_create(0);
        resect_update_string(result, default_value);
        return result;
    } else {
        return string;
    }
}

resect_string resect_string_from_c(const char *string) {
    return resect_ensure_string_default_value(NULL, string);
}

resect_string resect_ensure_string_from_clang(resect_string provided, CXString from) {
    resect_string result = resect_ensure_string(provided);
    resect_update_string(result, clang_getCString(from));
    clang_disposeString(from);
    return result;
}


resect_string resect_string_from_clang(CXString from) {
    return resect_ensure_string_from_clang(NULL, from);
}

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
    vsnprintf(result->value, len, format, args);
    va_end(args);

    return result;
}

/*
 * COLLECTION
 */
struct resect_collection_element {
    void *value;
    struct resect_collection_element *next;
};

struct resect_collection {
    unsigned int size;
    struct resect_collection_element *head, *tail;
};

resect_collection resect_collection_create() {
    resect_collection collection = malloc(sizeof(struct resect_collection));
    collection->head = NULL;
    collection->tail = NULL;
    collection->size = 0;
    return collection;
}

void resect_collection_free(resect_collection collection) {
    struct resect_collection_element *el, *next;
    el = collection->head;
    while (el) {
        next = el->next;
        free(el);
        el = next;
    }
    free(collection);
}

void resect_collection_add(resect_collection collection, void *value) {
    struct resect_collection_element *element = malloc(sizeof(struct resect_collection_element));
    element->value = value;
    element->next = NULL;

    if (collection->head == NULL) {
        collection->head = element;
        collection->tail = element;
    } else {
        collection->tail->next = element;
        collection->tail = element;
    }
    collection->size += 1;
}

unsigned int resect_collection_size(resect_collection collection) {
    return collection->size;
}

/*
 * ITERATOR
 */
struct resect_iterator {
    struct resect_collection_element *head;
    struct resect_collection_element *current;
};

resect_iterator resect_collection_iterator(resect_collection collection) {
    resect_iterator iterator = malloc(sizeof(struct resect_iterator));
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

void resect_iterator_free(resect_iterator iter) {
    free(iter);
}

/*
 * UTIL
 */

long long filter_valid_value(long long value) {
    return value < 0 ? 0 : value;
}