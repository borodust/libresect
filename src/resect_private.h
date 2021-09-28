#ifndef RESECT_PRIVATE_H
#define RESECT_PRIVATE_H

#define CINDEX_NO_EXPORTS

#include "../resect.h"
#include <clang-c/Index.h>
#include <stdio.h>
#include <stdbool.h>

/*
 * STRING
 */
typedef struct P_resect_string *resect_string;

resect_string resect_string_format(const char *format, ...);

resect_string resect_string_from_clang(CXString from);

resect_string resect_string_from_c(const char *string);

resect_string resect_string_copy(resect_string string);

resect_string resect_string_update(resect_string string, const char *new);

resect_string resect_string_append(resect_string string, const char *postfix);

size_t resect_string_length(resect_string string);

resect_string resect_substring(resect_string string, long long start, long long end);

resect_bool resect_string_equal(resect_string this, resect_string that);

resect_bool resect_string_equal_c(resect_string this, const char *that);

resect_bool resect_string_starts_with_c(resect_string this, const char *prefix);

void resect_string_free(resect_string string);

const char *resect_string_to_c(resect_string string);

/*
 * COLLECTION
 */
resect_collection resect_collection_create();

void resect_collection_free(resect_collection collection);

void resect_collection_add(resect_collection collection, void *value);

void *resect_collection_pop_last(resect_collection collection);

void *resect_collection_peek_last(resect_collection collection);

unsigned int resect_collection_size(resect_collection collection);

/*
 * SET
 */
typedef struct P_resect_set *resect_set;

resect_set resect_set_create();

void resect_set_free(resect_set set);

resect_bool resect_set_contains(resect_set set, void *value);

resect_bool resect_set_add(resect_set set, void *value);

void resect_set_add_to_collection(resect_set set, resect_collection collection);

/*
 * ITERATOR
 */
resect_bool resect_iterator_next(resect_iterator iter);

void *resect_iterator_value(resect_iterator iter);

void resect_iterator_free(resect_iterator iter);

/*
 * HASH TABLE
 */
typedef struct P_resect_table *resect_table;

resect_table resect_table_create();

resect_bool resect_table_put_if_absent(resect_table table, const char *key, void *value);

void *resect_table_get(resect_table table, const char *key);

void resect_visit_table(resect_table table,
                        resect_bool (*entry_visitor)(void *, const char *, void *),
                        void *context);

void resect_table_free(resect_table table, void (*value_destructor)(void *, void *), void *context);

/*
 * FILTERING
 */

typedef struct P_resect_filtering_context *resect_filtering_context;

resect_filtering_context resect_filtering_context_create(resect_parse_options options);

void resect_filtering_context_free(resect_filtering_context context);

resect_inclusion_status resect_filtering_explicit_inclusion_status(resect_filtering_context context,
                                                                   const char *declaration_name,
                                                                   const char *declaration_source);


resect_inclusion_status resect_filtering_inclusion_status(resect_filtering_context context);

void resect_filtering_push_inclusion_status(resect_filtering_context context, resect_inclusion_status status);

resect_inclusion_status resect_filtering_pop_inclusion_status(resect_filtering_context context);

/*
 * CONTEXT
 */
typedef struct P_resect_translation_context *resect_translation_context;

enum P_resect_garbage_kind {
    RESECT_GARBAGE_KIND_TEMPLATE_ARGUMENT,
    RESECT_GARBAGE_KIND_DECL,
    RESECT_GARBAGE_KIND_TYPE,
};

resect_translation_context resect_context_create(resect_parse_options opts);

resect_collection resect_create_decl_collection(resect_translation_context context);

void resect_context_free(resect_translation_context context, resect_set deallocated);

enum CXChildVisitResult resect_visit_context_child(CXCursor cursor,
                                                   CXCursor parent,
                                                   CXClientData data);

void resect_register_decl(resect_translation_context context, resect_string id, resect_decl decl);

void resect_register_decl_language(resect_translation_context context, resect_language language);

resect_language resect_get_assumed_language(resect_translation_context context);

resect_inclusion_status resect_cursor_inclusion_status(resect_translation_context context, CXCursor cursor);

resect_inclusion_status resect_context_inclusion_status(resect_translation_context context);

void resect_context_push_inclusion_status(resect_translation_context context, resect_inclusion_status status);

resect_inclusion_status resect_context_pop_inclusion_status(resect_translation_context context);

void resect_export_decl(resect_translation_context context, resect_decl decl);

resect_decl resect_find_decl(resect_translation_context context, resect_string decl_id);

void resect_register_template_parameter(resect_translation_context context, resect_string name, resect_decl decl);

resect_decl resect_find_template_parameter(resect_translation_context context, resect_string name);

void *resect_context_current_state(resect_translation_context context);

void resect_context_push_state(resect_translation_context context, void *value);

void *resect_context_pop_state(resect_translation_context context);

void resect_register_garbage(resect_translation_context context, enum P_resect_garbage_kind kind, void *garbage);

/*
 * TYPE
 */
resect_type resect_type_create(resect_translation_context context, CXType canonical_type);

void resect_type_free(resect_type type, resect_set deallocated);

void resect_type_collection_free(resect_collection types, resect_set deallocated);

/*
 * DECLARATION
 */
typedef void (*resect_data_deallocator)(void *data, resect_set deallocated);

typedef struct {
    resect_decl_kind kind;
    resect_decl decl;
} resect_decl_result;

resect_decl_result resect_decl_create(resect_translation_context context, CXCursor cursor);

void resect_decl_free(resect_decl decl, resect_set deallocated);

void resect_decl_collection_free(resect_collection decls, resect_set deallocated);

enum CXChildVisitResult resect_visit_child_declaration(CXCursor cursor,
                                                       CXCursor parent,
                                                       CXClientData data);

resect_string resect_location_to_string(resect_location location);

resect_string resect_format_cursor_namespace(CXCursor cursor);

resect_location resect_location_from_cursor(CXCursor cursor);

void resect_location_free(resect_location location);

resect_string resect_extract_decl_id(CXCursor cursor);

void resect_register_exclusion(resect_translation_context translation_context);

void resect_reset_registered_exclusion(resect_translation_context translation_context);

bool resect_is_exclusion_detected(resect_translation_context translation_context);

/*
 * TEMPLATE ARGUMENT
 */
resect_template_argument resect_template_argument_create(resect_template_argument_kind kind,
                                                         resect_type type,
                                                         long long int value,
                                                         int arg_number);

void resect_template_argument_free(resect_template_argument arg, resect_set deallocated);

void resect_template_argument_collection_free(resect_collection args, resect_set deallocated);

resect_template_argument_kind convert_template_argument_kind(enum CXTemplateArgumentKind kind);

/*
 * UTIL
 */
long long filter_valid_value(long long value);

void resect_string_collection_free(resect_collection collection);

resect_string resect_extract_decl_id(CXCursor cursor);

resect_string resect_format_cursor_full_name(CXCursor cursor);


/*
 * OPTIONS
 */
resect_collection resect_options_get_included_definitions(resect_parse_options opts);

resect_collection resect_options_get_included_sources(resect_parse_options opts);

resect_collection resect_options_get_excluded_definitions(resect_parse_options opts);

resect_collection resect_options_get_excluded_sources(resect_parse_options opts);

#endif //RESECT_PRIVATE_H
