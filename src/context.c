#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <string.h>
#include "uthash.h"

/*
 * TRANSLATION CONTEXT
 */
struct resect_decl_table_entry {
    const char *key;
    resect_decl value;

    UT_hash_handle hh;
};

typedef struct resect_translation_context {
    resect_set exposed_decls;
    resect_collection namespace_queue;
    resect_string current_namespace;
    struct resect_decl_table_entry *decl_table;
} *resect_translation_context;

resect_translation_context resect_context_create() {
    resect_translation_context context = malloc(sizeof(struct resect_translation_context));
    context->exposed_decls = resect_set_create();
    context->decl_table = NULL;
    context->namespace_queue = resect_collection_create();
    context->current_namespace = resect_string_from_c("");

    return context;
}

void resect_context_free(resect_translation_context context, resect_set deallocated) {
    if (!resect_set_add(deallocated, context)) {
        return;
    }

    struct resect_decl_table_entry *element, *tmp;
    HASH_ITER(hh, context->decl_table, element, tmp) {
        HASH_DEL(context->decl_table, element);
        resect_decl_free(element->value, deallocated);
        free((void *) element->key);
        free(element);
    }

    resect_set_free(context->exposed_decls);
    resect_string_free(context->current_namespace);
    resect_string_collection_free(context->namespace_queue);
    free(context);
}

void resect_expose_decl(resect_translation_context context, resect_decl decl) {
    resect_set_add(context->exposed_decls, decl);
}

void resect_register_decl(resect_translation_context context, resect_string decl_id, resect_decl decl) {
    const char *id = resect_string_to_c(decl_id);

    struct resect_decl_table_entry *entry = NULL;

    HASH_FIND_STR(context->decl_table, id, entry);
    if (entry != NULL) {
        return;
    }

    entry = malloc(sizeof(struct resect_decl_table_entry));

    size_t len = strlen(id);
    char *key = malloc(sizeof(char) * (len + 1));
    strncpy(key, id, len);
    key[len] = 0;

    entry->key = key;
    entry->value = decl;
    HASH_ADD_STR(context->decl_table, key, entry);
}

resect_decl resect_find_decl(resect_translation_context context, resect_string decl_id) {
    struct resect_decl_table_entry *entry = NULL;
    HASH_FIND_STR(context->decl_table, resect_string_to_c(decl_id), entry);
    if (entry == NULL) {
        return NULL;
    }
    return entry->value;
}

resect_collection resect_create_decl_collection(resect_translation_context context) {
    resect_collection collection = resect_collection_create();
    resect_set_add_to_collection(context->exposed_decls, collection);
    return collection;
}

void resect_update_current_namespace(resect_translation_context context) {
    resect_string_update(context->current_namespace, "");

    int i = 0;
    resect_iterator iter = resect_collection_iterator(context->namespace_queue);
    while (resect_iterator_next(iter)) {
        resect_string namespace = resect_iterator_value(iter);
        if (i > 0) {
            resect_string_append(context->current_namespace, "::");
        }
        resect_string_append(context->current_namespace, resect_string_to_c(namespace));
        ++i;
    }
    resect_iterator_free(iter);
}

void resect_push_namespace(resect_translation_context context, resect_string namespace) {
    resect_collection_add(context->namespace_queue, resect_string_copy(namespace));
    resect_update_current_namespace(context);
}

void resect_pop_namespace(resect_translation_context context) {
    resect_string_free(resect_collection_pop_last(context->namespace_queue));

    resect_update_current_namespace(context);
}

resect_string resect_namespace(resect_translation_context context) {
    return context->current_namespace;
}