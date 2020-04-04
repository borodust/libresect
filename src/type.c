#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <clang-c/Index.h>

#include "uthash.h"

/*
 * TYPE
 */
struct resect_type {
    resect_type_kind kind;
    resect_string name;
    unsigned int size;
    unsigned int alignment;
    resect_type_category category;
    resect_collection fields;

    resect_decl decl;

    data_deallocator data_deallocator;
    void *data;
};

void resect_type_free(resect_type type, resect_set deallocated) {
    if (!resect_set_add(deallocated, type)) {
        return;
    }

    if (type->data_deallocator) {
        type->data_deallocator(type->data, deallocated);
    }

    resect_string_free(type->name);
    resect_decl_collection_free(type->fields, deallocated);
    resect_decl_free(type->decl, deallocated);

    free(type);
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

enum CXVisitorResult visit_type_fields(CXCursor cursor, CXClientData data) {
    resect_type type = data;
    resect_decl field_decl = resect_parse_decl(resect_decl_get_context(type->decl), cursor);
    resect_collection_add(type->fields, field_decl);
    return CXVisit_Continue;
}

/*
 * ARRAY
 */
typedef struct resect_array_data {
    resect_type type;
    long long size;
} *resect_array_data;

void resect_array_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    free(data);
}

void resect_array_init(resect_type type, CXType clangType) {
    resect_array_data data = malloc(sizeof(struct resect_array_data));
    data->type = resect_type_create(resect_decl_get_context(type->decl), clang_getArrayElementType(clangType));
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
typedef struct resect_pointer_data {
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

void resect_pointer_init(resect_type type, CXType clangType) {
    resect_pointer_data data = malloc(sizeof(struct resect_pointer_data));
    data->type = resect_type_create(resect_decl_get_context(type->decl), clang_getPointeeType(clangType));

    type->data_deallocator = resect_pointer_data_free;
    type->data = data;
}

RESECT_API resect_type resect_pointer_get_pointee_type(resect_type type) {
    assert(type->category == RESECT_TYPE_CATEGORY_POINTER);
    resect_pointer_data data = type->data;
    return data->type;
}

resect_type resect_type_create(resect_translation_context context, CXType clangType) {
    resect_type type = malloc(sizeof(struct resect_type));

    type->kind = convert_type_kind(clangType.kind);
    type->name = resect_string_from_clang(clang_getTypeSpelling(clangType));
    type->size = filter_valid_value(clang_Type_getSizeOf(clangType));
    type->alignment = filter_valid_value(clang_Type_getAlignOf(clangType));
    type->fields = resect_collection_create();
    type->data_deallocator = NULL;
    type->data = NULL;

    type->decl = resect_parse_decl(context, clang_getTypeDeclaration(clangType));
    clang_Type_visitFields(clangType, visit_type_fields, type);

    switch (type->kind) {
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
            type->category = RESECT_TYPE_CATEGORY_ARITHMETIC;
            break;
        case RESECT_TYPE_KIND_VOID:
        case RESECT_TYPE_KIND_NULLPTR:
        case RESECT_TYPE_KIND_OVERLOAD:
        case RESECT_TYPE_KIND_DEPENDENT:
        case RESECT_TYPE_KIND_AUTO:
        case RESECT_TYPE_KIND_ATTRIBUTED:
        case RESECT_TYPE_KIND_ELABORATED:
            type->category = RESECT_TYPE_CATEGORY_AUX;
            break;
        case RESECT_TYPE_KIND_POINTER:
        case RESECT_TYPE_KIND_MEMBERPOINTER:
        case RESECT_TYPE_KIND_BLOCKPOINTER:
            type->category = RESECT_TYPE_CATEGORY_POINTER;
            resect_pointer_init(type, clangType);
            break;
        case RESECT_TYPE_KIND_LVALUEREFERENCE:
        case RESECT_TYPE_KIND_RVALUEREFERENCE:
            type->category = RESECT_TYPE_CATEGORY_REFERENCE;
            break;
        case RESECT_TYPE_KIND_RECORD:
        case RESECT_TYPE_KIND_ENUM:
        case RESECT_TYPE_KIND_TYPEDEF:
        case RESECT_TYPE_KIND_FUNCTIONNOPROTO:
        case RESECT_TYPE_KIND_FUNCTIONPROTO:
            type->category = RESECT_TYPE_CATEGORY_UNIQUE;
            break;
        case RESECT_TYPE_KIND_CONSTANTARRAY:
        case RESECT_TYPE_KIND_VECTOR:
        case RESECT_TYPE_KIND_INCOMPLETEARRAY:
        case RESECT_TYPE_KIND_VARIABLEARRAY:
        case RESECT_TYPE_KIND_DEPENDENTSIZEDARRAY:
        case RESECT_TYPE_KIND_EXTVECTOR:
            type->category = RESECT_TYPE_CATEGORY_ARRAY;
            resect_array_init(type, clangType);
            break;
        default:
            type->category = RESECT_TYPE_CATEGORY_UNKNOWN;
    }

    return type;
}


resect_type_kind resect_type_get_kind(resect_type type) {
    return type->kind;
}

const char *resect_type_get_name(resect_type type) {
    return resect_string_to_c(type->name);
}

long long resect_type_sizeof(resect_type type) {
    return type->size;
}

long long resect_type_alignof(resect_type type) {
    return type->alignment;
}

long long resect_type_offsetof(resect_type type, const char *field_name) {
    resect_iterator iterator = resect_collection_iterator(type->fields);

    long long result = -1;
    while (resect_iterator_next(iterator)) {
        resect_decl field_decl = resect_iterator_value(iterator);
        if (strcmp(resect_decl_get_name(field_decl), field_name) == 0) {
            result = resect_field_get_offset(field_decl);
            break;
        }
    }
    resect_iterator_free(iterator);
    return result;
}

resect_collection resect_type_fields(resect_type type) {
    return type->fields;
}

resect_decl resect_type_get_declaration(resect_type type) {
    return type->decl;
}

resect_type_category resect_type_get_category(resect_type type) {
    return type->category;
}