#ifndef RESECT_PRIVATE_H
#define RESECT_PRIVATE_H

#include "../resect.h"
#include <clang-c/Index.h>

/*
 * SET
 */
typedef struct resect_set *resect_set;

resect_set resect_set_create();

void resect_set_free(resect_set set);

resect_bool resect_set_contains(resect_set set, void *value);

resect_bool resect_set_add(resect_set set, void *value);

/*
 * STRING
 */
typedef struct resect_string *resect_string;

resect_string resect_string_format(const char *format, ...);

resect_string resect_string_from_clang(CXString from);

resect_string resect_string_from_c(const char *string);

void resect_string_free(resect_string string);

const char *resect_string_to_c(resect_string string);

/*
 * COLLECTION
 */
resect_collection resect_collection_create();

void resect_collection_free(resect_collection collection);

void resect_collection_add(resect_collection collection, void *value);

unsigned int resect_collection_size(resect_collection collection);

resect_iterator resect_collection_iterator(resect_collection collection);

/*
 * ITERATOR
 */
resect_bool resect_iterator_next(resect_iterator iter);

void *resect_iterator_value(resect_iterator iter);

void resect_iterator_free(resect_iterator iter);

/*
 * TYPE
 */
resect_type resect_type_create(resect_translation_context context, CXType clangType);

/*
 * DECLARATION
 */
typedef void (*data_deallocator)(void *data, resect_set deallocated);

resect_decl resect_parse_decl(resect_translation_context context, CXCursor cursor);

void resect_decl_free(resect_decl decl, resect_set deallocated);

void resect_register_decl(resect_translation_context context, const char *id, resect_decl decl);

resect_translation_context resect_decl_get_context(resect_decl decl);

resect_decl resect_find_decl(resect_translation_context context, const char *id);

void resect_decl_collection_free(resect_collection decls, resect_set deallocated);

/*
 * FIELD
 */
/*
 * UTIL
 */
long long filter_valid_value(long long value);

#endif //RESECT_PRIVATE_H
