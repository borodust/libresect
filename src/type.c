#include "../resect.h"
#include "resect_private.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <clang-c/Index.h>

#define CONST_QUALIFIER_LENGTH (6)
/*
 * TYPE
 */
struct P_resect_type_field {
    resect_type type;
    resect_string name;
    long long offset;
};

struct P_resect_type {
    resect_type_kind kind;
    resect_string name;
    unsigned int size;
    unsigned int alignment;
    resect_type_category category;
    resect_collection fields;
    resect_collection base_classes;
    resect_collection methods;
    resect_bool const_qualified;
    resect_bool pod;
    resect_bool undeclared;
    resect_collection template_arguments;

    resect_decl decl;

    resect_bool initialized;
    resect_data_deallocator data_deallocator;
    void *data;
};

typedef struct P_resect_type_method {
    resect_string name;
    resect_type type;
}* resect_type_method;

resect_type_method resect_method_create(resect_visit_context visit_context, resect_translation_context context,
                                   CXCursor cursor) {
    resect_type_method method = malloc(sizeof(struct P_resect_type_method));
    method->name = resect_string_from_clang(clang_getCursorSpelling(cursor));
    method->type = resect_type_create(visit_context, context, clang_getCursorType(cursor));
    return method;
}

void resect_method_free(resect_type_method method, resect_set deallocated) {
    resect_string_free(method->name);
    resect_type_free(method->type, deallocated);
    free(method);
}

void resect_method_collection_free(resect_type type, resect_set deallocated) {
    resect_iterator method_iter = resect_collection_iterator(type->methods);
    while (resect_iterator_next(method_iter)) {
        resect_type_method method = resect_iterator_value(method_iter);
        resect_method_free(method, deallocated);
    }
    resect_iterator_free(method_iter);
    resect_collection_free(type->methods);
}

void resect_type_free(resect_type type, resect_set deallocated) {
    if (!resect_set_add(deallocated, type)) {
        return;
    }

    if (type->data_deallocator) {
        type->data_deallocator(type->data, deallocated);
    }

    resect_string_free(type->name);
    resect_field_collection_free(type->fields, deallocated);
    resect_type_collection_free(type->base_classes, deallocated);
    resect_method_collection_free(type, deallocated);
    resect_template_argument_collection_free(type->template_arguments, deallocated);

    if (type->decl != NULL) {
        resect_decl_free(type->decl, deallocated);
    }

    free(type);
}

resect_type_field resect_field_create(resect_visit_context visit_context, resect_translation_context context, CXType parent,
                                 CXType clang_field, resect_string name) {
    resect_type_field field = malloc(sizeof(struct P_resect_type_field));
    field->type = resect_type_create(visit_context, context, clang_field);
    field->name = resect_string_copy(name);
    field->offset = clang_Type_getOffsetOf(parent, resect_string_to_c(name));
    return field;
}

const char *resect_type_field_get_name(resect_type_field field) { return resect_string_to_c(field->name); }

resect_type resect_type_field_get_type(resect_type_field field) { return field->type; }

long long resect_type_field_get_offset(resect_type_field field) { return field->offset; }

const char* resect_type_method_get_name(resect_type_method method) {
    return resect_string_to_c(method->name);
}

resect_type resect_type_method_get_type(resect_type_method method) {
    return method->type;
}

void resect_field_free(resect_type_field field, resect_set deallocated) {
    resect_type_free(field->type, deallocated);
    resect_string_free(field->name);
    free(field);
}

void resect_field_collection_free(resect_collection fields, resect_set deallocated) {
    resect_iterator iter = resect_collection_iterator(fields);
    while (resect_iterator_next(iter)) {
        resect_type_field field = resect_iterator_value(iter);
        resect_field_free(field, deallocated);
    }
    resect_iterator_free(iter);
    resect_collection_free(fields);
}

void resect_type_collection_free(resect_collection types, resect_set deallocated) {
    resect_iterator iter = resect_collection_iterator(types);
    while (resect_iterator_next(iter)) {
        resect_type_free(resect_iterator_value(iter), deallocated);
    }
    resect_iterator_free(iter);
    resect_collection_free(types);
}

resect_type_kind convert_type_kind(enum CXTypeKind kind) {
    switch (kind) {
        case CXType_Invalid:
        case CXType_Unexposed:
            return RESECT_TYPE_KIND_UNKNOWN;
        default:
            return (resect_type_kind) kind;
    }
}

typedef struct P_resect_type_visit_data {
    resect_type type;
    resect_visit_context visit_context;
    resect_translation_context context;
    CXType parent;
} *resect_type_visit_data;

static enum CXVisitorResult visit_type_field(CXCursor cursor, CXClientData data) {
    resect_type_visit_data visit_data = data;

    CXType clang_type = clang_getCursorType(cursor);

    resect_string name = resect_string_from_clang(clang_getCursorDisplayName(cursor));
    resect_type_field field =
            resect_field_create(visit_data->visit_context, visit_data->context, visit_data->parent, clang_type, name);
    resect_string_free(name);

    resect_collection_add(visit_data->type->fields, field);

    return CXVisit_Continue;
}

static enum CXVisitorResult visit_type_base_class(CXCursor cursor, CXClientData data) {
    resect_type_visit_data visit_data = data;

    resect_type class_type =
            resect_type_create(visit_data->visit_context, visit_data->context, clang_getCursorType(cursor));
    resect_collection_add(visit_data->type->base_classes, class_type);

    return CXVisit_Continue;
}

static enum CXVisitorResult visit_type_method(CXCursor cursor, CXClientData data) {
    resect_type_visit_data visit_data = data;

    resect_collection_add(visit_data->type->methods,
        resect_method_create(visit_data->visit_context, visit_data->context, cursor));

    return CXVisit_Continue;
}

/*
 * ARRAY
 */
typedef struct P_resect_array_data {
    resect_type type;
    long long size;
} *resect_array_data;

void resect_array_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }

    resect_array_data array_data = data;
    resect_type_free(array_data->type, deallocated);
    free(data);
}

void resect_array_init(resect_visit_context visit_context, resect_translation_context context, resect_type type,
                       CXType clangType) {
    resect_array_data data = malloc(sizeof(struct P_resect_array_data));
    data->type = resect_type_create(visit_context, context, clang_getArrayElementType(clangType));
    data->size = clang_getArraySize(clangType);

    type->data_deallocator = resect_array_data_free;
    type->data = data;
}

long long resect_array_get_size(resect_type type) {
    assert(type->category == RESECT_TYPE_CATEGORY_ARRAY);
    resect_array_data data = type->data;
    return data->size;
}

resect_type resect_array_get_element_type(resect_type type) {
    assert(type->category == RESECT_TYPE_CATEGORY_ARRAY);
    resect_array_data data = type->data;
    return data->type;
}

/*
 * POINTER
 */
typedef struct P_resect_pointer_data {
    resect_type type;
} *resect_pointer_data;

void resect_pointer_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_pointer_data pointer = data;
    resect_type_free(pointer->type, deallocated);
    free(data);
}

void resect_pointer_init(resect_visit_context visit_context, resect_translation_context context, resect_type type,
                         CXType clangType) {
    resect_pointer_data data = malloc(sizeof(struct P_resect_pointer_data));
    data->type = resect_type_create(visit_context, context, clang_getPointeeType(clangType));

    type->data_deallocator = resect_pointer_data_free;
    type->data = data;
}

resect_type resect_pointer_get_pointee_type(resect_type type) {
    assert(type->category == RESECT_TYPE_CATEGORY_POINTER);
    resect_pointer_data data = type->data;
    return data->type;
}


/*
 * REFERENCE
 */
typedef struct P_resect_reference_data {
    resect_bool is_lvalue;
    resect_type type;
} *resect_reference_data;

void resect_reference_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_reference_data pointer = data;
    resect_type_free(pointer->type, deallocated);
    free(data);
}

void resect_reference_init(resect_visit_context visit_context, resect_translation_context context, resect_type type,
                           CXType clangType) {
    resect_reference_data data = malloc(sizeof(struct P_resect_reference_data));

    data->type = resect_type_create(visit_context, context, clang_getPointeeType(clangType));
    data->is_lvalue = clangType.kind == CXType_LValueReference;

    type->data_deallocator = resect_reference_data_free;
    type->data = data;
}

resect_type resect_reference_get_pointee_type(resect_type type) {
    assert(type->category == RESECT_TYPE_CATEGORY_REFERENCE);
    resect_reference_data data = type->data;
    return data->type;
}

resect_bool resect_reference_is_lvalue(resect_type type) {
    assert(type->category == RESECT_TYPE_CATEGORY_REFERENCE);
    resect_reference_data data = type->data;
    return data->is_lvalue;
}

/*
 * FUNCTION PROTO
 */
typedef struct P_resect_function_proto_data {
    resect_type result_type;
    resect_bool variadic;
    resect_collection parameters;
} *resect_function_proto_data;

void resect_function_proto_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_function_proto_data function_proto = data;
    resect_type_free(function_proto->result_type, deallocated);
    resect_type_collection_free(function_proto->parameters, deallocated);

    free(data);
}

void resect_function_proto_init(resect_visit_context visit_context, resect_translation_context context,
                                resect_type type, CXType clangType) {
    resect_function_proto_data data = malloc(sizeof(struct P_resect_function_proto_data));
    data->result_type = resect_type_create(visit_context, context, clang_getResultType(clangType));
    data->variadic = convert_bool_from_uint(clang_isFunctionTypeVariadic(clangType));
    data->parameters = resect_collection_create();

    int arg_count = clang_getNumArgTypes(clangType);
    for (int i = 0; i < arg_count; ++i) {
        CXType arg_type = clang_getArgType(clangType, i);
        resect_collection_add(data->parameters, resect_type_create(visit_context, context, arg_type));
    }

    type->data_deallocator = resect_function_proto_free;
    type->data = data;
}

resect_type resect_function_proto_get_result_type(resect_type type) {
    assert(type->kind == RESECT_TYPE_KIND_FUNCTIONPROTO);

    resect_function_proto_data data = type->data;
    return data->result_type;
}

resect_bool resect_function_proto_is_variadic(resect_type type) {
    assert(type->kind == RESECT_TYPE_KIND_FUNCTIONPROTO);

    resect_function_proto_data data = type->data;
    return data->variadic;
}

resect_collection resect_function_proto_parameters(resect_type type) {
    assert(type->kind == RESECT_TYPE_KIND_FUNCTIONPROTO);

    resect_function_proto_data data = type->data;
    return data->parameters;
}

/*
 * TEMPLATE ARGUMENT
 */
void resect_init_template_args_from_type(resect_visit_context visit_context, resect_translation_context context,
                                         resect_collection args, CXType type) {
    int arg_count = clang_Type_getNumTemplateArguments(type);

    for (int i = 0; i < arg_count; ++i) {
        CXType clang_arg_type = clang_Type_getTemplateArgumentAsType(type, i);

        resect_template_argument_kind arg_kind = RESECT_TEMPLATE_ARGUMENT_KIND_UNKNOWN;
        resect_type arg_type = NULL;
        long long int arg_value = 0;

        switch (clang_arg_type.kind) {
            case CXType_Invalid:
            case CXType_Unexposed:
                arg_kind = RESECT_TEMPLATE_ARGUMENT_KIND_INTEGRAL;
                arg_value = -1;
                break;
            default:
                arg_kind = RESECT_TEMPLATE_ARGUMENT_KIND_TYPE;
                arg_type = resect_type_create(visit_context, context, clang_arg_type);
                break;
        }

        resect_collection_add(args, resect_template_argument_create(arg_kind, arg_type, arg_value, i));
    }
}

static resect_string resect_string_fqn_from_type(resect_translation_context context, CXType clang_type) {
    return resect_string_from_clang(clang_getFullyQualifiedName(clang_type,
                                                                resect_context_get_printing_policy(
                                                                    context),
                                                                0));
}

static resect_string resect_extract_type_id(resect_translation_context context, CXType clang_type) {
    CXCursor cursor = clang_getTypeDeclaration(clang_type);
    resect_string type_id;
    if (!clang_Cursor_isNull(cursor) && clang_getCursorKind(cursor) != CXCursor_NoDeclFound) {
        type_id = resect_extract_decl_id(cursor);
        resect_string_append_c(type_id, "+");
    } else {
        type_id = resect_string_from_c("");
    }

    resect_string fqn = resect_string_fqn_from_type(context, clang_type);
    resect_string_append(type_id, fqn);
    resect_string_free(fqn);

    return type_id;
}

resect_type_category get_type_category(resect_type_kind kind) {
    switch (kind) {
        case RESECT_TYPE_KIND_BOOL:
        case RESECT_TYPE_KIND_CHAR_U:
        case RESECT_TYPE_KIND_UCHAR:
        case RESECT_TYPE_KIND_CHAR16:
        case RESECT_TYPE_KIND_CHAR32:
        case RESECT_TYPE_KIND_USHORT:
        case RESECT_TYPE_KIND_UINT:
        case RESECT_TYPE_KIND_ULONG:
        case RESECT_TYPE_KIND_ULONGLONG:
        case RESECT_TYPE_KIND_UINT128:
        case RESECT_TYPE_KIND_CHAR_S:
        case RESECT_TYPE_KIND_SCHAR:
        case RESECT_TYPE_KIND_WCHAR:
        case RESECT_TYPE_KIND_SHORT:
        case RESECT_TYPE_KIND_INT:
        case RESECT_TYPE_KIND_LONG:
        case RESECT_TYPE_KIND_LONGLONG:
        case RESECT_TYPE_KIND_INT128:
        case RESECT_TYPE_KIND_FLOAT:
        case RESECT_TYPE_KIND_DOUBLE:
        case RESECT_TYPE_KIND_LONGDOUBLE:
        case RESECT_TYPE_KIND_FLOAT128:
        case RESECT_TYPE_KIND_HALF:
        case RESECT_TYPE_KIND_FLOAT16:
        case RESECT_TYPE_KIND_COMPLEX:
            return RESECT_TYPE_CATEGORY_ARITHMETIC;
        case RESECT_TYPE_KIND_VOID:
        case RESECT_TYPE_KIND_NULLPTR:
        case RESECT_TYPE_KIND_OVERLOAD:
        case RESECT_TYPE_KIND_DEPENDENT:
        case RESECT_TYPE_KIND_AUTO:
        case RESECT_TYPE_KIND_ATOMIC:
            return RESECT_TYPE_CATEGORY_AUX;
        case RESECT_TYPE_KIND_POINTER:
        case RESECT_TYPE_KIND_MEMBERPOINTER:
        case RESECT_TYPE_KIND_BLOCKPOINTER:
            return RESECT_TYPE_CATEGORY_POINTER;
        case RESECT_TYPE_KIND_LVALUEREFERENCE:
        case RESECT_TYPE_KIND_RVALUEREFERENCE:
            return RESECT_TYPE_CATEGORY_REFERENCE;
        case RESECT_TYPE_KIND_RECORD:
        case RESECT_TYPE_KIND_ENUM:
        case RESECT_TYPE_KIND_TYPEDEF:
        case RESECT_TYPE_KIND_FUNCTIONNOPROTO:
            return RESECT_TYPE_CATEGORY_UNIQUE;
        case RESECT_TYPE_KIND_CONSTANTARRAY:
        case RESECT_TYPE_KIND_VECTOR:
        case RESECT_TYPE_KIND_INCOMPLETEARRAY:
        case RESECT_TYPE_KIND_VARIABLEARRAY:
        case RESECT_TYPE_KIND_DEPENDENTSIZEDARRAY:
        case RESECT_TYPE_KIND_EXTVECTOR:
            return RESECT_TYPE_CATEGORY_ARRAY;
        default:
            return RESECT_TYPE_CATEGORY_UNKNOWN;
    }
}

/*
 * TYPE CONSTRUCTOR
 */
resect_type resect_type_create(resect_visit_context visit_context, resect_translation_context context,
                               CXType clang_type) {
    switch (clang_type.kind) {
        case CXType_Elaborated:
            return resect_type_create(visit_context, context, clang_Type_getNamedType(clang_type));
        case CXType_Unexposed: {
            CXType canonical_type = clang_getCanonicalType(clang_type);
            if (CXType_Unexposed != canonical_type.kind) {
                return resect_type_create(visit_context, context, canonical_type);
            }
        }
        break;
        case CXType_Attributed:
            // skip attributes
            return resect_type_create(visit_context, context, clang_Type_getModifiedType(clang_type));
        default: ;
    }

    resect_string type_id = resect_extract_type_id(context, clang_type);
    resect_type type = resect_find_type(context, type_id);
    if (type != NULL) {
        goto done;
    }

    type = malloc(sizeof(struct P_resect_type));
    type->initialized = false;
    type->kind = convert_type_kind(clang_type.kind);
    type->category = get_type_category(type->kind);
    type->name = resect_string_fqn_from_type(context, clang_type);

    long long int size = clang_Type_getSizeOf(clang_type);
    if (size <= 0) {
        // most likely forward declaration
        type->size = 0;
        type->alignment = 0;
    } else {
        type->size = 8 * size;
        type->alignment = 8 * filter_valid_value(clang_Type_getAlignOf(clang_type));
    }
    type->fields = resect_collection_create();
    type->base_classes = resect_collection_create();
    type->methods = resect_collection_create();
    type->const_qualified = convert_bool_from_uint(clang_isConstQualifiedType(clang_type));
    type->pod = convert_bool_from_uint(clang_isPODType(clang_type));
    type->template_arguments = resect_collection_create();
    type->decl = NULL;

    type->data_deallocator = NULL;
    type->data = NULL;

    resect_register_type(context, type_id, type);

    CXCursor declaration_cursor = clang_getTypeDeclaration(clang_type);
    if (declaration_cursor.kind == CXCursor_NoDeclFound) {
        type->undeclared = resect_true;
        type->decl = NULL;
    } else {
        type->undeclared = resect_false;
        type->decl = resect_decl_create(visit_context, context, declaration_cursor).decl;
    }

    if (type->kind == RESECT_TYPE_KIND_UNKNOWN && type->decl == NULL) {
        resect_decl param = NULL;
        if (clang_isConstQualifiedType(clang_type) && strlen(resect_string_to_c(type->name)) > CONST_QUALIFIER_LENGTH) {
            resect_string unqualified_name = resect_substring(type->name, CONST_QUALIFIER_LENGTH, -1);
            param = resect_find_template_parameter(context, unqualified_name);
            resect_string_free(unqualified_name);
        } else {
            param = resect_find_template_parameter(context, type->name);
        }
        if (param != NULL) {
            type->kind = RESECT_TYPE_KIND_TEMPLATE_PARAMETER;
            type->category = RESECT_TYPE_CATEGORY_UNIQUE;
            type->decl = param;
        }
    }

    switch (type->kind) {
        case RESECT_TYPE_KIND_FUNCTIONPROTO:
            resect_function_proto_init(visit_context, context, type, clang_type);
            break;
        default:
            switch (type->category) {
                case RESECT_TYPE_CATEGORY_POINTER:
                    resect_pointer_init(visit_context, context, type, clang_type);
                    break;
                case RESECT_TYPE_CATEGORY_REFERENCE:
                    resect_reference_init(visit_context, context, type, clang_type);
                    break;
                case RESECT_TYPE_CATEGORY_ARRAY:
                    resect_array_init(visit_context, context, type, clang_type);
                    break;
                default:
                    break;
            }
    }

    type->initialized = true;

    if (type->decl != NULL) {
        resect_init_template_args_from_type(visit_context, context, type->template_arguments, clang_type);
        resect_decl root_template = resect_decl_get_root_template(type->decl);
        if (root_template != NULL) {
            resect_decl_register_specialization(root_template, type);
        }

        struct P_resect_type_visit_data visit_data = {
            .type = type, .visit_context = visit_context, .context = context, .parent = clang_type
        };

        clang_Type_visitFields(clang_type, visit_type_field, &visit_data);
        clang_visitCXXBaseClasses(clang_type, visit_type_base_class, &visit_data);
        clang_visitCXXMethods(clang_type, visit_type_method, &visit_data);
    }

done:
    resect_string_free(type_id);
    return type;
}

resect_bool resect_type_is_undeclared(resect_type type) { return type->undeclared; }

resect_type_kind resect_type_get_kind(resect_type type) { return type->kind; }

const char *resect_type_get_name(resect_type type) { return resect_string_to_c(type->name); }

long long resect_type_sizeof(resect_type type) { return type->size; }

long long resect_type_alignof(resect_type type) { return type->alignment; }

long long resect_type_offsetof(resect_type type, const char *field_name) {
    resect_iterator iterator = resect_collection_iterator(type->fields);
    long long result = -1;
    while (resect_iterator_next(iterator)) {
        resect_type_field field = resect_iterator_value(iterator);
        if (resect_string_equal_c(field->name, field_name)) {
            result = field->offset;
            break;
        }
    }
    resect_iterator_free(iterator);
    return result;
}

resect_collection resect_type_fields(resect_type type) { return type->fields; }

resect_collection resect_type_base_classes(resect_type type) { return type->base_classes; }

resect_collection resect_type_methods(resect_type type) { return type->methods; }

resect_bool resect_type_is_const_qualified(resect_type type) { return type->const_qualified; }

resect_bool resect_type_is_pod(resect_type type) { return type->pod; }

resect_decl resect_type_get_declaration(resect_type type) { return type->decl; }

resect_type_category resect_type_get_category(resect_type type) { return type->category; }

resect_collection resect_type_template_arguments(resect_type type) { return type->template_arguments; }

resect_string resect_type_pretty_print(resect_translation_context context, CXType type) {
    return resect_string_from_clang(clang_getTypePrettyPrinted(type,
                                                               resect_context_get_printing_policy(context)));
}
