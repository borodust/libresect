#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <string.h>

#include <clang-c/Index.h>

/*
 * PARSER
 */
struct resect_parse_options {
    resect_collection args;
};

void resect_options_add(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_from_c(key));
    resect_collection_add(opts->args, resect_string_from_c(value));
}

void resect_options_add_concat(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_format("%s%s", key, value));
}

resect_parse_options resect_options_create() {
    resect_parse_options opts = malloc(sizeof(struct resect_parse_options));
    opts->args = resect_collection_create();

    resect_collection_add(opts->args, resect_string_from_c("-ferror-limit=0"));

    return opts;
}

void resect_options_add_include_path(resect_parse_options opts, const char *path) {
    resect_options_add(opts, "--include-directory", path);
}

void resect_options_add_framework_path(resect_parse_options opts, const char *framework) {
    resect_options_add_concat(opts, "-F", framework);
}

void resect_options_add_abi(resect_parse_options opts, const char *value) {
    resect_options_add_concat(opts, "-mabi=", value);
}

void resect_options_add_arch(resect_parse_options opts, const char *value) {
    resect_options_add_concat(opts, "-march=", value);
}

void resect_options_add_cpu(resect_parse_options opts, const char *value) {
    resect_options_add_concat(opts, "-mcpu=", value);
}

void resect_options_add_language(resect_parse_options opts, const char *lang) {
    resect_options_add(opts, "--language", lang);
}

void resect_options_add_standard(resect_parse_options opts, const char *standard) {
    resect_options_add(opts, "--std", standard);
}

void resect_options_add_target(resect_parse_options opts, const char *target) {
    resect_options_add(opts, "-target", target);
}

void resect_options_free(resect_parse_options opts) {
    resect_string_collection_free(opts->args);
    free(opts);
}

/*
 * UNIT
 */
struct resect_translation_unit {
    resect_collection declarations;
    resect_translation_context context;
};

resect_collection resect_unit_declarations(resect_translation_unit unit) {
    return unit->declarations;
}

enum CXChildVisitResult resect_visit_context_declarations(CXCursor cursor,
                                                          CXCursor parent,
                                                          CXClientData data) {
    resect_translation_context context = data;
    resect_decl_create(context, cursor);
    return CXChildVisit_Continue;
}

resect_translation_unit resect_parse(const char *filename, resect_parse_options options) {
    const char **clang_argv;
    int clang_argc;
    if (options != NULL) {
        clang_argc = (int) resect_collection_size(options->args);
        clang_argv = malloc(clang_argc * sizeof(char *));

        resect_iterator arg_iter = resect_collection_iterator(options->args);
        int i = 0;
        while (resect_iterator_next(arg_iter)) {
            resect_string arg = resect_iterator_value(arg_iter);
            clang_argv[i++] = resect_string_to_c(arg);
        }
        resect_iterator_free(arg_iter);
    } else {
        clang_argc = 0;
        clang_argv = NULL;
    }

    resect_translation_context context = resect_context_create();

    CXIndex index = clang_createIndex(0, 1);

    CXTranslationUnit clangUnit = clang_parseTranslationUnit(index, filename, clang_argv, clang_argc,
                                                             NULL,
                                                             0, CXTranslationUnit_DetailedPreprocessingRecord |
                                                                CXTranslationUnit_KeepGoing |
                                                                CXTranslationUnit_SkipFunctionBodies);

    free(clang_argv);

    CXCursor cursor = clang_getTranslationUnitCursor(clangUnit);

    clang_visitChildren(cursor, resect_visit_context_declarations, context);

    clang_disposeTranslationUnit(clangUnit);
    clang_disposeIndex(index);

    resect_translation_unit result = malloc(sizeof(struct resect_translation_unit));
    result->context = context;
    result->declarations = resect_create_decl_collection(context);

    return result;
}

void resect_free(resect_translation_unit result) {
    resect_set deallocated = resect_set_create();
    resect_context_free(result->context, deallocated);
    resect_collection_free(result->declarations);
    resect_set_free(deallocated);
    free(result);
}