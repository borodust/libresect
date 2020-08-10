#ifndef RESECT_PRIVATE_H
#define RESECT_PRIVATE_H

#include "../resect.h"
#include <clang-c/Index.h>
#include <bits/types/FILE.h>

/*
 * STRING
 */
typedef struct resect_string *resect_string;

resect_string resect_string_format(const char *format, ...);

resect_string resect_string_from_clang(CXString from);

resect_string resect_string_from_c(const char *string);

resect_string resect_string_copy(resect_string string);

resect_string resect_string_update(resect_string string, const char *new);

resect_string resect_string_append(resect_string string, const char *postfix);

size_t resect_string_length(resect_string string);

resect_string resect_substring(resect_string string, long long start, long long end);


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
typedef struct resect_set *resect_set;

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
typedef struct resect_table *resect_table;

resect_table resect_table_create();

resect_bool resect_table_put_if_absent(resect_table table, const char *key, void *value);

void *resect_table_get(resect_table table, const char *key);

void resect_visit_table(resect_table table,
                        resect_bool (*entry_visitor)(void *, const char *, void *),
                        void *context);

void resect_table_free(resect_table table, void (*value_destructor)(void *, void *), void *context);

/*
 * MEMORY FILE
 */
typedef struct resect_memory_file *resect_memory_file;

resect_memory_file resect_memory_file_create(const char **filenames, int count);

void resect_memory_file_flush(resect_memory_file file);

FILE *resect_memory_file_handle(resect_memory_file file);

void resect_memory_file_free(resect_memory_file file);

/*
 * CONTEXT
 */

typedef struct resect_translation_context *resect_translation_context;

resect_translation_context resect_context_create();

resect_collection resect_create_decl_collection(resect_translation_context context);

void resect_context_free(resect_translation_context context, resect_set deallocated);

enum CXChildVisitResult resect_visit_context_child(CXCursor cursor,
                                                   CXCursor parent,
                                                   CXClientData data);

void resect_register_decl(resect_translation_context context, resect_string id, resect_decl decl);

void resect_register_decl_language(resect_translation_context context, resect_language language);

resect_language resect_get_assumed_language(resect_translation_context context);

void resect_expose_decl(resect_translation_context context, resect_decl decl);

resect_decl resect_find_decl(resect_translation_context context, resect_string decl_id);

void resect_register_template_parameter(resect_translation_context context, resect_string name, resect_decl decl);

resect_decl resect_find_template_parameter(resect_translation_context context, resect_string name);

/*
 * TYPE
 */
resect_type resect_type_create(resect_translation_context context, CXType canonical_type);

void resect_type_free(resect_type type, resect_set deallocated);

/*
 * DECLARATION
 */
typedef void (*resect_data_deallocator)(void *data, resect_set deallocated);

resect_decl resect_decl_create(resect_translation_context context, CXCursor cursor);

void resect_decl_free(resect_decl decl, resect_set deallocated);

void resect_decl_collection_free(resect_collection decls, resect_set deallocated);

enum CXChildVisitResult resect_visit_child_declaration(CXCursor cursor,
                                                       CXCursor parent,
                                                       CXClientData data);

/*
 * UTIL
 */
long long filter_valid_value(long long value);

void resect_string_collection_free(resect_collection collection);

#endif //RESECT_PRIVATE_H
