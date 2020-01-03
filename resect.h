#ifndef RESECT_H
#define RESECT_H

#if defined(_WIN32)
#  define RESECT_API __declspec(dllexport)
#elif defined(__GNUC__)
#  define RESECT_API __attribute__((visibility("default")))
#else
#  define RESECT_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RESECT_CURSOR_KIND_UNKNOWN = 0,
    RESECT_CURSOR_KIND_TRANSLATION_UNIT = 1,
    RESECT_CURSOR_KIND_STRUCT = 2,
    RESECT_CURSOR_KIND_UNION = 3,
    RESECT_CURSOR_KIND_CLASS = 4,
    RESECT_CURSOR_KIND_ENUM = 5,
    RESECT_CURSOR_KIND_FIELD = 6,
    RESECT_CURSOR_KIND_FUNCTION = 7,
    RESECT_CURSOR_KIND_VARIABLE = 8,
    RESECT_CURSOR_KIND_PARAMETER = 9,
    RESECT_CURSOR_KIND_TYPEDEF = 10,
    RESECT_CURSOR_KIND_METHOD = 11,
    RESECT_CURSOR_KIND_NAMESPACE = 12,
    RESECT_CURSOR_KIND_CONSTRUCTOR = 13,
    RESECT_CURSOR_KIND_DESTRUCTOR = 14,
    RESECT_CURSOR_KIND_CONVERTER = 15,
    RESECT_CURSOR_KIND_TYPE_REFERENCE = 16,
    RESECT_CURSOR_KIND_TEMPLATE_REFERENCE = 17,
    RESECT_CURSOR_KIND_ENUM_CONSTANT = 18
} resect_cursor_kind;


typedef enum {
    RESECT_TYPE_KIND_UNKNOWN = 0,
    RESECT_TYPE_KIND_VOID = 2,
    RESECT_TYPE_KIND_BOOL = 3,
    RESECT_TYPE_KIND_CHAR_U = 4,
    RESECT_TYPE_KIND_UCHAR = 5,
    RESECT_TYPE_KIND_CHAR16 = 6,
    RESECT_TYPE_KIND_CHAR32 = 7,
    RESECT_TYPE_KIND_USHORT = 8,
    RESECT_TYPE_KIND_UINT = 9,
    RESECT_TYPE_KIND_ULONG = 10,
    RESECT_TYPE_KIND_ULONGLONG = 11,
    RESECT_TYPE_KIND_UINT128 = 12,
    RESECT_TYPE_KIND_CHAR_S = 13,
    RESECT_TYPE_KIND_SCHAR = 14,
    RESECT_TYPE_KIND_WCHAR = 15,
    RESECT_TYPE_KIND_SHORT = 16,
    RESECT_TYPE_KIND_INT = 17,
    RESECT_TYPE_KIND_LONG = 18,
    RESECT_TYPE_KIND_LONGLONG = 19,
    RESECT_TYPE_KIND_INT128 = 20,
    RESECT_TYPE_KIND_FLOAT = 21,
    RESECT_TYPE_KIND_DOUBLE = 22,
    RESECT_TYPE_KIND_LONGDOUBLE = 23,
    RESECT_TYPE_KIND_NULLPTR = 24,
    RESECT_TYPE_KIND_OVERLOAD = 25,
    RESECT_TYPE_KIND_DEPENDENT = 26,

    RESECT_TYPE_KIND_FLOAT128 = 30,
    RESECT_TYPE_KIND_HALF = 31,
    RESECT_TYPE_KIND_FLOAT16 = 32,

    RESECT_TYPE_KIND_COMPLEX = 100,
    RESECT_TYPE_KIND_POINTER = 101,
    RESECT_TYPE_KIND_BLOCKPOINTER = 102,
    RESECT_TYPE_KIND_LVALUEREFERENCE = 103,
    RESECT_TYPE_KIND_RVALUEREFERENCE = 104,
    RESECT_TYPE_KIND_RECORD = 105,
    RESECT_TYPE_KIND_ENUM = 106,
    RESECT_TYPE_KIND_TYPEDEF = 107,
    RESECT_TYPE_KIND_FUNCTIONNOPROTO = 110,
    RESECT_TYPE_KIND_FUNCTIONPROTO = 111,
    RESECT_TYPE_KIND_CONSTANTARRAY = 112,
    RESECT_TYPE_KIND_VECTOR = 113,
    RESECT_TYPE_KIND_INCOMPLETEARRAY = 114,
    RESECT_TYPE_KIND_VARIABLEARRAY = 115,
    RESECT_TYPE_KIND_DEPENDENTSIZEDARRAY = 116,
    RESECT_TYPE_KIND_MEMBERPOINTER = 117,
    RESECT_TYPE_KIND_AUTO = 118,

    RESECT_TYPE_KIND_ELABORATED = 119,
    RESECT_TYPE_KIND_ATTRIBUTED = 163,

    RESECT_TYPE_KIND_EXTVECTOR = 178
} resect_type_kind;


typedef enum {
    RESECT_VISIT_RESULT_RECURSE = 0,
    RESECT_VISIT_RESULT_BREAK = 1,
    RESECT_VISIT_RESULT_CONTINUE = 2
} resect_visit_result;

typedef enum {
    RESECT_EVALUATION_RESULT_KIND_UNKNOWN = 0,
    RESECT_EVALUATION_RESULT_KIND_INT = 1,
    RESECT_EVALUATION_RESULT_KIND_FLOAT = 2,
    RESECT_EVALUATION_RESULT_KIND_STRING = 3,
} resect_evaluation_result_kind;

typedef struct resect_cursor *resect_cursor;

typedef struct resect_parse_options *resect_parse_options;

typedef struct resect_cursor_location *resect_cursor_location;

typedef struct resect_type *resect_type;

typedef struct resect_string *resect_string;

typedef struct resect_evaluation_result *resect_evaluation_result;

typedef resect_visit_result (*resect_visitor)(resect_cursor current_cursor, resect_cursor parent_cursor);

/*
 * STRING
 */
RESECT_API resect_string resect_allocate_string(unsigned int initial_capacity);

RESECT_API const char *resect_string_to_c(resect_string string);

RESECT_API void resect_free_string(resect_string string);

/*
 * TYPE
 */
RESECT_API resect_type resect_allocate_type();

RESECT_API void resect_free_type(resect_type type);

RESECT_API resect_type_kind resect_type_get_kind(resect_type type);

RESECT_API resect_string resect_type_get_name(resect_string result, resect_type type);

RESECT_API long long resect_type_sizeof(resect_type type);

RESECT_API long long resect_type_alignof(resect_type type);

RESECT_API long long resect_type_offsetof(resect_type type, const char *field_name);

/*
 * CURSOR
 */
RESECT_API resect_cursor resect_allocate_cursor();

RESECT_API void resect_free_cursor(resect_cursor cursor);

RESECT_API int resect_cursor_equal(resect_cursor this, resect_cursor that);

RESECT_API resect_string resect_cursor_get_name(resect_string result, resect_cursor cursor);

RESECT_API resect_cursor_location resect_allocate_cursor_location();

RESECT_API void resect_free_cursor_location(resect_cursor_location location);

RESECT_API unsigned int resect_cursor_location_line(resect_cursor_location location);

RESECT_API unsigned int resect_cursor_location_column(resect_cursor_location location);

RESECT_API const char *resect_cursor_location_name(resect_cursor_location location);

RESECT_API resect_cursor_kind resect_cursor_get_kind(resect_cursor cursor);

RESECT_API resect_string resect_cursor_get_id(resect_string provided, resect_cursor cursor);

RESECT_API resect_cursor_location resect_cursor_get_location(resect_cursor_location result, resect_cursor cursor);

RESECT_API resect_string resect_cursor_get_comment(resect_string result, resect_cursor cursor);

RESECT_API resect_cursor resect_type_get_declaration(resect_cursor provided_result, resect_type type);

RESECT_API resect_type resect_cursor_get_type(resect_type result, resect_cursor cursor);

RESECT_API resect_string resect_cursor_get_debug_info(resect_string result, resect_cursor cursor);

/*
 * TYPEDEF
 */
RESECT_API resect_type resect_cursor_get_aliased_type(resect_type provided, resect_cursor cursor);
/*
 * ENUM
 */
RESECT_API long long resect_cursor_get_enum_value(resect_cursor cursor);
/*
 * PARSER
 */
RESECT_API void resect_parse(const char *filename, resect_visitor visitor, resect_parse_options options);

#ifdef __csplusplus
}
#endif
#endif //RESECT_H