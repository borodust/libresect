#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <string.h>

#include <clang-c/Index.h>

#include "uthash.h"

/*
 * TRANSLATION CONTEXT
 */
struct resect_decl_table {
    const char *key;
    resect_decl value;

    UT_hash_handle hh;
};

struct resect_translation_context {
    resect_collection declarations;
    struct resect_decl_table *decl_table;
};

resect_translation_context resect_context_create() {
    resect_translation_context context = malloc(sizeof(struct resect_translation_context));
    context->declarations = resect_collection_create();
    context->decl_table = NULL;

    return context;
}

void resect_context_free(resect_translation_context context, resect_set deallocated) {
    if (!resect_set_add(deallocated, context)) {
        return;
    }

    struct resect_decl_table *element, *tmp;
    HASH_ITER(hh, context->decl_table, element, tmp) {
        HASH_DEL(context->decl_table, element);
        free((void *) element->key);
        free(element);
    }

    resect_decl_collection_free(context->declarations, deallocated);
    free(context);
}

resect_collection resect_context_declarations(resect_translation_context decl) {
    return decl->declarations;
}


void resect_register_decl(resect_translation_context context, const char *id, resect_decl decl) {
    struct resect_decl_table *entry = malloc(sizeof(struct resect_decl_table));

    size_t len = strlen(id);
    char *key = malloc(sizeof(char) * (len + 1));
    strncpy(key, id, len);
    key[len] = 0;

    entry->key = key;
    entry->value = decl;
    HASH_ADD_STR(context->decl_table, key, entry);
}


resect_decl resect_find_decl(resect_translation_context context, const char *id) {
    struct resect_decl_table *entry = NULL;
    HASH_FIND_STR(context->decl_table, id, entry);
    if (entry == NULL) {
        return NULL;
    }
    return entry->value;
}

enum CXChildVisitResult resect_visit_context_declarations(CXCursor cursor,
                                                          CXCursor parent,
                                                          CXClientData data) {
    resect_translation_context context = data;
    resect_collection_add(context->declarations, resect_parse_decl(context, cursor));
    return CXChildVisit_Continue;
}

/*
 * PARSER
 */
struct resect_parse_options {
    resect_collection args;
};

resect_parse_options resect_options_create() {
    resect_parse_options opts = malloc(sizeof(struct resect_parse_options));
    opts->args = resect_collection_create();
    return opts;
}

void resect_options_add(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_from_c(key));
    resect_collection_add(opts->args, resect_string_from_c(value));
}

void resect_options_add_concat(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_format("%s%s", key, value));
}

void resect_options_add_include_path(resect_parse_options opts, const char *path) {
    resect_options_add(opts, "--include-directory", path);
}

void resect_options_add_framework_path(resect_parse_options opts, const char *framework) {
    resect_options_add_concat(opts, "-F", framework);
}

void resect_options_add_language(resect_parse_options opts, const char *lang) {
    resect_options_add(opts, "--language", lang);
}

void resect_options_add_standard(resect_parse_options opts, const char *standard) {
    resect_options_add(opts, "--std", standard);
}

void resect_options_free(resect_parse_options opts) {
    resect_iterator arg_iter = resect_collection_iterator(opts->args);
    while (resect_iterator_next(arg_iter)) {
        resect_string_free(resect_iterator_value(arg_iter));
    }
    resect_iterator_free(arg_iter);
    resect_collection_free(opts->args);
    free(opts);
}

resect_translation_context resect_parse(const char *filename, resect_parse_options options) {
    const char **argv;
    int argc;
    if (options != NULL) {
        argc = (int) resect_collection_size(options->args);
        argv = malloc(argc * sizeof(char *));

        resect_iterator arg_iter = resect_collection_iterator(options->args);
        int i = 0;
        while (resect_iterator_next(arg_iter)) {
            resect_string arg = resect_iterator_value(arg_iter);
            argv[i++] = resect_string_to_c(arg);
        }
        resect_iterator_free(arg_iter);
    } else {
        argc = 0;
        argv = NULL;
    }

    resect_translation_context context = resect_context_create();

    CXIndex index = clang_createIndex(0, 1);

    CXTranslationUnit clangUnit = clang_parseTranslationUnit(index, filename, argv, argc,
                                                             NULL,
                                                             0, CXTranslationUnit_DetailedPreprocessingRecord |
                                                                CXTranslationUnit_KeepGoing |
                                                                CXTranslationUnit_SkipFunctionBodies);

    free(argv);


    CXCursor cursor = clang_getTranslationUnitCursor(clangUnit);

    clang_visitChildren(cursor, resect_visit_context_declarations, context);

    clang_disposeTranslationUnit(clangUnit);
    clang_disposeIndex(index);

    return context;
}

void resect_free(resect_translation_context result) {
    resect_set deallocated = resect_set_create();
    resect_context_free(result, deallocated);
    resect_set_free(deallocated);
}