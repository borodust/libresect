#include "../resect.h"
#include "resect_private.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <clang-c/Index.h>

/*
 * LOCATION
 */
struct resect_location {
    unsigned int line;
    unsigned int column;
    resect_string name;
};

unsigned int resect_location_line(resect_location location) {
    return location->line;
}

unsigned int resect_location_column(resect_location location) {
    return location->column;
}

const char *resect_location_name(resect_location location) {
    return resect_string_to_c(location->name);
}

resect_location resect_location_from_cursor(CXCursor cursor) {
    resect_location result = malloc(sizeof(struct resect_location));

    CXFile file;
    clang_getFileLocation(clang_getCursorLocation(cursor),
                          &file,
                          &result->line,
                          &result->column,
                          NULL);

    result->name = resect_string_from_clang(clang_getFileName(file));

    return result;
}

void resect_location_free(resect_location location) {
    resect_string_free(location->name);
    free(location);
}

/*
 * DECLARATION
 */
typedef struct resect_decl_child_visit_data {
    resect_translation_context context;
    resect_decl parent;
} *resect_decl_child_visit_data;

struct resect_decl {
    resect_decl_kind kind;
    resect_string id;
    resect_string name;
    resect_string namespace;
    resect_location location;
    resect_string mangled_name;
    resect_string comment;
    resect_access_specifier access;
    resect_collection template_parameters;

    resect_type type;

    void *data;
    data_deallocator data_deallocator;
};

resect_decl_kind convert_cursor_kind(CXCursor cursor) {

    enum CXCursorKind clang_kind = clang_getTemplateCursorKind(cursor);

    if (clang_kind == CXCursor_NoDeclFound) {
        clang_kind = clang_getCursorKind(cursor);
    }

    switch (clang_kind) {
        case CXCursor_MacroDefinition:
            return RESECT_DECL_KIND_MACRO;
        case CXCursor_StructDecl:
            return RESECT_DECL_KIND_STRUCT;
        case CXCursor_UnionDecl:
            return RESECT_DECL_KIND_UNION;
        case CXCursor_EnumDecl:
            return RESECT_DECL_KIND_ENUM;
        case CXCursor_FieldDecl:
            return RESECT_DECL_KIND_FIELD;
        case CXCursor_EnumConstantDecl:
            return RESECT_DECL_KIND_ENUM_CONSTANT;
        case CXCursor_FunctionTemplate:
        case CXCursor_FunctionDecl:
            return RESECT_DECL_KIND_FUNCTION;
        case CXCursor_VarDecl:
            return RESECT_DECL_KIND_VARIABLE;
        case CXCursor_ParmDecl:
            return RESECT_DECL_KIND_PARAMETER;
        case CXCursor_TypeAliasTemplateDecl:
        case CXCursor_TypedefDecl:
        case CXCursor_TypeAliasDecl:
            return RESECT_DECL_KIND_TYPEDEF;
        case CXCursor_TemplateRef:
        case CXCursor_ClassTemplate:
        case CXCursor_ClassTemplatePartialSpecialization:
        case CXCursor_ClassDecl:
            return RESECT_DECL_KIND_CLASS;
        case CXCursor_CXXMethod:
        case CXCursor_Constructor:
        case CXCursor_Destructor:
        case CXCursor_ConversionFunction:
            return RESECT_DECL_KIND_METHOD;
        case CXCursor_TemplateTemplateParameter:
        case CXCursor_TemplateTypeParameter:
        case CXCursor_NonTypeTemplateParameter:
            return RESECT_DECL_KIND_TEMPLATE_PARAMETER;
        default:
            return RESECT_DECL_KIND_UNKNOWN;
    }
}

resect_access_specifier convert_access_specifier(enum CX_CXXAccessSpecifier specifier) {
    switch (specifier) {
        case CX_CXXInvalidAccessSpecifier:
            return RESECT_ACCESS_SPECIFIER_UNKNOWN;
        case CX_CXXPublic:
            return RESECT_ACCESS_SPECIFIER_PUBLIC;
        case CX_CXXProtected:
            return RESECT_ACCESS_SPECIFIER_PROTECTED;
        case CX_CXXPrivate:
            return RESECT_ACCESS_SPECIFIER_PRIVATE;
        default:
            return RESECT_ACCESS_SPECIFIER_UNKNOWN;
    }
}

resect_bool resect_is_forward_declaration(CXCursor cursor) {
    CXCursor definition = clang_getCursorDefinition(cursor);

    return clang_equalCursors(definition, clang_getNullCursor())
           || !clang_equalCursors(cursor, definition);
}

resect_bool resect_is_specializable(CXCursor cursor) {
    if (clang_equalCursors(clang_getSpecializedCursorTemplate(cursor), clang_getNullCursor())) {
        return resect_false;
    } else {
        return resect_true;
    }
}

void resect_decl_init_from_cursor(resect_decl decl, resect_translation_context context, CXCursor cursor) {
    decl->kind = convert_cursor_kind(cursor);
    decl->id = resect_string_from_clang(clang_getCursorUSR(cursor));
    decl->name = resect_string_from_clang(clang_getCursorSpelling(cursor));
    decl->comment = resect_string_from_clang(clang_Cursor_getRawCommentText(cursor));
    decl->location = resect_location_from_cursor(cursor);
    decl->namespace = resect_string_copy(resect_namespace(context));
    decl->access = convert_access_specifier(clang_getCXXAccessSpecifier(cursor));
    decl->mangled_name = resect_string_from_clang(clang_Cursor_getMangling(cursor));
    decl->template_parameters = resect_collection_create();

    decl->data_deallocator = NULL;
    decl->data = NULL;

    decl->type = resect_type_create(context, clang_getCursorType(cursor));
}


void resect_record_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_enum_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_enum_constant_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_function_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_variable_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_typedef_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_field_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_method_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_macro_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_template_parameter_init(resect_translation_context context, resect_decl decl, CXCursor cursor);

void resect_parse_namespace(resect_translation_context, CXCursor cursor);

resect_language convert_language(enum CXLanguageKind language) {
    switch (language) {
        case CXLanguage_C:
            return RESECT_LANGUAGE_C;
        case CXLanguage_ObjC:
            return RESECT_LANGUAGE_OBJC;
        case CXLanguage_CPlusPlus:
            return RESECT_LANGUAGE_CXX;
        default:
            return RESECT_LANGUAGE_UNKNOWN;
    }
}

enum CXChildVisitResult resect_visit_child_declarations(CXCursor cursor,
                                                        CXCursor parent,
                                                        CXClientData data) {
    resect_translation_context context = data;
    resect_decl_create(context, cursor);
    return CXChildVisit_Continue;
}

resect_decl resect_decl_create(resect_translation_context context, CXCursor cursor) {
    if (resect_is_forward_declaration(cursor)) {
        CXCursor definition = clang_getCursorDefinition(cursor);
        if ((clang_getCursorKind(definition) < CXCursor_FirstInvalid
             || clang_getCursorKind(definition) > CXCursor_LastInvalid)
            && !clang_equalCursors(cursor, definition)) {
            return resect_decl_create(context, definition);
        }
    }

    if (resect_is_specializable(cursor)) {
        return resect_decl_create(context, clang_getSpecializedCursorTemplate(cursor));
    }

    switch (clang_getCursorKind(cursor)) {
        case CXCursor_Namespace:
            resect_parse_namespace(context, cursor);
            return NULL;
        case CXCursor_UnexposedDecl:
            /* we might encounter exposed declarations within unexposed one, e.g. inside extern "C"/"C++" block */
            clang_visitChildren(cursor, resect_visit_child_declarations, context);
            return NULL;
    }

    resect_string decl_id = resect_string_from_clang(clang_getCursorUSR(cursor));

    resect_decl registered_decl = resect_find_decl(context, decl_id);
    if (registered_decl != NULL) {
        resect_string_free(decl_id);
        return registered_decl;
    }

    resect_decl decl = malloc(sizeof(struct resect_decl));
    memset(decl, 0, sizeof(struct resect_decl));

    resect_register_decl(context, decl_id, decl);
    resect_register_decl_language(context, convert_language(clang_getCursorLanguage(cursor)));

    resect_decl_init_from_cursor(decl, context, cursor);

    switch (decl->kind) {
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_CLASS:
        case RESECT_DECL_KIND_UNION:
            resect_expose_decl(context, decl);
            resect_record_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_ENUM:
            resect_expose_decl(context, decl);
            resect_enum_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_ENUM_CONSTANT:
            resect_enum_constant_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_FUNCTION:
            resect_expose_decl(context, decl);
            resect_function_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_VARIABLE:
            resect_expose_decl(context, decl);
            resect_variable_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_TYPEDEF:
            resect_expose_decl(context, decl);
            resect_typedef_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_FIELD:
            resect_field_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_METHOD:
            resect_method_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_MACRO:
            resect_macro_init(context, decl, cursor);
            break;
        case RESECT_DECL_KIND_TEMPLATE_PARAMETER:
            resect_template_parameter_init(context, decl, cursor);
            break;
    }
    resect_string_free(decl_id);

    return decl;
}

void resect_decl_free(resect_decl decl, resect_set deallocated) {
    if (!resect_set_add(deallocated, decl)) {
        return;
    }

    if (decl->data_deallocator != NULL) {
        decl->data_deallocator(decl->data, deallocated);
    }

    resect_string_free(decl->id);
    resect_string_free(decl->name);
    resect_location_free(decl->location);
    resect_string_free(decl->comment);
    resect_string_free(decl->namespace);
    resect_string_free(decl->mangled_name);
    resect_type_free(decl->type, deallocated);

    resect_decl_collection_free(decl->template_parameters, deallocated);

    free(decl);
}

resect_decl_kind resect_decl_get_kind(resect_decl decl) {
    return decl->kind;
}

resect_access_specifier resect_decl_get_access_specifier(resect_decl decl) {
    return decl->access;
}

const char *resect_decl_get_id(resect_decl decl) {
    return resect_string_to_c(decl->id);
}

resect_location resect_decl_get_location(resect_decl decl) {
    return decl->location;
}

const char *resect_decl_get_name(resect_decl decl) {
    return resect_string_to_c(decl->name);
}

const char *resect_decl_get_namespace(resect_decl decl) {
    return resect_string_to_c(decl->namespace);
}

const char *resect_decl_get_mangled_name(resect_decl decl) {
    return resect_string_to_c(decl->mangled_name);
}

const char *resect_decl_get_comment(resect_decl decl) {
    return resect_string_to_c(decl->comment);
}

resect_type resect_decl_get_type(resect_decl decl) {
    return decl->type;
}

resect_collection resect_decl_template_parameters(resect_decl decl) {
    return decl->template_parameters;
}

void resect_decl_collection_free(resect_collection decls, resect_set deallocated) {
    resect_iterator iter = resect_collection_iterator(decls);
    while (resect_iterator_next(iter)) {
        resect_decl decl = resect_iterator_value(iter);
        resect_decl_free(decl, deallocated);
    }
    resect_iterator_free(iter);
    resect_collection_free(decls);
}

/*
 * RECORD
 */
typedef struct resect_field_data {
    resect_bool bitfield;
    long long width;
    long long offset;
} *resect_field_data;

typedef struct resect_record_data {
    resect_collection fields;
    resect_collection methods;
    resect_collection parents;
} *resect_record_data;

resect_bool resect_is_struct(resect_decl decl) {
    return decl->kind == RESECT_DECL_KIND_CLASS
           || decl->kind == RESECT_DECL_KIND_STRUCT
           || decl->kind == RESECT_DECL_KIND_UNION;
}

resect_collection resect_record_methods(resect_decl decl) {
    assert(resect_is_struct(decl));
    resect_record_data data = decl->data;
    return data->methods;
}

resect_collection resect_record_fields(resect_decl decl) {
    assert(resect_is_struct(decl));
    resect_record_data data = decl->data;
    return data->fields;
}

resect_collection resect_record_parents(resect_decl decl) {
    assert(resect_is_struct(decl));
    resect_record_data data = decl->data;
    return data->parents;
}

long long resect_field_get_offset(resect_decl field_decl) {
    assert(field_decl->kind == RESECT_DECL_KIND_FIELD);
    resect_field_data data = field_decl->data;
    return data->offset;
}

resect_bool resect_field_is_bitfield(resect_decl field_decl) {
    assert(field_decl->kind == RESECT_DECL_KIND_FIELD);
    resect_field_data data = field_decl->data;
    return data->bitfield;
}

long long resect_field_get_width(resect_decl field_decl) {
    assert(field_decl->kind == RESECT_DECL_KIND_FIELD);
    resect_field_data data = field_decl->data;
    return data->width;
}

void resect_field_data_free(void *data, resect_set deallocated) {
    resect_field_data field = data;
    if (!resect_set_add(deallocated, field)) {
        return;
    }
    free(field);
}

void resect_field_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_field_data data = malloc(sizeof(struct resect_field_data));

    data->offset = filter_valid_value(clang_Cursor_getOffsetOfField(cursor));
    data->bitfield = clang_Cursor_isBitField(cursor) != 0 ? resect_true : resect_false;
    data->width = clang_getFieldDeclBitWidth(cursor);

    decl->data_deallocator = resect_field_data_free;
    decl->data = data;
}

enum CXChildVisitResult resect_visit_record_child(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl_child_visit_data visit_data = data;

    resect_record_data record_data = visit_data->parent->data;

    if (clang_getCursorKind(cursor) == CXCursor_CXXBaseSpecifier) {
        CXCursor base_type = clang_getTypeDeclaration(clang_getCursorType(cursor));
        resect_collection_add(record_data->parents, resect_decl_create(visit_data->context, base_type));
        return CXChildVisit_Continue;
    }

    resect_decl decl = resect_decl_create(visit_data->context, cursor);
    if (decl) {
        switch (decl->kind) {
            case RESECT_DECL_KIND_METHOD:
                resect_collection_add(record_data->methods, decl);
                break;
            case RESECT_DECL_KIND_FIELD:
                resect_collection_add(record_data->fields, decl);
                break;
            case RESECT_DECL_KIND_TEMPLATE_PARAMETER:
                resect_collection_add(visit_data->parent->template_parameters, decl);
                break;
        }
    }

    return CXChildVisit_Continue;
}

void resect_record_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_record_data record_data = data;

    resect_decl_collection_free(record_data->methods, deallocated);
    resect_decl_collection_free(record_data->fields, deallocated);
    resect_decl_collection_free(record_data->parents, deallocated);

    free(data);
}

void resect_record_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_record_data data = malloc(sizeof(struct resect_record_data));
    data->methods = resect_collection_create();
    data->fields = resect_collection_create();
    data->parents = resect_collection_create();

    decl->data_deallocator = resect_record_data_free;
    decl->data = data;

    struct resect_decl_child_visit_data visit_data = {.context =  context, .parent =  decl};
    clang_visitChildren(cursor, resect_visit_record_child, &visit_data);
}

/*
 * TYPEDEF
 */
typedef struct resect_typedef_data {
    resect_type aliased_type;
} *resect_typedef_data;


void resect_typedef_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_typedef_data typedef_data = data;
    resect_type_free(typedef_data->aliased_type, deallocated);
    free(typedef_data);
}

void resect_typedef_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_typedef_data data = malloc(sizeof(struct resect_typedef_data));

    CXType canonical_type = clang_getCanonicalType(clang_getTypedefDeclUnderlyingType(cursor));
    data->aliased_type = resect_type_create(context, canonical_type);

    decl->data_deallocator = resect_typedef_data_free;
    decl->data = data;
}

resect_type resect_typedef_get_aliased_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_TYPEDEF);
    resect_typedef_data typedef_data = decl->data;
    return typedef_data->aliased_type;
}

/*
 * FUNCTION
 */
typedef struct resect_function_data {
    resect_bool variadic;
    resect_function_storage_class storage_class;
    resect_collection parameters;
    resect_function_calling_convention calling_convention;
    resect_type result_type;
} *resect_function_data;

resect_type resect_function_get_result_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->result_type;
}

resect_function_storage_class resect_function_get_storage_class(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->storage_class;
}

resect_bool resect_function_is_variadic(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->variadic;
}

resect_collection resect_function_parameters(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->parameters;
}

void resect_visit_parameter(resect_translation_context context, CXCursor cursor, resect_function_data data) {
    resect_decl decl = resect_decl_create(context, cursor);
    if (decl != NULL && decl->kind == RESECT_DECL_KIND_PARAMETER) {
        resect_collection_add(data->parameters, decl);
    }
}

enum CXChildVisitResult resect_visit_function_parameter(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl_child_visit_data visit_data = data;
    resect_function_data function_data = visit_data->parent->data;

    resect_visit_parameter(visit_data->context, cursor, function_data);

    return CXChildVisit_Continue;
}

resect_function_calling_convention convert_calling_convention(enum CXCallingConv calling_convention) {
    switch (calling_convention) {
        case CXCallingConv_Default:
            return RESECT_FUNCTION_CALLING_CONVENTION_DEFAULT;
        case CXCallingConv_C:
            return RESECT_FUNCTION_CALLING_CONVENTION_C;
        case CXCallingConv_X86StdCall:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_STDCALL;
        case CXCallingConv_X86FastCall:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_FASTCALL;
        case CXCallingConv_X86ThisCall:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_THISCALL;
        case CXCallingConv_X86Pascal:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_PASCAL;
        case CXCallingConv_AAPCS:
            return RESECT_FUNCTION_CALLING_CONVENTION_AAPCS;
        case CXCallingConv_AAPCS_VFP:
            return RESECT_FUNCTION_CALLING_CONVENTION_AAPCS_VFP;
        case CXCallingConv_X86RegCall:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_REGCALL;
        case CXCallingConv_IntelOclBicc:
            return RESECT_FUNCTION_CALLING_CONVENTION_INTEL_OCL_BICC;
        case CXCallingConv_Win64:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_64_WIN64;
        case CXCallingConv_X86_64SysV:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_64_SYSV;
        case CXCallingConv_X86VectorCall:
            return RESECT_FUNCTION_CALLING_CONVENTION_X86_VECTORCALL;
        case CXCallingConv_Swift:
            return RESECT_FUNCTION_CALLING_CONVENTION_SWIFT;
        case CXCallingConv_PreserveMost:
            return RESECT_FUNCTION_CALLING_CONVENTION_PRESERVE_MOST;
        case CXCallingConv_PreserveAll:
            return RESECT_FUNCTION_CALLING_CONVENTION_PRESERVE_ALL;
        case CXCallingConv_AArch64VectorCall:
            return RESECT_FUNCTION_CALLING_CONVENTION_AARCH64_VECTORCALL;
        default:
            return RESECT_FUNCTION_CALLING_CONVENTION_UNKNOWN;
    }
}

resect_function_storage_class convert_storage_class(enum CX_StorageClass storage_class) {
    switch (storage_class) {
        case CX_SC_None:
            return RESECT_FUNCTION_STORAGE_CLASS_NONE;
        case CX_SC_Extern:
            return RESECT_FUNCTION_STORAGE_CLASS_EXTERN;
        case CX_SC_Static:
            return RESECT_FUNCTION_STORAGE_CLASS_STATIC;
        case CX_SC_PrivateExtern:
            return RESECT_FUNCTION_STORAGE_CLASS_PRIVATE_EXTERN;
        case CX_SC_OpenCLWorkGroupLocal:
            return RESECT_FUNCTION_STORAGE_CLASS_OPENCL_WORKGROUP_LOCAL;
        case CX_SC_Auto:
            return RESECT_FUNCTION_STORAGE_CLASS_AUTO;
        case CX_SC_Register:
            return RESECT_FUNCTION_STORAGE_CLASS_REGISTER;
        default:
            return RESECT_FUNCTION_STORAGE_CLASS_UNKNOWN;
    }
}

void resect_function_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_function_data function = data;
    resect_decl_collection_free(function->parameters, deallocated);
    resect_type_free(function->result_type, deallocated);
    free(function);
}

resect_function_data resect_function_data_create(resect_translation_context context, CXCursor cursor) {
    resect_function_data data = malloc(sizeof(struct resect_function_data));

    CXType functionType = clang_getCursorType(cursor);
    data->parameters = resect_collection_create();
    data->storage_class = convert_storage_class(clang_Cursor_getStorageClass(cursor));
    data->calling_convention = convert_calling_convention(clang_getFunctionTypeCallingConv(functionType));
    data->variadic = clang_isFunctionTypeVariadic(functionType) != 0 ? resect_true : resect_false;
    data->result_type = resect_type_create(context, clang_getResultType(functionType));


    return data;
}

void resect_function_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    decl->data_deallocator = resect_function_data_free;
    decl->data = resect_function_data_create(context, cursor);

    struct resect_decl_child_visit_data visit_data = {.context = context, .parent =  decl};
    clang_visitChildren(cursor, resect_visit_function_parameter, &visit_data);
}

/*
 * ENUM
 */
typedef struct resect_enum_constant_data {
    long long value;
} *resect_enum_constant_data;


long long resect_enum_constant_value(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_ENUM_CONSTANT);
    resect_enum_constant_data data = decl->data;
    return data->value;
}

void resect_enum_constant_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }

    free(data);
}

void resect_enum_constant_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_enum_constant_data data = malloc(sizeof(struct resect_enum_constant_data));
    data->value = clang_getEnumConstantDeclValue(cursor);
    decl->data_deallocator = resect_enum_constant_free;
    decl->data = data;
}

typedef struct resect_enum_data {
    resect_collection constants;
    resect_type type;
} *resect_enum_data;

resect_collection resect_enum_constants(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_ENUM);
    resect_enum_data data = decl->data;

    return data->constants;
}

resect_type resect_enum_get_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_ENUM);
    resect_enum_data data = decl->data;

    return data->type;
}

enum CXChildVisitResult resect_visit_enum_constants(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl_child_visit_data visit_data = data;
    resect_enum_data enum_data = visit_data->parent->data;

    resect_decl decl = resect_decl_create(visit_data->context, cursor);
    if (decl->kind == RESECT_DECL_KIND_ENUM_CONSTANT) {
        resect_collection_add(enum_data->constants, decl);
    }

    return CXChildVisit_Continue;
}

void resect_enum_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_enum_data enum_data = data;
    resect_type_free(enum_data->type, deallocated);
    resect_decl_collection_free(enum_data->constants, deallocated);
    free(enum_data);
}

void resect_enum_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_enum_data data = malloc(sizeof(struct resect_enum_data));
    data->constants = resect_collection_create();
    data->type = resect_type_create(context, clang_getEnumDeclIntegerType(cursor));

    decl->data_deallocator = resect_enum_data_free;
    decl->data = data;
    struct resect_decl_child_visit_data visit_data = {.context = context, .parent = decl};
    clang_visitChildren(cursor, resect_visit_enum_constants, &visit_data);
}

/*
 * VARIABLE
 */
typedef struct resect_variable_data {
    resect_variable_type type;
    resect_string string_value;
    long long int_value;
    double float_value;
} *resect_variable_data;


resect_variable_type resect_variable_get_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_VARIABLE);
    resect_variable_data data = decl->data;
    return data->type;
}

long long resect_variable_get_value_as_int(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_VARIABLE);
    resect_variable_data data = decl->data;
    return data->int_value;
}

double resect_variable_get_value_as_float(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_VARIABLE);
    resect_variable_data data = decl->data;
    return data->float_value;
}

const char *resect_variable_get_value_as_string(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_VARIABLE);
    resect_variable_data data = decl->data;
    return resect_string_to_c(data->string_value);
}

void resect_variable_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }

    resect_variable_data var_data = data;
    resect_string_free(var_data->string_value);

    free(data);
}

void resect_variable_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    CXEvalResult value = clang_Cursor_Evaluate(cursor);
    resect_variable_data data = malloc(sizeof(struct resect_variable_data));
    data->string_value = resect_string_from_c("");


    switch (clang_EvalResult_getKind(value)) {
        case CXEval_Int:
            data->type = RESECT_VARIABLE_TYPE_INT;
            data->int_value = clang_EvalResult_getAsLongLong(value);
            break;
        case CXEval_Float:
            data->type = RESECT_VARIABLE_TYPE_FLOAT;
            data->float_value = clang_EvalResult_getAsDouble(value);
            break;
        case CXEval_StrLiteral:
            data->type = RESECT_VARIABLE_TYPE_STRING;
            resect_string_update(data->string_value, clang_EvalResult_getAsStr(value));
            break;
        default:
            data->type = RESECT_VARIABLE_TYPE_UNKNOWN;
    }

    decl->data = data;
    decl->data_deallocator = resect_variable_data_free;

    clang_EvalResult_dispose(value);
}

/*
 * MACRO
 */
typedef struct resect_macro_data {
    resect_bool is_function_like;
} *resect_macro_data;

resect_bool resect_macro_is_function_like(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_MACRO);
    resect_macro_data data = decl->data;
    return data->is_function_like;

}

void resect_macro_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }

    free(data);
}

void resect_macro_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_macro_data data = malloc(sizeof(struct resect_macro_data));

    data->is_function_like = clang_Cursor_isMacroFunctionLike(cursor) != 0 ? resect_true : resect_false;

    decl->data_deallocator = resect_macro_data_free;
    decl->data = data;
}

/*
 * METHOD
 */
typedef struct resect_method_data {
    resect_function_data function_data;
} *resect_method_data;

resect_type resect_method_get_result_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->result_type;
}

resect_function_storage_class resect_method_get_storage_class(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->storage_class;
}

resect_bool resect_method_is_variadic(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->variadic;
}

resect_collection resect_method_parameters(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->parameters;
}

enum CXChildVisitResult resect_visit_method_parameters(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl_child_visit_data visit_data = data;
    resect_method_data method_data = visit_data->parent->data;

    resect_visit_parameter(visit_data->context, cursor, method_data->function_data);

    return CXChildVisit_Continue;
}

void resect_method_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }

    resect_method_data method_data = data;
    resect_function_data_free(method_data->function_data, deallocated);

    free(data);
}

void resect_method_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_method_data data = malloc(sizeof(struct resect_method_data));
    data->function_data = resect_function_data_create(context, cursor);

    decl->data_deallocator = resect_method_data_free;
    decl->data = data;

    struct resect_decl_child_visit_data visit_data = {.context = context, .parent =  decl};
    clang_visitChildren(cursor, resect_visit_method_parameters, &visit_data);
}

/*
 * TEMPLATE PARAMETER
 */
typedef struct resect_template_parameter_data {
    resect_template_parameter_kind type;
} *resect_template_parameter_data;

void resect_template_parameter_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    free(data);
}

resect_template_parameter_kind convert_template_parameter_type(enum CXCursorKind kind) {
    switch (kind) {
        case CXCursor_TemplateTemplateParameter:
            return RESECT_TEMPLATE_PARAMETER_KIND_TEMPLATE;
        case CXCursor_TemplateTypeParameter:
            return RESECT_TEMPLATE_PARAMETER_KIND_TYPE;
        case CXCursor_NonTypeTemplateParameter:
            return RESECT_TEMPLATE_PARAMETER_KIND_NONE_TYPE;
        default:
            return RESECT_TEMPLATE_PARAMETER_KIND_UNKNOWN;
    }
}

void resect_template_parameter_init(resect_translation_context context, resect_decl decl, CXCursor cursor) {
    resect_template_parameter_data data = malloc(sizeof(struct resect_template_parameter_data));

    data->type = convert_template_parameter_type(clang_getCursorKind(cursor));

    decl->data_deallocator = resect_template_parameter_data_free;
    decl->data = data;
}

/*
 * NAMESPACE
 */
enum CXChildVisitResult resect_visit_namespace_child(CXCursor cursor,
                                                     CXCursor parent,
                                                     CXClientData data) {
    resect_translation_context context = data;
    resect_decl_create(context, cursor);
    return CXChildVisit_Continue;
}

void resect_parse_namespace(resect_translation_context context, CXCursor cursor) {
    resect_string namespace = resect_string_from_clang(clang_getCursorSpelling(cursor));

    resect_push_namespace(context, namespace);
    clang_visitChildren(cursor, resect_visit_namespace_child, context);
    resect_pop_namespace(context);

    resect_string_free(namespace);
}
