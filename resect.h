#ifndef RESECT_H
#define RESECT_H

#include <clang-c/Index.h>

#if defined(_WIN32)
#  define RESECT_API __declspec(dllexport)
#elif defined(__GNUC__)
#  define RESECT_API __attribute__((visibility("default")))
#else
#  define RESECT_API
#endif

#define MAX_FILENAME_LENGTH 16383

typedef struct resect_object *resect_object;

typedef void (*resect_visitor)(resect_object current_cursor, resect_object parent_cursor);

typedef enum {
    RESECT_CURSOR_KIND_UNKNOWN
} resect_cursor_kind;

/*
 * STRING
 */
typedef struct resect_string resect_string;

RESECT_API resect_string *resect_create_string(unsigned int initial_capacity);

RESECT_API const char *resect_string_to_c(resect_string *string);

RESECT_API void resect_destroy_string(resect_string *string);

/*
 * INDEX
 */
RESECT_API resect_object resect_create_index();

RESECT_API void resect_destroy_index(resect_object index);


/*
 * TRANSLATION UNIT
 */
typedef struct resect_parse_options {

} resect_parse_options;

RESECT_API resect_object resect_parse_translation_unit(resect_object index,
                                                       const char *source,
                                                       resect_parse_options *options);

RESECT_API void resect_destroy_translation_unit(resect_object unit);

/*
 * CURSOR
 */
typedef struct resect_cursor_location resect_cursor_location;

RESECT_API resect_cursor_location *resect_create_cursor_location();

RESECT_API void resect_destroy_cursor_location(resect_cursor_location *location);

RESECT_API unsigned int resect_cursor_location_line(resect_cursor_location *location);

RESECT_API unsigned int resect_cursor_location_column(resect_cursor_location *location);

RESECT_API const char *resect_cursor_location_name(resect_cursor_location *location);

RESECT_API resect_object resect_acquire_null_cursor();

RESECT_API resect_object resect_acquire_translation_unit_cursor(resect_object translation_unit);

RESECT_API void resect_release_cursor(resect_object cursor);

RESECT_API void resect_visit_children(resect_object cursor, resect_visitor visitor);

RESECT_API resect_cursor_kind resect_get_cursor_kind(resect_object cursor);

RESECT_API resect_cursor_location *resect_get_cursor_location(resect_object cursor, resect_cursor_location *location);

#endif //RESECT_H