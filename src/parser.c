#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <string.h>

#include <clang-c/Index.h>

/*
 * PARSER
 */
struct P_resect_parse_options {
    resect_collection args;
    resect_bool single;
    resect_bool diagnostics;

    resect_collection included_definition_patterns;
    resect_collection included_source_patterns;
    resect_collection excluded_definition_patterns;
    resect_collection excluded_source_patterns;
    resect_collection enforced_definition_patterns;
    resect_collection enforced_source_patterns;
};

void resect_options_add(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_from_c(key));
    resect_collection_add(opts->args, resect_string_from_c(value));
}

void resect_options_add_concat(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_format("%s%s", key, value));
}

void resect_options_add_string(resect_parse_options opts, resect_string string) {
    resect_collection_add(opts->args, resect_string_copy(string));
}

resect_parse_options resect_options_create() {
    resect_parse_options opts = malloc(sizeof(struct P_resect_parse_options));
    opts->args = resect_collection_create();
    opts->single = resect_false;
    opts->diagnostics = resect_false;

    opts->included_definition_patterns = resect_collection_create();
    opts->included_source_patterns = resect_collection_create();
    opts->excluded_definition_patterns = resect_collection_create();
    opts->excluded_source_patterns = resect_collection_create();

    opts->enforced_definition_patterns = resect_collection_create();
    opts->enforced_source_patterns = resect_collection_create();

    resect_collection_add(opts->args, resect_string_from_c("-ferror-limit=0"));
    resect_collection_add(opts->args, resect_string_from_c("-fno-implicit-templates"));
    resect_collection_add(opts->args, resect_string_from_c("-fc++-abi=itanium"));
    resect_collection_add(opts->args, resect_string_from_c("-fsyntax-only"));

    return opts;
}

void resect_options_free(resect_parse_options opts) {
    resect_string_collection_free(opts->args);

    resect_string_collection_free(opts->included_definition_patterns);
    resect_string_collection_free(opts->included_source_patterns);

    resect_string_collection_free(opts->excluded_definition_patterns);
    resect_string_collection_free(opts->excluded_source_patterns);

    resect_string_collection_free(opts->enforced_definition_patterns);
    resect_string_collection_free(opts->enforced_source_patterns);

    free(opts);
}

void resect_options_include_definition(resect_parse_options opts, const char *name) {
    resect_collection_add(opts->included_definition_patterns, resect_string_from_c(name));
}

void resect_options_include_source(resect_parse_options opts, const char *name) {
    resect_collection_add(opts->included_source_patterns, resect_string_from_c(name));
}

void resect_options_exclude_definition(resect_parse_options opts, const char *name) {
    resect_collection_add(opts->excluded_definition_patterns, resect_string_from_c(name));
}

void resect_options_exclude_source(resect_parse_options opts, const char *name) {
    resect_collection_add(opts->excluded_source_patterns, resect_string_from_c(name));
}

void resect_options_enforce_definition(resect_parse_options opts, const char *name) {
    resect_collection_add(opts->enforced_definition_patterns, resect_string_from_c(name));
}

void resect_options_enforce_source(resect_parse_options opts, const char *name) {
    resect_collection_add(opts->enforced_source_patterns, resect_string_from_c(name));
}

resect_collection resect_options_get_included_definitions(resect_parse_options opts) {
    return opts->included_definition_patterns;
}

resect_collection resect_options_get_included_sources(resect_parse_options opts) {
    return opts->included_source_patterns;
}

resect_collection resect_options_get_excluded_definitions(resect_parse_options opts) {
    return opts->excluded_definition_patterns;
}

resect_collection resect_options_get_excluded_sources(resect_parse_options opts) {
    return opts->excluded_source_patterns;
}

resect_collection resect_options_get_enforced_definitions(resect_parse_options opts) {
    return opts->enforced_definition_patterns;
}

resect_collection resect_options_get_enforced_sources(resect_parse_options opts) {
    return opts->enforced_source_patterns;
}

void resect_options_add_resource_path(resect_parse_options opts, const char *path) {
    resect_options_add(opts, "-resource-dir", path);
}

void resect_options_add_include_path(resect_parse_options opts, const char *path) {
    resect_options_add(opts, "--include-directory", path);
}

void resect_options_add_include_file(resect_parse_options opts, const char *filepath) {
    resect_options_add_concat(opts, "--include=", filepath);
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
    resect_string str = resect_string_format("-x%s-header", lang);
    resect_options_add_string(opts, str);
    resect_string_free(str);
}

void resect_options_add_define(resect_parse_options opts, const char *name, const char *value) {
    resect_string define = resect_string_format("-D%s=%s", name, value);
    resect_collection_add(opts->args, define);
}

void resect_options_add_standard(resect_parse_options opts, const char *standard) {
    resect_options_add_concat(opts, "-std=", standard);
}

void resect_options_add_target(resect_parse_options opts, const char *target) {
    resect_options_add_concat(opts, "--target=", target);
}

void resect_options_intrinsic(resect_parse_options opts, resect_option_intrinsic intrinsic) {
    if (intrinsic == RESECT_OPTION_INTRINSICS_NEON) {
        resect_options_add_concat(opts, "-mfpu=neon", "");
        return;
    }

    char *intrinsic_name;
    switch (intrinsic) {
        case RESECT_OPTION_INTRINSICS_SSE:
            intrinsic_name = "sse";
            break;
        case RESECT_OPTION_INTRINSICS_SSE2:
            intrinsic_name = "sse2";
            break;
        case RESECT_OPTION_INTRINSICS_SSE3:
            intrinsic_name = "sse3";
            break;
        case RESECT_OPTION_INTRINSICS_SSE41:
            intrinsic_name = "sse4.1";
            break;
        case RESECT_OPTION_INTRINSICS_SSE42:
            intrinsic_name = "sse4.2";
            break;
        case RESECT_OPTION_INTRINSICS_AVX:
            intrinsic_name = "avx";
            break;
        case RESECT_OPTION_INTRINSICS_AVX2:
            intrinsic_name = "avx2";
            break;
        default:
            return;
    }
    resect_options_add_concat(opts, "-m", intrinsic_name);
}

void resect_options_single_header(resect_parse_options opts) {
    opts->single = resect_true;
}

void resect_options_print_diagnostics(resect_parse_options opts) {
    opts->diagnostics = resect_true;
}

/*
 * UNIT
 */
struct P_resect_translation_unit {
    resect_collection declarations;
    resect_translation_context context;
};

resect_collection resect_unit_declarations(resect_translation_unit unit) {
    return unit->declarations;
}

resect_language resect_unit_get_language(resect_translation_unit unit) {
    return resect_get_assumed_language(unit->context);
}

resect_translation_unit resect_parse(const char *filename, resect_parse_options options) {
    int clang_argc = (int) resect_collection_size(options->args);
    char **clang_argv = malloc(clang_argc * sizeof(char *));

    resect_iterator arg_iter = resect_collection_iterator(options->args);
    int i = 0;
    while (resect_iterator_next(arg_iter)) {
        resect_string arg = resect_iterator_value(arg_iter);
        clang_argv[i++] = (char *) resect_string_to_c(arg);
    }
    resect_iterator_free(arg_iter);


    resect_translation_context context = resect_context_create(options);

    CXIndex index = clang_createIndex(0, options->diagnostics ? 1 : 0);
    clang_toggleCrashRecovery(false);

    enum CXTranslationUnit_Flags unitFlags = CXTranslationUnit_DetailedPreprocessingRecord |
                                             CXTranslationUnit_KeepGoing |
                                             CXTranslationUnit_SkipFunctionBodies |
                                             CXTranslationUnit_IncludeAttributedTypes |
                                             CXTranslationUnit_VisitImplicitAttributes;

    if (options->single) {
        unitFlags |= CXTranslationUnit_SingleFileParse;
    }

    CXTranslationUnit clangUnit = clang_parseTranslationUnit(index, filename,
                                                             (const char *const *) clang_argv,
                                                             clang_argc,
                                                             NULL,
                                                             0, unitFlags);

    CXCursor cursor = clang_getTranslationUnitCursor(clangUnit);
    resect_context_init_printing_policy(context, cursor);
    clang_visitChildren(cursor, resect_visit_context_child, context);
    resect_context_release_printing_policy(context);
    clang_disposeTranslationUnit(clangUnit);
    clang_disposeIndex(index);
    free(clang_argv);

    resect_translation_unit result = malloc(sizeof(struct P_resect_translation_unit));
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
