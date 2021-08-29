#ifndef RESECT_H
#define RESECT_H

#if defined(_WIN32)
#  if !defined(RESECT_API_NOEXPORT)
#    define RESECT_API __declspec(dllexport)
#  else
#    define RESECT_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define RESECT_API __attribute__((visibility("default")))
#else
#  define RESECT_API
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define resect_true (1)
#define resect_false (0)

typedef int resect_bool;

typedef enum {
    RESECT_DECL_KIND_UNKNOWN = 0,
    RESECT_DECL_KIND_STRUCT = 1,
    RESECT_DECL_KIND_UNION = 2,
    RESECT_DECL_KIND_CLASS = 3,
    RESECT_DECL_KIND_ENUM = 4,
    RESECT_DECL_KIND_FIELD = 5,
    RESECT_DECL_KIND_FUNCTION = 6,
    RESECT_DECL_KIND_VARIABLE = 7,
    RESECT_DECL_KIND_PARAMETER = 8,
    RESECT_DECL_KIND_TYPEDEF = 9,
    RESECT_DECL_KIND_METHOD = 10,
    RESECT_DECL_KIND_ENUM_CONSTANT = 11,
    RESECT_DECL_KIND_MACRO = 12,
    RESECT_DECL_KIND_TEMPLATE_PARAMETER = 13,
    RESECT_DECL_KIND_UNDECLARED = 14,
} resect_decl_kind;

typedef enum {
    RESECT_ACCESS_SPECIFIER_UNKNOWN = 0,
    RESECT_ACCESS_SPECIFIER_PUBLIC = 1,
    RESECT_ACCESS_SPECIFIER_PROTECTED = 2,
    RESECT_ACCESS_SPECIFIER_PRIVATE = 3,
} resect_access_specifier;


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

    RESECT_TYPE_KIND_ATOMIC = 177,
    RESECT_TYPE_KIND_EXTVECTOR = 178,

    RESECT_TYPE_KIND_TEMPLATE_PARAMETER = 10000
} resect_type_kind;

typedef enum {
    RESECT_TYPE_CATEGORY_UNKNOWN = 0,
    RESECT_TYPE_CATEGORY_ARITHMETIC = 1,
    RESECT_TYPE_CATEGORY_POINTER = 2,
    RESECT_TYPE_CATEGORY_REFERENCE = 3,
    RESECT_TYPE_CATEGORY_ARRAY = 4,
    RESECT_TYPE_CATEGORY_UNIQUE = 5,
    RESECT_TYPE_CATEGORY_AUX = 6
} resect_type_category;

typedef enum {
    RESECT_FUNCTION_CALLING_CONVENTION_UNKNOWN = 0,
    RESECT_FUNCTION_CALLING_CONVENTION_DEFAULT = 1,
    RESECT_FUNCTION_CALLING_CONVENTION_C = 2,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_STDCALL = 3,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_FASTCALL = 4,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_THISCALL = 5,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_REGCALL = 6,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_VECTORCALL = 7,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_PASCAL = 8,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_64_WIN64 = 9,
    RESECT_FUNCTION_CALLING_CONVENTION_X86_64_SYSV = 10,
    RESECT_FUNCTION_CALLING_CONVENTION_AARCH64_VECTORCALL = 11,
    RESECT_FUNCTION_CALLING_CONVENTION_AAPCS = 12,
    RESECT_FUNCTION_CALLING_CONVENTION_AAPCS_VFP = 13,
    RESECT_FUNCTION_CALLING_CONVENTION_INTEL_OCL_BICC = 14,
    RESECT_FUNCTION_CALLING_CONVENTION_SWIFT = 15,
    RESECT_FUNCTION_CALLING_CONVENTION_PRESERVE_MOST = 16,
    RESECT_FUNCTION_CALLING_CONVENTION_PRESERVE_ALL = 17,
} resect_function_calling_convention;


typedef enum {
    RESECT_FUNCTION_STORAGE_CLASS_UNKNOWN = 0,
    RESECT_FUNCTION_STORAGE_CLASS_NONE = 1,
    RESECT_FUNCTION_STORAGE_CLASS_EXTERN = 2,
    RESECT_FUNCTION_STORAGE_CLASS_STATIC = 3,
    RESECT_FUNCTION_STORAGE_CLASS_PRIVATE_EXTERN = 4,
    RESECT_FUNCTION_STORAGE_CLASS_OPENCL_WORKGROUP_LOCAL = 5,
    RESECT_FUNCTION_STORAGE_CLASS_AUTO = 6,
    RESECT_FUNCTION_STORAGE_CLASS_REGISTER = 7,
} resect_function_storage_class;


typedef enum {
    RESECT_VARIABLE_TYPE_UNKNOWN = 0,
    RESECT_VARIABLE_TYPE_INT,
    RESECT_VARIABLE_TYPE_FLOAT,
    RESECT_VARIABLE_TYPE_STRING,
    RESECT_VARIABLE_TYPE_OTHER,
} resect_variable_kind;

typedef enum {
    RESECT_LANGUAGE_UNKNOWN = 0,
    RESECT_LANGUAGE_C,
    RESECT_LANGUAGE_CXX,
    RESECT_LANGUAGE_OBJC,

    RESECT_LANGUAGE__LAST = RESECT_LANGUAGE_OBJC,
} resect_language;

typedef enum {
    RESECT_TEMPLATE_PARAMETER_KIND_UNKNOWN = 0,
    RESECT_TEMPLATE_PARAMETER_KIND_TEMPLATE,
    RESECT_TEMPLATE_PARAMETER_KIND_TYPE,
    RESECT_TEMPLATE_PARAMETER_KIND_NON_TYPE,
} resect_template_parameter_kind;

typedef enum {
    RESECT_TEMPLATE_ARGUMENT_KIND_UNKNOWN = 0,
    RESECT_TEMPLATE_ARGUMENT_KIND_NULL,
    RESECT_TEMPLATE_ARGUMENT_KIND_TYPE,
    RESECT_TEMPLATE_ARGUMENT_KIND_DECLARATION,
    RESECT_TEMPLATE_ARGUMENT_KIND_NULL_PTR,
    RESECT_TEMPLATE_ARGUMENT_KIND_INTEGRAL,
    RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE,
    RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE_EXPANSION,
    RESECT_TEMPLATE_ARGUMENT_KIND_EXPRESSION,
    RESECT_TEMPLATE_ARGUMENT_KIND_PACK
} resect_template_argument_kind;

typedef enum {
    RESECT_LINKAGE_KIND_UNKNOWN = 0,
    RESECT_LINKAGE_KIND_NO_LINKAGE = 1,
    RESECT_LINKAGE_KIND_INTERNAL = 2,
    RESECT_LINKAGE_KIND_UNIQUE_EXTERNAL = 3,
    RESECT_LINKAGE_KIND_EXTERNAL = 4
} resect_linkage_kind;

typedef enum {
    RESECT_OPTION_INTRINSICS_UNKNOWN = 0,
    RESECT_OPTION_INTRINSICS_SSE = 1,
    RESECT_OPTION_INTRINSICS_SSE2 = 2,
    RESECT_OPTION_INTRINSICS_SSE3 = 3,
    RESECT_OPTION_INTRINSICS_SSE41 = 4,
    RESECT_OPTION_INTRINSICS_SSE42 = 5,
    RESECT_OPTION_INTRINSICS_AVX = 6,
    RESECT_OPTION_INTRINSICS_AVX2 = 7,
    RESECT_OPTION_INTRINSICS_NEON = 8,
} resect_option_intrinsic;

typedef enum {
    RESECT_INCLUSION_STATUS_EXCLUDED = 0,
    RESECT_INCLUSION_STATUS_WEAKLY_EXCLUDED = 1,
    RESECT_INCLUSION_STATUS_WEAKLY_INCLUDED = 2,
    RESECT_INCLUSION_STATUS_WEAKLY_ENFORCED = 3,
    RESECT_INCLUSION_STATUS_INCLUDED = 4
} resect_inclusion_status;

typedef struct P_resect_translation_unit *resect_translation_unit;
typedef struct P_resect_collection *resect_collection;
typedef struct P_resect_iterator *resect_iterator;
typedef struct P_resect_location *resect_location;
typedef struct P_resect_decl *resect_decl;
typedef struct P_resect_type *resect_type;
typedef struct P_resect_template_argument *resect_template_argument;

/*
 * COLLECTION
 */
RESECT_API resect_iterator resect_collection_iterator(resect_collection collection);

RESECT_API resect_bool resect_iterator_next(resect_iterator iter);

RESECT_API void *resect_iterator_value(resect_iterator iter);

RESECT_API void resect_iterator_free(resect_iterator iter);

/*
 * LOCATION
 */
RESECT_API unsigned int resect_location_line(resect_location location);

RESECT_API unsigned int resect_location_column(resect_location location);

RESECT_API const char *resect_location_name(resect_location location);

/*
 * TYPE
 */
RESECT_API resect_type_kind resect_type_get_kind(resect_type type);

RESECT_API const char *resect_type_get_name(resect_type type);

RESECT_API long long resect_type_sizeof(resect_type type);

RESECT_API long long resect_type_alignof(resect_type type);

RESECT_API long long resect_type_offsetof(resect_type type, const char *field_name);

RESECT_API resect_collection resect_type_fields(resect_type type);

RESECT_API resect_decl resect_type_get_declaration(resect_type type);

RESECT_API resect_type_category resect_type_get_category(resect_type type);

RESECT_API resect_bool resect_type_is_const_qualified(resect_type decl);

RESECT_API resect_bool resect_type_is_pod(resect_type decl);

RESECT_API resect_collection resect_type_template_arguments(resect_type type);

RESECT_API resect_bool resect_type_is_undeclared(resect_type type);

/*
 * TEMPLATE ARGUMENT
 */
RESECT_API resect_template_argument_kind resect_template_argument_get_kind(resect_template_argument arg);

RESECT_API resect_type resect_template_argument_get_type(resect_template_argument arg);

RESECT_API long long resect_template_argument_get_value(resect_template_argument arg);

RESECT_API int resect_template_argument_get_position(resect_template_argument arg);

/*
 * ARRAY
 */
RESECT_API long long resect_array_get_size(resect_type type);

RESECT_API resect_type resect_array_get_element_type(resect_type type);

/*
 * POINTER
 */
RESECT_API resect_type resect_pointer_get_pointee_type(resect_type type);

/*
 * REFERENCE
 */
RESECT_API resect_type resect_reference_get_pointee_type(resect_type type);

RESECT_API resect_bool resect_reference_is_lvalue(resect_type type);

/*
 * FUNCTION PROTO
 */
RESECT_API resect_type resect_function_proto_get_result_type(resect_type type);

RESECT_API resect_collection resect_function_proto_parameters(resect_type type);

RESECT_API resect_bool resect_function_proto_is_variadic(resect_type type);

/*
 * DECLARATION
 */
RESECT_API resect_decl_kind resect_decl_get_kind(resect_decl decl);

RESECT_API const char *resect_decl_get_id(resect_decl decl);

RESECT_API const char *resect_decl_get_name(resect_decl decl);

RESECT_API const char *resect_decl_get_namespace(resect_decl decl);

RESECT_API const char *resect_decl_get_mangled_name(resect_decl decl);

RESECT_API resect_location resect_decl_get_location(resect_decl decl);

RESECT_API const char *resect_decl_get_comment(resect_decl decl);

RESECT_API resect_access_specifier resect_decl_get_access_specifier(resect_decl decl);

RESECT_API resect_decl resect_decl_get_template(resect_decl decl);

RESECT_API resect_bool resect_decl_is_partially_specialized(resect_decl decl);

RESECT_API resect_bool resect_decl_is_template(resect_decl decl);

RESECT_API resect_collection resect_decl_template_parameters(resect_decl decl);

RESECT_API resect_type resect_decl_get_type(resect_decl decl);

RESECT_API resect_decl resect_decl_get_owner(resect_decl decl);

RESECT_API resect_collection resect_decl_template_arguments(resect_decl decl);

RESECT_API const char *resect_decl_get_source(resect_decl decl);

RESECT_API resect_linkage_kind resect_decl_get_linkage(resect_decl decl);

RESECT_API resect_bool resect_decl_is_forward(resect_decl decl);

RESECT_API resect_inclusion_status resect_decl_get_inclusion_status(resect_decl decl);

/*
 * TRANSLATION UNIT
 */
RESECT_API resect_collection resect_unit_declarations(resect_translation_unit unit);

RESECT_API resect_language resect_unit_get_language(resect_translation_unit unit);

/*
 * RECORD
 */
RESECT_API long long resect_field_get_offset(resect_decl decl);

RESECT_API resect_bool resect_field_is_bitfield(resect_decl decl);

RESECT_API long long resect_field_get_width(resect_decl decl);

RESECT_API resect_collection resect_record_fields(resect_decl decl);

RESECT_API resect_collection resect_record_methods(resect_decl decl);

RESECT_API resect_collection resect_record_parents(resect_decl decl);

RESECT_API resect_bool resect_record_is_abstract(resect_decl decl);

/*
 * ENUM
 */
RESECT_API long long resect_enum_constant_value(resect_decl decl);

RESECT_API resect_collection resect_enum_constants(resect_decl decl);

RESECT_API resect_type resect_enum_get_type(resect_decl decl);

/*
 * FUNCTION
 */
RESECT_API resect_collection resect_function_parameters(resect_decl decl);

RESECT_API resect_type resect_function_get_result_type(resect_decl decl);

RESECT_API resect_function_storage_class resect_function_get_storage_class(resect_decl decl);

RESECT_API resect_bool resect_function_is_variadic(resect_decl decl);

RESECT_API resect_bool resect_function_is_inlined(resect_decl decl);


/*
 * VARIABLE
 */
RESECT_API resect_variable_kind resect_variable_get_kind(resect_decl decl);

RESECT_API resect_type resect_variable_get_type(resect_decl decl);

RESECT_API long long resect_variable_get_value_as_int(resect_decl decl);

RESECT_API double resect_variable_get_value_as_float(resect_decl decl);

RESECT_API const char *resect_variable_get_value_as_string(resect_decl decl);

/*
 * TYPEDEF
 */
RESECT_API resect_type resect_typedef_get_aliased_type(resect_decl decl);

/*
 * METHOD
 */
RESECT_API resect_collection resect_method_parameters(resect_decl decl);

RESECT_API resect_type resect_method_get_result_type(resect_decl decl);

RESECT_API resect_function_storage_class resect_method_get_storage_class(resect_decl decl);

RESECT_API resect_bool resect_method_is_variadic(resect_decl decl);

RESECT_API resect_bool resect_method_is_pure_virtual(resect_decl decl);

RESECT_API resect_bool resect_method_is_const(resect_decl decl);

/*
 * MACRO
 */
RESECT_API resect_bool resect_macro_is_function_like(resect_decl decl);

/*
 * TEMPLATE PARAMETER
 */
RESECT_API resect_template_parameter_kind resect_template_parameter_get_kind(resect_decl param);

/*
 * PARSE OPTIONS
 */
typedef struct P_resect_parse_options *resect_parse_options;

RESECT_API resect_parse_options resect_options_create();

RESECT_API void resect_options_include_definition(resect_parse_options opts, const char *name);

RESECT_API void resect_options_include_source(resect_parse_options opts, const char *source);

RESECT_API void resect_options_exclude_definition(resect_parse_options opts, const char *name);

RESECT_API void resect_options_exclude_source(resect_parse_options opts, const char *source);

RESECT_API void resect_options_add_include_path(resect_parse_options opts, const char *path);

RESECT_API void resect_options_add_framework_path(resect_parse_options opts, const char *framework);

RESECT_API void resect_options_add_language(resect_parse_options opts, const char *lang);

RESECT_API void resect_options_add_define(resect_parse_options opts, const char *name, const char* value);

RESECT_API void resect_options_add_standard(resect_parse_options opts, const char *standard);

RESECT_API void resect_options_add_abi(resect_parse_options opts, const char *value);

RESECT_API void resect_options_add_arch(resect_parse_options opts, const char *value);

RESECT_API void resect_options_add_cpu(resect_parse_options opts, const char *value);

RESECT_API void resect_options_add_target(resect_parse_options opts, const char *target);

RESECT_API void resect_options_add_intrinsics(resect_parse_options opts, const char *target);

RESECT_API void resect_options_single_header(resect_parse_options opts);

RESECT_API void resect_options_print_diagnostics(resect_parse_options opts);

RESECT_API void resect_options_intrinsic(resect_parse_options opts, resect_option_intrinsic intrinsic);

RESECT_API void resect_options_free(resect_parse_options opts);

/*
 * PARSER
 */
RESECT_API resect_translation_unit resect_parse(const char *filename, resect_parse_options options);

RESECT_API void resect_free(resect_translation_unit result);

#ifdef __csplusplus
}
#endif
#endif //RESECT_H
