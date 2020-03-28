#include "resect.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <clang-c/Index.h>

#include "uthash.h"

resect_decl resect_parse_decl(resect_translation_context context, CXCursor cursor);

void resect_register_decl(resect_translation_context context, const char *id, resect_decl decl);

resect_decl resect_find_decl(resect_translation_context context, const char *id);


/*
 * TABLE
 */
struct resect_decl_table {
    const char *key;
    resect_decl value;

    UT_hash_handle hh;
};

/*
 * PARSE RESULT
 */
struct resect_parse_result {
    resect_translation_context context;
};

/*
 * SET
 */

typedef struct resect_set_item {
    void *key;

    UT_hash_handle hh;
} *resect_set_item;

typedef struct resect_set {
    resect_set_item head;
} *resect_set;

resect_set resect_set_create() {
    resect_set set = malloc(sizeof(struct resect_set));
    set->head = NULL;
    return set;
}

resect_bool resect_set_contains(resect_set set, void *value) {
    struct resect_set_item *entry;
    HASH_FIND_PTR(set->head, value, entry);
    return entry != NULL ? resect_true : resect_false;
}

resect_bool resect_set_add(resect_set set, void *value) {
    if (!resect_set_contains(set, value)) {
        struct resect_set_item *entry = malloc(sizeof(struct resect_set_item));
        entry->key = value;
        HASH_ADD_PTR(set->head, key, entry);
        return resect_true;
    }
    return resect_false;
}

void resect_set_free(resect_set set) {
    struct resect_set_item *element, *tmp;
    HASH_ITER(hh, set->head, element, tmp) {
        HASH_DEL(set->head, element);
        free(element);
    }
    free(set);
}

/*
 * STRING
 */
typedef struct resect_string {
    char *value;
    unsigned int capacity;
} *resect_string;

resect_string resect_string_create(unsigned int initial_capacity) {
    resect_string result = malloc(sizeof(struct resect_string));
    result->capacity = initial_capacity > 0 ? initial_capacity : 1;
    result->value = malloc(result->capacity * sizeof(char));
    result->value[0] = 0;
    return result;
}

void resect_string_free(resect_string string) {
    free(string->value);
    free(string);
}

const char *resect_string_to_c(resect_string string) {
    assert(string != NULL);
    return string->value;
}

void resect_ensure_string_capacity(resect_string string, unsigned long new_capacity) {
    if (string->capacity < new_capacity) {
        free(string->value);

        string->capacity = new_capacity;

        char *value = malloc(sizeof(char) * string->capacity);
        assert(value);
        string->value = value;
    }
    string->value[string->capacity - 1] = 0;
}

resect_string resect_update_string(resect_string string, const char *new_value) {
    assert(string != NULL);
    const char *ensured_new_value = new_value == NULL ? "" : new_value;
    size_t new_string_size = strlen(ensured_new_value);

    resect_ensure_string_capacity(string, new_string_size + 1);

    memcpy(string->value, ensured_new_value, sizeof(char) * new_string_size);
    string->value[new_string_size] = 0;
    return string;
}

char *_resect_prepare_c_string(resect_string string, int length) {
    assert(string != NULL);
    resect_ensure_string_capacity(string, length + 1);

    string->value[length] = 0;
    return string->value;
}

resect_string resect_ensure_string(resect_string string) {
    return string == NULL ? resect_string_create(0) : string;
}


resect_string resect_ensure_string_default_length(resect_string string, int length) {
    return string == NULL ? resect_string_create(length + 1) : string;
}

resect_string resect_ensure_string_default_value(resect_string string, const char *default_value) {
    if (string == NULL) {
        resect_string result;
        result = resect_string_create(0);
        resect_update_string(result, default_value);
        return result;
    } else {
        return string;
    }
}

resect_string resect_string_from_c(const char *string) {
    return resect_ensure_string_default_value(NULL, string);
}

resect_string resect_ensure_string_from_clang(resect_string provided, CXString from) {
    resect_string result = resect_ensure_string(provided);
    resect_update_string(result, clang_getCString(from));
    clang_disposeString(from);
    return result;
}


resect_string resect_string_from_clang(CXString from) {
    return resect_ensure_string_from_clang(NULL, from);
}

resect_string resect_string_format(const char *format, ...) {
    if (format == NULL) {
        return resect_ensure_string(NULL);
    }

    va_list args;
    int len = 0;
    va_start(args, format);
    len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    resect_string result = resect_ensure_string_default_length(NULL, len);
    va_start(args, format);
    vsnprintf(result->value, len, format, args);
    va_end(args);

    return result;
}

/*
 * COLLECTION
 */
struct resect_collection_element {
    void *value;
    struct resect_collection_element *next;
};

struct resect_collection {
    unsigned int size;
    struct resect_collection_element *head, *tail;
};

resect_collection resect_collection_create() {
    resect_collection collection = malloc(sizeof(struct resect_collection));
    collection->head = NULL;
    collection->tail = NULL;
    collection->size = 0;
    return collection;
}

void resect_collection_free(resect_collection collection) {
    struct resect_collection_element *el, *next;
    el = collection->head;
    while (el) {
        next = el->next;
        free(el);
        el = next;
    }
    free(collection);
}

void resect_collection_add(resect_collection collection, void *value) {
    struct resect_collection_element *element = malloc(sizeof(struct resect_collection_element));
    element->value = value;
    element->next = NULL;

    if (collection->head == NULL) {
        collection->head = element;
        collection->tail = element;
    } else {
        collection->tail->next = element;
        collection->tail = element;
    }
    collection->size += 1;
}

unsigned int resect_collection_size(resect_collection collection) {
    return collection->size;
}

/*
 * ITERATOR
 */
struct resect_iterator {
    struct resect_collection_element *head;
    struct resect_collection_element *current;
};

resect_iterator resect_collection_iterator(resect_collection collection) {
    resect_iterator iterator = malloc(sizeof(struct resect_iterator));
    iterator->head = collection->head;
    iterator->current = NULL;
    return iterator;
}

resect_bool resect_iterator_next(resect_iterator iter) {
    if (iter->head == NULL || (iter->current != NULL && iter->current->next == NULL)) {
        return resect_false;
    }

    if (iter->current == NULL) {
        iter->current = iter->head;
    } else {
        iter->current = iter->current->next;
    }

    return resect_true;
}


void *resect_iterator_value(resect_iterator iter) {
    assert(iter->current != NULL);

    return iter->current->value;
}

void resect_iterator_free(resect_iterator iter) {
    free(iter);
}

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
resect_type resect_type_create(resect_translation_context context, CXType clangType);

typedef void (*data_deallocator)(void *data, resect_set deallocated);

struct resect_decl {
    resect_translation_context context;

    resect_decl_kind kind;
    resect_string id;
    resect_string name;
    resect_location location;
    resect_string comment;

    resect_type type;

    void *data;
    data_deallocator data_deallocator;
};

resect_decl_kind convert_cursor_kind(enum CXCursorKind kind) {
    switch (kind) {
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
        case CXCursor_FunctionDecl:
            return RESECT_DECL_KIND_FUNCTION;
        case CXCursor_VarDecl:
            return RESECT_DECL_KIND_VARIABLE;
        case CXCursor_ParmDecl:
            return RESECT_DECL_KIND_PARAMETER;
        case CXCursor_TypedefDecl:
            return RESECT_DECL_KIND_TYPEDEF;
        default:
            return RESECT_DECL_KIND_UNKNOWN;
    }
}

void resect_decl_init_from_cursor(resect_decl decl, resect_translation_context context, CXCursor cursor) {
    decl->kind = convert_cursor_kind(clang_getCursorKind(cursor));
    decl->id = resect_string_from_clang(clang_getCursorUSR(cursor));
    decl->name = resect_string_from_clang(clang_getCursorSpelling(cursor));
    decl->comment = resect_string_from_clang(clang_Cursor_getRawCommentText(cursor));
    decl->location = resect_location_from_cursor(cursor);
    decl->context = context;
    decl->data_deallocator = NULL;
    decl->data = NULL;

    decl->type = resect_type_create(context, clang_getCursorType(cursor));
}


void resect_struct_init(resect_decl decl, CXCursor cursor);

void resect_union_init(resect_decl decl, CXCursor cursor);

void resect_enum_init(resect_decl decl, CXCursor cursor);

void resect_enum_constant_init(resect_decl decl, CXCursor cursor);

void resect_function_init(resect_decl decl, CXCursor cursor);

void resect_variable_init(resect_decl decl, CXCursor cursor);

void resect_typedef_init(resect_decl decl, CXCursor cursor);

void resect_field_init(resect_decl decl, CXCursor cursor);

void resect_type_free(resect_type type, resect_set deallocated);

resect_decl resect_parse_decl(resect_translation_context context, CXCursor cursor) {
    resect_string decl_id = resect_string_from_clang(clang_getCursorUSR(cursor));
    resect_decl registered_decl = resect_find_decl(context, resect_string_to_c(decl_id));
    if (registered_decl != NULL) {
        resect_string_free(decl_id);
        return registered_decl;
    }

    resect_decl decl = malloc(sizeof(struct resect_decl));
    memset(decl, 0, sizeof(struct resect_decl));

    resect_register_decl(context, resect_string_to_c(decl_id), decl);
    resect_string_free(decl_id);

    resect_decl_init_from_cursor(decl, context, cursor);

    switch (decl->kind) {
        case RESECT_DECL_KIND_STRUCT:
            resect_struct_init(decl, cursor);
            break;
        case RESECT_DECL_KIND_UNION:
            resect_union_init(decl, cursor);
            break;
        case RESECT_DECL_KIND_ENUM:
            resect_enum_init(decl, cursor);
            break;
        case RESECT_DECL_KIND_ENUM_CONSTANT:
            resect_enum_constant_init(decl, cursor);
            break;
        case RESECT_DECL_KIND_FUNCTION:
            resect_function_init(decl, cursor);
            break;
        case RESECT_DECL_KIND_VARIABLE:
            resect_variable_init(decl, cursor);
            break;
        case RESECT_DECL_KIND_TYPEDEF:
            resect_typedef_init(decl, cursor);
            break;
        case RESECT_DECL_KIND_FIELD:
            resect_field_init(decl, cursor);
            break;
    }

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
    resect_type_free(decl->type, deallocated);

    free(decl);
}

resect_decl_kind resect_decl_get_kind(resect_decl decl) {
    return decl->kind;
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

const char *resect_decl_get_comment(resect_decl decl) {
    return resect_string_to_c(decl->comment);
}

resect_type resect_decl_get_type(resect_decl decl) {
    return decl->type;
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
 * TYPE
 */
typedef struct resect_field_data {
    resect_bool bitfield;
    long long width;
    long long offset;
} *resect_field_data;

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

    resect_string_free(type->name);
    resect_iterator iter = resect_collection_iterator(type->fields);
    while (resect_iterator_next(iter)) {
        free(resect_iterator_value(iter));
    }
    resect_iterator_free(iter);
    resect_collection_free(type->fields);

    resect_decl_free(type->decl, deallocated);

    if (type->data_deallocator) {
        type->data_deallocator(type->data, deallocated);
    }
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

long long filter_valid_value(long long value) {
    return value < 0 ? 0 : value;
}

enum CXVisitorResult visit_type_fields(CXCursor cursor, CXClientData data) {
    resect_type type = data;
    resect_decl field_decl = resect_parse_decl(type->decl->context, cursor);
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
    data->type = resect_type_create(type->decl->context, clang_getArrayElementType(clangType));
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
    data->type = resect_type_create(type->decl->context, clang_getPointeeType(clangType));

    type->data_deallocator = resect_pointer_data_free;
    type->data = data;
}

RESECT_API resect_type resect_pointer_get_pointee_type(resect_type type) {
    assert(type->category == RESECT_TYPE_CATEGORY_POINTER);
    resect_pointer_data data = type->data;
    return data->type;
}

/*
 * TYPE
 */
resect_type resect_type_create(resect_translation_context context, CXType clangType) {
    resect_type type = malloc(sizeof(struct resect_type));

    type->kind = convert_type_kind(clangType.kind);
    type->name = resect_string_from_clang(clang_getTypeSpelling(clangType));
    type->size = filter_valid_value(clang_Type_getSizeOf(clangType));
    type->alignment = filter_valid_value(clang_Type_getAlignOf(clangType));
    type->fields = resect_collection_create();
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
        if (strcmp(resect_string_to_c(field_decl->name), field_name) == 0) {
            resect_field_data field_data = field_decl->data;
            result = field_data->offset;
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

/*
 * STRUCT
 */
typedef struct resect_record_data {
    resect_collection fields;
} *resect_record_data;

resect_collection resect_struct_fields(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_STRUCT);
    return ((resect_record_data) decl->data)->fields;
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

void resect_field_init(resect_decl decl, CXCursor cursor) {
    resect_field_data data = malloc(sizeof(struct resect_field_data));

    data->offset = filter_valid_value(clang_Cursor_getOffsetOfField(cursor));
    data->bitfield = clang_Cursor_isBitField(cursor) > 0 ? resect_true : resect_false;
    data->width = clang_getFieldDeclBitWidth(cursor);

    decl->data_deallocator = resect_field_data_free;
    decl->data = data;
}

enum CXChildVisitResult resect_visit_fields(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl record_decl = data;

    resect_decl field_decl = resect_parse_decl(record_decl->context, cursor);
    if (field_decl->kind == RESECT_DECL_KIND_FIELD) {
        resect_record_data record_data = record_decl->data;
        resect_collection_add(record_data->fields, field_decl);
    }

    return CXChildVisit_Continue;
}

void resect_record_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    resect_record_data record = data;

    resect_decl_collection_free(record->fields, deallocated);

    free(data);
}

void resect_struct_init(resect_decl decl, CXCursor cursor) {
    resect_record_data record_data = malloc(sizeof(struct resect_record_data));
    record_data->fields = resect_collection_create();

    decl->data_deallocator = resect_record_data_free;
    decl->data = record_data;

    clang_visitChildren(cursor, resect_visit_fields, decl);
}


/*
 * UNION
 */
resect_collection resect_union_fields(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_UNION);
    return ((resect_record_data) decl->data)->fields;
}

void resect_union_init(resect_decl decl, CXCursor cursor) {
    resect_record_data record_data = malloc(sizeof(struct resect_record_data));
    record_data->fields = resect_collection_create();

    decl->data_deallocator = resect_record_data_free;
    decl->data = record_data;

    clang_visitChildren(cursor, resect_visit_fields, decl);
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

void resect_typedef_init(resect_decl decl, CXCursor cursor) {
    resect_typedef_data data = malloc(sizeof(struct resect_typedef_data));

    data->aliased_type = resect_type_create(decl->context, clang_getTypedefDeclUnderlyingType(cursor));

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
} *resect_function_data;

resect_type resect_function_get_return_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    return decl->type;
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

enum CXChildVisitResult resect_visit_parameters(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl parent_decl = data;
    resect_function_data function_data = parent_decl->data;

    resect_decl decl = resect_parse_decl(parent_decl->context, cursor);
    if (decl->kind == RESECT_DECL_KIND_PARAMETER) {
        resect_collection_add(function_data->parameters, decl);
    }

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
    free(function);
}

void resect_function_init(resect_decl decl, CXCursor cursor) {
    CXType functionType = clang_getCursorType(cursor);

    resect_function_data data = malloc(sizeof(struct resect_function_data));
    data->parameters = resect_collection_create();
    data->storage_class = convert_storage_class(clang_Cursor_getStorageClass(cursor));
    data->calling_convention = convert_calling_convention(clang_getFunctionTypeCallingConv(functionType));
    data->variadic = clang_isFunctionTypeVariadic(functionType) > 0 ? resect_true : resect_false;

    decl->data_deallocator = resect_function_data_free;
    decl->data = data;

    clang_visitChildren(cursor, resect_visit_parameters, decl);
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

void resect_enum_constant_init(resect_decl decl, CXCursor cursor) {
    resect_enum_constant_data data = malloc(sizeof(resect_enum_constant_data));
    data->value = clang_getEnumConstantDeclValue(cursor);
    decl->data_deallocator = resect_enum_constant_free;
    decl->data = data;
}

typedef struct resect_enum_data {
    resect_collection constants;
} *resect_enum_data;


resect_collection resect_enum_constants(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_ENUM);
    resect_enum_data data = decl->data;

    return data->constants;
}


enum CXChildVisitResult resect_visit_enum_constants(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl parent_decl = data;
    resect_enum_data enum_data = parent_decl->data;

    resect_decl decl = resect_parse_decl(parent_decl->context, cursor);
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
    resect_decl_collection_free(enum_data->constants, deallocated);
    free(enum_data);
}

void resect_enum_init(resect_decl decl, CXCursor cursor) {
    resect_enum_data data = malloc(sizeof(struct resect_enum_data));
    data->constants = resect_collection_create();

    decl->data_deallocator = resect_enum_data_free;
    decl->data = data;
    clang_visitChildren(cursor, resect_visit_enum_constants, decl);
}

/*
 * VARIABLE
 */
void resect_variable_init(resect_decl decl, CXCursor cursor) {
}

/*
 * TRANSLATION CONTEXT
 */
struct resect_translation_context {
    resect_collection declarations;
    struct resect_decl_table *decl_table;
};

resect_translation_context resect_context_create() {
    resect_translation_context context = malloc(sizeof(struct resect_translation_context));
    context->declarations = resect_collection_create();
    context->decl_table = NULL;

    return context;
}

void resect_context_free(resect_translation_context context, resect_set deallocated) {
    if (!resect_set_add(deallocated, context)) {
        return;
    }

    struct resect_decl_table *element, *tmp;
    HASH_ITER(hh, context->decl_table, element, tmp) {
        HASH_DEL(context->decl_table, element);
        resect_decl_free(element->value, deallocated);
        free(element);
    }
    resect_collection_free(context->declarations);
    free(context);
}

resect_collection resect_context_declarations(resect_translation_context decl) {
    return decl->declarations;
}


void resect_register_decl(resect_translation_context context, const char *id, resect_decl decl) {
    struct resect_decl_table *entry = malloc(sizeof(struct resect_decl_table));
    entry->key = id;
    entry->value = decl;
    HASH_ADD_STR(context->decl_table, key, entry);
}


resect_decl resect_find_decl(resect_translation_context context, const char *id) {
    struct resect_decl_table *entry = NULL;
    HASH_FIND_STR(context->decl_table, id, entry);
    if (entry == NULL) {
        return NULL;
    }
    return entry->value;
}

enum CXChildVisitResult resect_visit_context_declarations(CXCursor cursor,
                                                          CXCursor parent,
                                                          CXClientData data) {
    resect_translation_context context = data;
    resect_collection_add(context->declarations, resect_parse_decl(context, cursor));
    return CXChildVisit_Continue;
}

/*
 * PARSER
 */
struct resect_parse_options {
    resect_collection args;
};

resect_parse_options resect_options_create() {
    resect_parse_options opts = malloc(sizeof(struct resect_parse_options));
    opts->args = resect_collection_create();
    return opts;
}

void resect_options_add(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_from_c(key));
    resect_collection_add(opts->args, resect_string_from_c(value));
}


void resect_options_add_concat(resect_parse_options opts, const char *key, const char *value) {
    resect_collection_add(opts->args, resect_string_format("%s%s", key, value));
}

void resect_options_add_include_path(resect_parse_options opts, const char *path) {
    resect_options_add(opts, "--include-directory", path);
}

void resect_options_add_framework_path(resect_parse_options opts, const char *framework) {
    resect_options_add_concat(opts, "-F", framework);
}

void resect_options_add_language(resect_parse_options opts, const char *lang) {
    resect_options_add(opts, "--language", lang);
}

void resect_options_add_standard(resect_parse_options opts, const char *standard) {
    resect_options_add(opts, "--std", standard);
}

void resect_options_free(resect_parse_options opts) {
    resect_iterator arg_iter = resect_collection_iterator(opts->args);
    while (resect_iterator_next(arg_iter)) {
        resect_string_free(resect_iterator_value(arg_iter));
    }
    resect_iterator_free(arg_iter);
    free(opts);
}

resect_translation_context resect_parse(const char *filename, resect_parse_options options) {
    const char **argv;
    int argc;
    if (options != NULL) {
        argc = (int) resect_collection_size(options->args);
        argv = malloc(argc * sizeof(char *));

        resect_iterator arg_iter = resect_collection_iterator(options->args);
        int i = 0;
        while (resect_iterator_next(arg_iter)) {
            resect_string arg = resect_iterator_value(arg_iter);
            argv[i++] = resect_string_to_c(arg);
        }
        resect_iterator_free(arg_iter);
    } else {
        argc = 0;
        argv = NULL;
    }

    resect_translation_context context = resect_context_create();

    CXIndex index = clang_createIndex(0, 1);

    CXTranslationUnit clangUnit = clang_parseTranslationUnit(index, filename, argv, argc,
                                                             NULL,
                                                             0, CXTranslationUnit_DetailedPreprocessingRecord |
                                                                CXTranslationUnit_KeepGoing |
                                                                CXTranslationUnit_SkipFunctionBodies);


    CXCursor cursor = clang_getTranslationUnitCursor(clangUnit);

    clang_visitChildren(cursor, resect_visit_context_declarations, context);

    clang_disposeTranslationUnit(clangUnit);
    clang_disposeIndex(index);

    return context;
}

void resect_free(resect_translation_context result) {
    resect_set deallocated = resect_set_create();
    resect_context_free(result, deallocated);
    resect_set_free(deallocated);
}