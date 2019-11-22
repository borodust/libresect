#include "resect.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <clang-c/Documentation.h>

/*
 * STRING
 */
struct resect_string {
    char *value;
    unsigned int capacity;
};

resect_string *resect_create_string(unsigned int initial_capacity) {
    resect_string *result = malloc(sizeof(resect_string));
    result->capacity = initial_capacity > 0 ? initial_capacity : 1;
    result->value = malloc(result->capacity * sizeof(char));
    result->value[0] = 0;
    return result;
}

void resect_destroy_string(resect_string *string) {
    free(string->value);
    free(string);
}


const char *resect_string_to_c(resect_string *string) {
    assert(string);
    return string->value;
}

resect_string *resect_update_string(resect_string *string, const char *new_value) {
    assert(string);
    assert(new_value);

    size_t new_string_size = strlen(new_value);

    if (string->capacity < new_string_size + 1) {
        free(string->value);

        string->capacity = new_string_size + 1;

        char *value = malloc(sizeof(char) * string->capacity);
        assert(value);
        string->value = value;
    }

    memcpy(string->value, new_value, sizeof(char) * new_string_size);
    string->value[new_string_size] = 0;
    return string;
}

/*
 * OBJECT
 */
typedef enum resect_object_type {
    RESECT_UNKNOWN = 0,
    RESECT_INDEX,
    RESECT_TRANSLATION_UNIT,
    RESECT_CURSOR
} resect_object_type;

struct resect_object {
    resect_object_type type;
    union {
        void *handle;
        CXIndex *index;
        CXTranslationUnit translation_unit;
        CXCursor *cursor;
    };
};

resect_object resect_create_object(resect_object_type type, void *handle) {
    resect_object object = calloc(1, sizeof(struct resect_object));
    object->type = type;
    object->handle = handle;
    return object;
}

void resect_destroy_object(resect_object object) {
    free(object);
}

resect_object resect_create_index() {
    return resect_create_object(RESECT_INDEX, clang_createIndex(0, 0));
}

void resect_destroy_index(resect_object index) {
    assert(index);
    assert(index->type == RESECT_INDEX);
    clang_disposeIndex(index->index);
    resect_destroy_object(index);
}

/*
 * TRANSLATION UNIT
 */
resect_object resect_parse_translation_unit(resect_object index,
                                            const char *source,
                                            resect_parse_options *options) {
    assert(index->type == RESECT_INDEX);
    CXTranslationUnit unit = clang_parseTranslationUnit(index->index, source, NULL, 0, NULL, 0, 0);
    return resect_create_object(RESECT_TRANSLATION_UNIT, unit);
}

void resect_destroy_translation_unit(resect_object unit) {
    assert(unit);
    assert(unit->type == RESECT_TRANSLATION_UNIT);
    clang_disposeTranslationUnit(unit->translation_unit);
    resect_destroy_object(unit);
}

/*
 * CURSOR
 */

struct resect_cursor_location {
    unsigned int line;
    unsigned int column;
    resect_string *name;
};

typedef struct {
    resect_visitor visit;
} resect_visitor_data;

resect_object resect_create_cursor(CXCursor data) {
    CXCursor *cursor = malloc(sizeof(CXCursor));
    *cursor = data;
    return resect_create_object(RESECT_CURSOR, cursor);
}

resect_object resect_acquire_null_cursor() {
    return resect_create_cursor(clang_getNullCursor());
}

resect_object resect_acquire_translation_unit_cursor(resect_object translation_unit) {
    return resect_create_cursor(clang_getTranslationUnitCursor(translation_unit->translation_unit));
}

void resect_release_cursor(resect_object cursor) {
    assert(cursor);
    assert(cursor->type == RESECT_CURSOR);
    free(cursor->cursor);
    resect_destroy_object(cursor);
}

enum CXChildVisitResult resect_visit_cursor_children(CXCursor current, CXCursor parent, CXClientData client_data) {
    resect_visitor_data *visitor_data = (resect_visitor_data *) client_data;

    struct resect_object current_cursor = {.type =  RESECT_CURSOR, .handle =  &current};
    struct resect_object parent_cursor = {.type =  RESECT_CURSOR, .handle =  &parent};

    visitor_data->visit(&current_cursor, &parent_cursor);

    return CXChildVisit_Recurse;
}

void resect_visit_children(resect_object cursor, resect_visitor visitor) {
    assert(cursor);
    assert(visitor);
    assert(cursor->type == RESECT_CURSOR);
    resect_visitor_data data;
    data.visit = visitor;
    clang_visitChildren(*cursor->cursor, resect_visit_cursor_children, &data);
}

resect_cursor_kind resect_get_cursor_kind(resect_object cursor) {
    assert(cursor);
    assert(cursor->type == RESECT_CURSOR);
    switch (clang_getCursorKind(*cursor->cursor)) {
        case CXCursor_UnexposedDecl:
            break;
        case CXCursor_StructDecl:
            break;
        case CXCursor_UnionDecl:
            break;
        case CXCursor_ClassDecl:
            break;
        case CXCursor_EnumDecl:
            break;
        case CXCursor_FieldDecl:
            break;
        case CXCursor_EnumConstantDecl:
            break;
        case CXCursor_FunctionDecl:
            break;
        case CXCursor_VarDecl:
            break;
        case CXCursor_ParmDecl:
            break;
        case CXCursor_TypedefDecl:
            break;
        case CXCursor_CXXMethod:
            break;
        case CXCursor_Namespace:
            break;
        case CXCursor_LinkageSpec:
            break;
        case CXCursor_Constructor:
            break;
        case CXCursor_Destructor:
            break;
        case CXCursor_ConversionFunction:
            break;
        case CXCursor_TemplateTypeParameter:
            break;
        case CXCursor_NonTypeTemplateParameter:
            break;
        case CXCursor_TemplateTemplateParameter:
            break;
        case CXCursor_FunctionTemplate:
            break;
        case CXCursor_ClassTemplate:
            break;
        case CXCursor_ClassTemplatePartialSpecialization:
            break;
        case CXCursor_NamespaceAlias:
            break;
        case CXCursor_UsingDirective:
            break;
        case CXCursor_UsingDeclaration:
            break;
        case CXCursor_TypeAliasDecl:
            break;
        case CXCursor_CXXAccessSpecifier:
            break;
        case CXCursor_FirstRef:
            break;
        case CXCursor_ObjCProtocolRef:
            break;
        case CXCursor_ObjCClassRef:
            break;
        case CXCursor_TypeRef:
            break;
        case CXCursor_CXXBaseSpecifier:
            break;
        case CXCursor_TemplateRef:
            break;
        case CXCursor_NamespaceRef:
            break;
        case CXCursor_MemberRef:
            break;
        case CXCursor_LabelRef:
            break;
        case CXCursor_OverloadedDeclRef:
            break;
        case CXCursor_VariableRef:
            break;
        case CXCursor_FirstInvalid:
            break;
        case CXCursor_NoDeclFound:
            break;
        case CXCursor_NotImplemented:
            break;
        case CXCursor_InvalidCode:
            break;
        case CXCursor_IntegerLiteral:
            break;
        case CXCursor_FloatingLiteral:
            break;
        case CXCursor_ImaginaryLiteral:
            break;
        case CXCursor_StringLiteral:
            break;
        case CXCursor_CharacterLiteral:
            break;
        case CXCursor_CompoundLiteralExpr:
            break;
        case CXCursor_GNUNullExpr:
            break;
        case CXCursor_CXXBoolLiteralExpr:
            break;
        case CXCursor_CXXNullPtrLiteralExpr:
            break;
        case CXCursor_CXXThisExpr:
            break;
        case CXCursor_FixedPointLiteral:
            break;
    }
    return RESECT_CURSOR_KIND_UNKNOWN;
}

resect_cursor_location *resect_create_cursor_location() {
    resect_cursor_location *location = calloc(1, sizeof(resect_cursor_location));
    location->name = resect_create_string(MAX_FILENAME_LENGTH + 1);
    return location;
}

void resect_destroy_cursor_location(resect_cursor_location *location) {
    resect_destroy_string(location->name);
    free(location);
}

unsigned int resect_cursor_location_line(resect_cursor_location *location) {
    return location->line;
}

unsigned int resect_cursor_location_column(resect_cursor_location *location) {
    return location->column;
}

const char *resect_cursor_location_name(resect_cursor_location *location) {
    return resect_string_to_c(location->name);
}

resect_cursor_location *resect_get_cursor_location(resect_object cursor, resect_cursor_location *provided_location) {
    assert(cursor);
    assert(cursor->type == RESECT_CURSOR);
    resect_cursor_location *location = provided_location == NULL ?
                                       resect_create_cursor_location() : provided_location;

    CXFile file;
    clang_getSpellingLocation(clang_getCursorLocation(*cursor->cursor),
                              &file,
                              &location->line,
                              &location->column,
                              NULL);

    CXString name = clang_getFileName(file);
    resect_update_string(location->name, clang_getCString(name));
    clang_disposeString(name);

    return location;
}

resect_string *resect_cursor_get_comment(resect_object cursor, resect_string *string) {
    assert(cursor->type == RESECT_CURSOR);
    resect_string *result = string == NULL ? resect_create_string(0) : string;

    CXComment comment = clang_Cursor_getParsedComment(*cursor->cursor);
    enum CXCommentKind kind = clang_Comment_getKind(comment);
    if (kind == CXComment_Null) {
        resect_update_string(result, "");
        return result;
    }

    CXString text = clang_TextComment_getText(comment);
    resect_update_string(result, clang_getCString(text));
    clang_disposeString(text);

    return result;
}