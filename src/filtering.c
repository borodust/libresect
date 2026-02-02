#ifndef RESECT_FILTERING_H
#define RESECT_FILTERING_H

#include <stdlib.h>

#include "resect_private.h"

struct P_resect_filtering_context {
    resect_collection included_definition_patterns;
    resect_collection included_source_patterns;
    resect_collection excluded_definition_patterns;
    resect_collection excluded_source_patterns;
    resect_collection enforced_definition_patterns;
    resect_collection enforced_source_patterns;
    resect_collection ignored_definition_patterns;
    resect_collection ignored_source_patterns;
};

static resect_collection compile_pattern_collection(resect_collection collection) {
    resect_collection result = resect_collection_create();

    resect_iterator iter = resect_collection_iterator(collection);
    while (resect_iterator_next(iter)) {
        resect_string pattern = resect_iterator_value(iter);
        resect_pattern compiled = resect_pattern_create(pattern);
        resect_collection_add(result, compiled);
    }
    resect_iterator_free(iter);

    return result;
}

static void free_pattern_collection(resect_collection collection) {
    resect_iterator iter = resect_collection_iterator(collection);
    while (resect_iterator_next(iter)) {
        resect_pattern compiled = resect_iterator_value(iter);
        resect_pattern_free(compiled);
    }
    resect_iterator_free(iter);
    resect_collection_free(collection);
}

resect_filtering_context resect_filtering_context_create(resect_parse_options options) {
    resect_filtering_context context = malloc(sizeof(struct P_resect_filtering_context));
    context->included_definition_patterns =
            compile_pattern_collection(resect_options_get_included_definitions(options));
    context->included_source_patterns = compile_pattern_collection(resect_options_get_included_sources(options));
    context->excluded_definition_patterns =
            compile_pattern_collection(resect_options_get_excluded_definitions(options));
    context->excluded_source_patterns = compile_pattern_collection(resect_options_get_excluded_sources(options));
    context->enforced_definition_patterns =
            compile_pattern_collection(resect_options_get_enforced_definitions(options));
    context->enforced_source_patterns = compile_pattern_collection(resect_options_get_enforced_sources(options));
    context->ignored_definition_patterns =
            compile_pattern_collection(resect_options_get_ignored_definitions(options));
    context->ignored_source_patterns = compile_pattern_collection(resect_options_get_ignored_sources(options));

    return context;
}

void resect_filtering_context_free(resect_filtering_context context) {
    free_pattern_collection(context->included_definition_patterns);
    free_pattern_collection(context->included_source_patterns);
    free_pattern_collection(context->excluded_definition_patterns);
    free_pattern_collection(context->excluded_source_patterns);
    free_pattern_collection(context->enforced_definition_patterns);
    free_pattern_collection(context->enforced_source_patterns);
    free_pattern_collection(context->ignored_definition_patterns);
    free_pattern_collection(context->ignored_source_patterns);

    free(context);
}

static bool match_pattern_collection(resect_collection collection, const char *subject) {
    bool result = false;

    resect_iterator iter = resect_collection_iterator(collection);
    while (resect_iterator_next(iter)) {
        resect_pattern compiled = resect_iterator_value(iter);
        if (resect_pattern_match_c(compiled, subject)) {
            result = true;
            goto done;
        }
    }
done:
    resect_iterator_free(iter);
    return result;
}

resect_filter_status resect_filtering_status(resect_filtering_context context, const char *declaration_name,
                                             const char *declaration_source) {
    if (match_pattern_collection(context->enforced_definition_patterns, declaration_name) ||
        match_pattern_collection(context->enforced_source_patterns, declaration_source)) {
        return RESECT_FILTER_STATUS_ENFORCED;
    }

    if (match_pattern_collection(context->excluded_definition_patterns, declaration_name) ||
        match_pattern_collection(context->excluded_source_patterns, declaration_source)) {
        return RESECT_FILTER_STATUS_EXCLUDED;
        }

    if (match_pattern_collection(context->ignored_definition_patterns, declaration_name) ||
        match_pattern_collection(context->ignored_source_patterns, declaration_source)) {
        return RESECT_FILTER_STATUS_IGNORED;
        }

    if (match_pattern_collection(context->included_definition_patterns, declaration_name) ||
        match_pattern_collection(context->included_source_patterns, declaration_source)) {
        return RESECT_FILTER_STATUS_INCLUDED;
    }

    return RESECT_FILTER_STATUS_IGNORED;
}
#endif // RESECT_FILTERING_H
