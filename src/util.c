#include "../resect.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "uthash.h"
#include "resect_private.h"

/*
 * STRING
 */
struct resect_string {
    char *value;
    size_t capacity;
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

resect_string resect_string_update(resect_string string, const char *new_value) {
    assert(string != NULL);
    const char *ensured_new_value = new_value == NULL ? "" : new_value;
    size_t new_string_size = strlen(ensured_new_value);

    resect_ensure_string_capacity(string, new_string_size + 1);

    memcpy(string->value, ensured_new_value, sizeof(char) * new_string_size);
    string->value[new_string_size] = 0;
    return string;
}

resect_string resect_string_append(resect_string string, const char *postfix) {
    assert(string != NULL);
    size_t add_len = strlen(postfix);
    size_t old_len = strlen(string->value);
    size_t required_capacity = old_len + add_len + 1;
    resect_ensure_string_capacity(string, required_capacity);

    memcpy(string->value + old_len, postfix, sizeof(char) * add_len);

    string->value[required_capacity - 1] = 0;
}

resect_string resect_string_copy(resect_string string) {
    assert(string != NULL);
    return resect_string_from_c(resect_string_to_c(string));
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
        resect_string_update(result, default_value);
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
    resect_string_update(result, clang_getCString(from));
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
    struct resect_collection_element *prev;
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
        struct resect_collection_element *new_tail = collection->tail->prev;
        free(collection->tail);

        new_tail->next = NULL;
        collection->tail = new_tail;
    }
    collection->size -= 1;
    return result;
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

void resect_set_add_to_collection(resect_set set, resect_collection collection) {
    struct resect_set_item *element, *tmp;
    HASH_ITER(hh, set->head, element, tmp) {
        resect_collection_add(collection, element->key);
    }
}

/*
 * MEMORY FILE
 */
struct resect_memory_file {
    char *buf;
    size_t len;
    FILE *handle;
};

resect_memory_file resect_memory_file_create(const char **filenames, int count) {
    resect_memory_file file = malloc(sizeof(struct resect_memory_file));
    file->handle = open_memstream(&file->buf, &file->len);
    return file;
}

void resect_memory_file_flush(resect_memory_file file) {
    fflush(file->handle);
    fseek(file->handle, 0, SEEK_SET);
}


FILE *resect_memory_file_handle(resect_memory_file file) {
    return file->handle;
}


void resect_memory_file_free(resect_memory_file file) {
    fclose(file->handle);
    free(file->buf);
}

/*
 * STRING HASH TABLE
 */
struct resect_table_entry {
    char *key;
    void *value;

    UT_hash_handle hh;
};

struct resect_table {
    struct resect_table_entry *head;
};

resect_table resect_table_create() {
    resect_table table = malloc(sizeof(struct resect_table));
    table->head = NULL;
    return table;
}

resect_bool resect_table_put_if_absent(resect_table table, const char *key, void *value) {
    struct resect_table_entry *entry = NULL;
    HASH_FIND_STR(table->head, key, entry);
    if (entry != NULL) {
        return resect_false;
    }

    entry = malloc(sizeof(struct resect_table_entry));
    entry->value = value;

    size_t key_len = strlen(key);
    entry->key = malloc(sizeof(char) * (key_len + 1));
    strcpy(entry->key, key);

    HASH_ADD_STR(table->head, key, entry);
    return resect_true;
}

void *resect_table_get(resect_table table, const char *key) {
    struct resect_table_entry *entry = NULL;
    HASH_FIND_STR(table->head, key, entry);

    return entry != NULL ? entry->value : NULL;
}

void resect_visit_table(resect_table table,
                        resect_bool (*entry_visitor)(void *, const char *, void *),
                        void *context) {
    assert(entry_visitor != NULL);
    struct resect_table_entry *entry, *tmp;
    HASH_ITER(hh, table->head, entry, tmp) {
        if (!entry_visitor(context, entry->key, entry->value)) {
            break;
        }
    }
}

void resect_table_free(resect_table table, void (*value_destructor)(void *, void *), void *context) {
    struct resect_table_entry *entry, *tmp;
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
 * UTIL
 */
long long filter_valid_value(long long value) {
    return value < 0 ? 0 : value;
}

void resect_string_collection_free(resect_collection collection) {
    resect_iterator arg_iter = resect_collection_iterator(collection);
    while (resect_iterator_next(arg_iter)) {
        resect_string_free(resect_iterator_value(arg_iter));
    }
    resect_iterator_free(arg_iter);
    resect_collection_free(collection);
}