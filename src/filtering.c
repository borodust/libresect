#ifndef RESECT_FILTERING_H
#define RESECT_FILTERING_H

#define PCRE2_STATIC
#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>
#include <assert.h>
#include <string.h>

#include "resect_private.h"

struct resect_filtering_context {
    resect_collection included_definition_patterns;
    resect_collection included_source_patterns;
    resect_collection excluded_definition_patterns;
    resect_collection excluded_source_patterns;

    resect_collection status_stack;
};

static void print_pcre_error(int errornumber, size_t erroroffset) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
    fprintf(stderr, "PCRE2 compilation failed at offset %d: %s\n", (int) erroroffset,
            buffer);
}

static resect_collection compile_pattern_collection(resect_collection collection) {
    resect_collection result = resect_collection_create();

    resect_iterator iter = resect_collection_iterator(collection);
    while (resect_iterator_next(iter)) {
        resect_string pattern = resect_iterator_value(iter);

        int errornumber;
        PCRE2_SIZE erroroffset;
        pcre2_code *compiled = pcre2_compile((PCRE2_SPTR) (resect_string_to_c(pattern)),
                                             PCRE2_ZERO_TERMINATED,
                                             PCRE2_UTF,
                                             &errornumber, &erroroffset,
                                             NULL);
        if (compiled == NULL) {
            print_pcre_error(errornumber, erroroffset);
            // FIXME: add better error reporting
            assert(!"Failed to parse inclusion/exclusion pattern");
        }

        resect_collection_add(result, compiled);
    }
    resect_iterator_free(iter);

    return result;
}

static void free_pattern_collection(resect_collection collection) {
    resect_iterator iter = resect_collection_iterator(collection);
    while (resect_iterator_next(iter)) {
        pcre2_code *compiled = resect_iterator_value(iter);
        pcre2_code_free(compiled);
    }
    resect_iterator_free(iter);

    resect_collection_free(collection);
}

resect_filtering_context resect_filtering_context_create(resect_parse_options options) {
    resect_filtering_context context = malloc(sizeof(struct resect_filtering_context));
    context->included_definition_patterns =
            compile_pattern_collection(resect_options_get_included_definitions(options));
    context->included_source_patterns =
            compile_pattern_collection(resect_options_get_included_sources(options));
    context->excluded_definition_patterns =
            compile_pattern_collection(resect_options_get_excluded_definitions(options));
    context->excluded_source_patterns =
            compile_pattern_collection(resect_options_get_excluded_sources(options));

    context->status_stack = resect_collection_create();
    resect_collection_add(context->status_stack, (void *) WEAKLY_EXCLUDED);

    return context;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvoid-pointer-to-enum-cast"

static resect_inclusion_status current_inclusion_status(resect_filtering_context context) {
    return (resect_inclusion_status) resect_collection_peek_last(context->status_stack);
}

static void push_inclusion_status(resect_filtering_context context,
                                  resect_inclusion_status status) {
    resect_collection_add(context->status_stack, (void *) status);
}

static resect_inclusion_status pop_inclusion_status(resect_filtering_context context) {
    resect_collection status_stack = context->status_stack;
    if (resect_collection_size(status_stack) <= 1) {
        assert(!"Inclusion status stack exhausted");
    }
    return (resect_inclusion_status) resect_collection_pop_last(status_stack);
}

#pragma clang diagnostic pop

void resect_filtering_context_free(resect_filtering_context context) {
    free_pattern_collection(context->included_definition_patterns);
    free_pattern_collection(context->included_source_patterns);
    free_pattern_collection(context->excluded_definition_patterns);
    free_pattern_collection(context->excluded_source_patterns);

    resect_collection_free(context->status_stack);
    free(context);
}

static bool match_pattern_collection(resect_collection collection, const char *subject) {
    bool result = false;

    resect_iterator iter = resect_collection_iterator(collection);
    while (resect_iterator_next(iter)) {
        pcre2_code *compiled = resect_iterator_value(iter);
        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(compiled, NULL);

        int rc = pcre2_match(compiled,
                             (PCRE2_SPTR) subject,
                             PCRE2_ZERO_TERMINATED,
                             0,
                             0,
                             match_data,
                             NULL);
        pcre2_match_data_free(match_data);

        if (rc >= 0) {
            result = true;
            goto done;
        }
    }

    done:
    resect_iterator_free(iter);
    return result;
}

resect_inclusion_status resect_filtering_explicit_inclusion_status(resect_filtering_context context,
                                                                   const char *declaration_name,
                                                                   const char *declaration_source) {
    if (match_pattern_collection(context->excluded_definition_patterns, declaration_name)
        || match_pattern_collection(context->excluded_source_patterns, declaration_source)) {
        return EXCLUDED;
    }

    if (match_pattern_collection(context->included_definition_patterns, declaration_name)
        || match_pattern_collection(context->included_source_patterns, declaration_source)) {
        return INCLUDED;
    }

    return WEAKLY_EXCLUDED;
}

resect_inclusion_status resect_filtering_inclusion_status(resect_filtering_context context) {
    return current_inclusion_status(context);
}

void resect_filtering_push_inclusion_status(resect_filtering_context context, resect_inclusion_status status) {
    push_inclusion_status(context, status);
}

resect_inclusion_status resect_filtering_pop_inclusion_status(resect_filtering_context context) {
    return pop_inclusion_status(context);
}

#endif // RESECT_FILTERING_H