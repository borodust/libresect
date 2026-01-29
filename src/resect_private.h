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

resect_string resect_string_update_c(resect_string string, const char *new);

resect_string resect_string_append_c(resect_string string, const char *postfix);

resect_string resect_string_append(resect_string string, resect_string postfix);

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

unsigned int resect_set_size(resect_set set);

resect_bool resect_set_contains(resect_set set, void *value);

resect_bool resect_set_add(resect_set set, void *value);

void resect_set_remove_all(resect_set set);

void resect_set_add_to_collection(resect_set set, resect_collection collection);

void resect_visit_set(resect_set set, resect_bool (*item_visitor)(void *ctx, void *item), void *context);

/*
 * HASH TABLE
 */
typedef struct P_resect_table *resect_table;

resect_table resect_table_create();

resect_bool resect_table_put_if_absent(resect_table table, const char *key, void *value);

/**
 * @return previous value or NULL when no key found
 */
void *resect_table_put(resect_table table, const char *key, void *value);

void *resect_table_get(resect_table table, const char *key);

bool resect_table_remove(resect_table table, const char *key);

void resect_visit_table(resect_table table,
                        resect_bool (*entry_visitor)(void *, const char *, void *),
                        void *context);

unsigned int resect_table_size(resect_table table);

void resect_print_table(resect_table table);

void resect_table_free(resect_table table, void (*value_destructor)(void *, void *), void *context);

/*
 * FILTERING
 */
typedef enum {
    RESECT_FILTER_STATUS_IGNORED = 0,
    RESECT_FILTER_STATUS_EXCLUDED = 1,
    RESECT_FILTER_STATUS_INCLUDED = 2,
    RESECT_FILTER_STATUS_ENFORCED = 3,
} resect_filter_status;

typedef struct P_resect_filtering_context *resect_filtering_context;

resect_filtering_context resect_filtering_context_create(resect_parse_options options);

void resect_filtering_context_free(resect_filtering_context context);

resect_filter_status resect_filtering_status(resect_filtering_context context,
                                             const char *declaration_name,
                                             const char *declaration_source);

/*
 * TREE SHAKING
*/

typedef struct P_resect_visit_context *resect_visit_context;

typedef struct P_resect_inclusion_registry *resect_inclusion_registry;

typedef void (*resect_declaration_visitor)(resect_visit_context ctx, CXCursor cursor, void *data);

typedef struct P_resect_shaking_context *resect_shaking_context;

resect_shaking_context resect_shaking_context_create(resect_parse_options opts);

void resect_shaking_context_free(resect_shaking_context ctx);

void resect_decl_shake(resect_visit_context visit_context, CXCursor cursor, void *data);

resect_inclusion_registry resect_inclusion_registry_create(resect_shaking_context shaking_context);

bool resect_inclusion_registry_decl_included(resect_inclusion_registry, const char *decl_id);

void resect_inclusion_registry_free(resect_inclusion_registry registry);

/*
 * CONTEXT
*/

typedef struct P_resect_translation_context *resect_translation_context;

enum P_resect_garbage_kind {
    RESECT_GARBAGE_KIND_TEMPLATE_ARGUMENT,
    RESECT_GARBAGE_KIND_DECL,
    RESECT_GARBAGE_KIND_TYPE,
};

resect_translation_context resect_context_create(resect_parse_options opts, resect_inclusion_registry registry);

resect_collection resect_create_decl_collection(resect_translation_context context);

void resect_context_init_printing_policy(resect_translation_context context, CXCursor cursor);

void resect_context_release_printing_policy(resect_translation_context context);

CXPrintingPolicy resect_context_get_printing_policy(resect_translation_context context);

void resect_context_free(resect_translation_context context, resect_set deallocated);

void resect_register_decl(resect_translation_context context, resect_string id, resect_decl decl);

bool resect_register_type(resect_translation_context context, CXType clang_type, resect_type resect_type);

void resect_register_decl_language(resect_translation_context context, resect_language language);

resect_language resect_get_assumed_language(resect_translation_context context);

bool resect_is_decl_included(resect_translation_context context, resect_string decl_id);

void resect_export_decl(resect_translation_context context, resect_decl decl);

resect_decl resect_find_decl(resect_translation_context context, resect_string decl_id);

resect_type resect_find_type(resect_translation_context context, CXType clang_type);

void resect_register_template_parameter(resect_translation_context context, resect_string name, resect_decl decl);

resect_decl resect_find_template_parameter(resect_translation_context context, resect_string name);

void resect_register_garbage(resect_translation_context context, enum P_resect_garbage_kind kind, void *garbage);

void resect_context_flush_template_parameters(resect_translation_context context);

resect_visit_context resect_visit_context_create(resect_declaration_visitor visitor);

void resect_visit_context_free(resect_visit_context ctx);

void resect_visit_cursor_children(resect_visit_context context, CXCursor cursor, void *data);

void resect_visit_cursor(resect_visit_context context, CXCursor cursor, void *data);

CXCursor resect_find_declaration_owning_cursor(CXCursor cursor);

bool resect_context_extract_valid_decl_name(resect_translation_context context, resect_string name, resect_string out);

resect_string resect_cursor_pretty_print(resect_translation_context context, CXCursor cursor);

/*
 * TYPE
 */
resect_type resect_type_create(resect_visit_context visit_context, resect_translation_context context,
                               CXType canonical_type);

void resect_type_free(resect_type type, resect_set deallocated);

void resect_type_collection_free(resect_collection types, resect_set deallocated);

void resect_type_set_free(resect_set types, resect_set deallocated);

resect_type_field resect_field_create(resect_visit_context visit_context, resect_translation_context context, CXType parent, CXCursor cursor);

void resect_field_collection_free(resect_collection fields, resect_set deallocated);

resect_type_kind convert_type_kind(enum CXTypeKind kind);

resect_type_category get_type_category(resect_type_kind kind);

resect_string resect_type_pretty_print(resect_translation_context context, CXType type);

resect_string resect_string_fqn_from_type(resect_translation_context context, CXType clang_type);

resect_string resect_string_fqn_from_type_by_cursor(CXCursor cursor, CXType type);

/*
 * DECLARATION
 */

typedef struct P_resect_decl_visit_data *resect_decl_visit_data;

typedef void (*resect_data_deallocator)(void *data, resect_set deallocated);

typedef struct {
    resect_decl_kind kind;
    resect_decl decl;
} resect_decl_result;

resect_decl_result resect_decl_create(resect_visit_context visit_context,
                                      resect_translation_context context,
                                      CXCursor cursor);

void resect_decl_free(resect_decl decl, resect_set deallocated);

void resect_decl_collection_free(resect_collection decls, resect_set deallocated);

resect_string resect_location_to_string(resect_location location);

resect_string resect_format_cursor_namespace(CXCursor cursor);

resect_location resect_location_from_cursor(CXCursor cursor);

void resect_location_free(resect_location location);

resect_string resect_extract_decl_id(CXCursor cursor);

void resect_decl_register_specialization(resect_decl decl, resect_type specialization);

resect_bool resect_is_forward_declaration(CXCursor cursor);

resect_bool resect_is_specialized(CXCursor cursor);

resect_decl_kind convert_cursor_kind(CXCursor cursor);

bool is_cursor_anonymous(CXCursor cursor);

void resect_decl_parse(resect_visit_context visit_context, CXCursor cursor, void *data);

resect_decl_visit_data resect_decl_visit_data_create(resect_translation_context context);

void resect_visit_decl_data_free(resect_decl_visit_data data);

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

resect_string resect_format_cursor_full_name(CXCursor cursor);

resect_string resect_format_cursor_source(CXCursor cursor);

unsigned long resect_hash(const char *str);


/*
 * OPTIONS
 */
resect_collection resect_options_get_included_definitions(resect_parse_options opts);

resect_collection resect_options_get_included_sources(resect_parse_options opts);

resect_collection resect_options_get_excluded_definitions(resect_parse_options opts);

resect_collection resect_options_get_excluded_sources(resect_parse_options opts);

resect_collection resect_options_get_enforced_definitions(resect_parse_options opts);

resect_collection resect_options_get_enforced_sources(resect_parse_options opts);

resect_diagnostics_level resect_options_current_diagnostics_level(resect_parse_options opts);

resect_bool convert_bool_from_uint(unsigned int val);

/*
 * PATTERN
*/
typedef struct P_resect_pattern *resect_pattern;

resect_pattern resect_pattern_create(resect_string pattern);

resect_pattern resect_pattern_create_c(const char* pattern);

void resect_pattern_free(resect_pattern pattern);

bool resect_pattern_match(resect_pattern pattern, resect_string subject);

bool resect_pattern_match_c(resect_pattern pattern, const char* subject);

/**
 * @param out updated with matched substring if match is found
 * @return true, if match found and substring successfully put into result, false if no match found or error
 */
bool resect_pattern_find(resect_pattern pattern, resect_string subject, resect_string out);

/**
 * @param out updated with matched substring if match is found
 * @return true, if match found and substring successfully put into result, false if no match found or error
 */
bool resect_pattern_find_c(resect_pattern pattern, const char *subject, resect_string out);


#endif //RESECT_PRIVATE_H
