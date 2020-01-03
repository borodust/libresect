#include "resect.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <clang-c/Index.h>


struct resect_cursor {
    CXCursor handle;
};

struct resect_string {
    char *value;
    unsigned int capacity;
};

struct resect_parse_options {
    const char *const *clang_argv;
    int clang_argc;
};

struct resect_type {
    CXType handle;
};

/*
 * STRING
 */
resect_string resect_allocate_string(unsigned int initial_capacity) {
    resect_string result = malloc(sizeof(struct resect_string));
    result->capacity = initial_capacity > 0 ? initial_capacity : 1;
    result->value = malloc(result->capacity * sizeof(char));
    result->value[0] = 0;
    return result;
}

void resect_free_string(resect_string string) {
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
    return string == NULL ? resect_allocate_string(0) : string;
}


resect_string resect_ensure_string_default_length(resect_string string, int length) {
    return string == NULL ? resect_allocate_string(length) : string;
}

resect_string resect_ensure_string_default_value(resect_string string, const char *default_value) {
    if (string == NULL) {
        resect_string result;
        result = resect_allocate_string(0);
        resect_update_string(result, default_value);
        return result;
    } else {
        return string;
    }
}

resect_string resect_ensure_string_from_clang(resect_string provided, CXString from) {
    resect_string result = resect_ensure_string(provided);
    resect_update_string(result, clang_getCString(from));
    clang_disposeString(from);
    return result;
}

/*
 * CURSOR
 */
resect_cursor resect_allocate_cursor() {
    return malloc(sizeof(struct resect_cursor));
}

void resect_free_cursor(resect_cursor cursor) {
    free(cursor);
}

int resect_cursor_equal(resect_cursor this, resect_cursor that) {
    return clang_equalCursors(this->handle, that->handle) != 0;
}

resect_type resect_cursor_get_aliased_type(resect_type provided, resect_cursor cursor) {
    resect_type result = provided == NULL ? resect_allocate_type() : provided;
    CXType aliased_type = clang_getTypedefDeclUnderlyingType(cursor->handle);
    result->handle = clang_getCanonicalType(aliased_type);
    return result;
}

long long resect_cursor_get_enum_value(resect_cursor cursor) {
    return clang_getEnumConstantDeclValue(cursor->handle);
}

struct resect_cursor_location {
    unsigned int line;
    unsigned int column;
    resect_string name;
};

typedef struct {
    resect_visitor visit;
} resect_visitor_data;

enum CXChildVisitResult resect_visit_cursor_children(CXCursor current, CXCursor parent, CXClientData client_data) {
    resect_visitor_data *visitor_data = (resect_visitor_data *) client_data;

    struct resect_cursor current_cursor = {.handle = current};
    struct resect_cursor parent_cursor = {.handle = parent};

    resect_visit_result result = visitor_data->visit(&current_cursor, &parent_cursor);

    switch (result) {
        case RESECT_VISIT_RESULT_BREAK:
            return CXChildVisit_Break;
        case RESECT_VISIT_RESULT_CONTINUE:
            return CXChildVisit_Continue;
        default:
            return CXChildVisit_Recurse;
    }
}

struct resect_evaluation_result {
    resect_evaluation_result_kind kind;
    union {
        long long int_result;
        double float_result;
        resect_string string_result;
    };
};

resect_evaluation_result resect_evaluate(resect_cursor cursor) {
    assert(cursor != NULL);

    resect_evaluation_result resect_result = malloc(sizeof(struct resect_evaluation_result));

    CXEvalResult result = clang_Cursor_Evaluate(cursor->handle);
    if (result == NULL) {
        resect_result->kind = RESECT_EVALUATION_RESULT_KIND_UNKNOWN;
        return resect_result;
    }

    CXEvalResultKind resultKind = clang_EvalResult_getKind(result);

    switch (resultKind) {
        case CXEval_Int:
            resect_result->kind = RESECT_EVALUATION_RESULT_KIND_INT;
            resect_result->int_result = clang_EvalResult_getAsLongLong(result);
            break;
        case CXEval_Float:
            resect_result->kind = RESECT_EVALUATION_RESULT_KIND_FLOAT;
            resect_result->float_result = clang_EvalResult_getAsDouble(result);
            break;
        case CXEval_Other:
        case CXEval_CFStr:
        case CXEval_StrLiteral:
            resect_result->kind = RESECT_EVALUATION_RESULT_KIND_STRING;
            resect_result->string_result = resect_allocate_string(0);
            resect_update_string(resect_result->string_result, clang_EvalResult_getAsStr(result));
            break;
        default:
            resect_result->kind = RESECT_EVALUATION_RESULT_KIND_UNKNOWN;
            break;
    }
    clang_EvalResult_dispose(result);
    return resect_result;
}

void resect_free_evaluation_result(resect_evaluation_result result) {
    if (result != NULL && (result->kind == RESECT_EVALUATION_RESULT_KIND_STRING)) {
        resect_free_string(result->string_result);
    }
    free(result);
}

resect_string resect_evaluate_as_string(resect_string result, resect_cursor cursor) {
    assert(cursor != NULL);

    resect_evaluation_result eval_result = resect_evaluate(cursor);
    switch (eval_result->kind) {
        case RESECT_EVALUATION_RESULT_KIND_UNKNOWN:
            resect_update_string(result, "");
            break;
        case RESECT_EVALUATION_RESULT_KIND_INT: {
            int len = snprintf(NULL, 0, "%lld", eval_result->int_result);
            snprintf(_resect_prepare_c_string(result, len), len + 1, "%lld", eval_result->int_result);
        }
            break;
        case RESECT_EVALUATION_RESULT_KIND_FLOAT: {
            int len = snprintf(NULL, 0, "%f", eval_result->float_result);
            snprintf(_resect_prepare_c_string(result, len), len + 1, "%f", eval_result->float_result);
        }
            break;
        case RESECT_EVALUATION_RESULT_KIND_STRING: {
            resect_update_string(result, resect_string_to_c(eval_result->string_result));
        }
            break;
    }
    resect_free_evaluation_result(eval_result);

    return result;
}

resect_string resect_cursor_get_debug_info(resect_string provided_result, resect_cursor cursor) {
    assert(cursor != NULL);
    static const char *debug_format = "{ kind: \"%s\", name: \"%s\", "
                                      "type: { kind: \"%s\", signature: \"%s\" }, "
                                      "value: \"%s\", source: \"%s\" }";

    CXString cursor_spelling = clang_getCursorSpelling(cursor->handle);
    CXString cursor_kind_spelling = clang_getCursorKindSpelling(cursor->handle.kind);
    CXString source = clang_getCursorPrettyPrinted(cursor->handle, NULL);

    CXType type = clang_getCursorType(cursor->handle);
    CXString type_spelling = clang_getTypeSpelling(type);
    CXString type_kind_spelling = clang_getTypeKindSpelling(type.kind);

    resect_string cursor_value = resect_allocate_string(0);

//    if (clang_isPreprocessing(cursor->handle.kind)) {
//        resect_evaluate_as_string(cursor_value, cursor);
//    } else {
    resect_update_string(cursor_value, "");
//    }

    const char *cursor_kind_string = clang_getCString(cursor_kind_spelling);
    const char *cursor_string = clang_getCString(cursor_spelling);
    const char *type_kind_string = clang_getCString(type_kind_spelling);
    const char *type_string = clang_getCString(type_spelling);
    const char *source_string = clang_getCString(source);

    int required_length = snprintf(NULL, 0, debug_format,
                                   cursor_kind_string,
                                   cursor_string,
                                   type_kind_string,
                                   type_string,
                                   resect_string_to_c(cursor_value),
                                   source_string);

    resect_string result = resect_ensure_string_default_length(provided_result, required_length);

    snprintf(_resect_prepare_c_string(result, required_length), required_length + 1, debug_format,
             cursor_kind_string,
             cursor_string,
             type_kind_string,
             type_string,
             resect_string_to_c(cursor_value),
             source_string);

    clang_disposeString(cursor_spelling);
    clang_disposeString(cursor_kind_spelling);
    clang_disposeString(type_spelling);
    clang_disposeString(type_kind_spelling);
    clang_disposeString(source);
    resect_free_string(cursor_value);

    return result;
}

resect_cursor_kind resect_cursor_get_kind(resect_cursor cursor) {
    assert(cursor != NULL);
    switch (clang_getCursorKind(cursor->handle)) {
        case CXCursor_TranslationUnit:
            return RESECT_CURSOR_KIND_TRANSLATION_UNIT;
        case CXCursor_UnexposedDecl:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_StructDecl:
            return RESECT_CURSOR_KIND_STRUCT;
        case CXCursor_UnionDecl:
            return RESECT_CURSOR_KIND_UNION;
        case CXCursor_ClassDecl:
            return RESECT_CURSOR_KIND_CLASS;
        case CXCursor_EnumDecl:
            return RESECT_CURSOR_KIND_ENUM;
        case CXCursor_FieldDecl:
            return RESECT_CURSOR_KIND_FIELD;
        case CXCursor_EnumConstantDecl:
            return RESECT_CURSOR_KIND_ENUM_CONSTANT;
        case CXCursor_FunctionDecl:
            return RESECT_CURSOR_KIND_FUNCTION;
        case CXCursor_VarDecl:
            return RESECT_CURSOR_KIND_VARIABLE;
        case CXCursor_ParmDecl:
            return RESECT_CURSOR_KIND_PARAMETER;
        case CXCursor_TypedefDecl:
            return RESECT_CURSOR_KIND_TYPEDEF;
        case CXCursor_CXXMethod:
            return RESECT_CURSOR_KIND_METHOD;
        case CXCursor_Namespace:
            return RESECT_CURSOR_KIND_NAMESPACE;
        case CXCursor_LinkageSpec:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_Constructor:
            return RESECT_CURSOR_KIND_CONSTRUCTOR;
        case CXCursor_Destructor:
            return RESECT_CURSOR_KIND_DESTRUCTOR;
        case CXCursor_ConversionFunction:
            return RESECT_CURSOR_KIND_CONVERTER;
        case CXCursor_TemplateTypeParameter:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_NonTypeTemplateParameter:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_TemplateTemplateParameter:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_FunctionTemplate:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_ClassTemplate:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_ClassTemplatePartialSpecialization:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_NamespaceAlias:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_UsingDirective:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_UsingDeclaration:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_TypeAliasDecl:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_CXXAccessSpecifier:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_FirstRef:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_TypeRef:
            return RESECT_CURSOR_KIND_TYPE_REFERENCE;
        case CXCursor_CXXBaseSpecifier:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_TemplateRef:
            return RESECT_CURSOR_KIND_TEMPLATE_REFERENCE;
        case CXCursor_NamespaceRef:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_MemberRef:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_LabelRef:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_OverloadedDeclRef:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_VariableRef:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_FirstInvalid:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_NoDeclFound:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_NotImplemented:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_InvalidCode:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_IntegerLiteral:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_FloatingLiteral:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_ImaginaryLiteral:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_StringLiteral:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_CharacterLiteral:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_CompoundLiteralExpr:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_GNUNullExpr:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_CXXBoolLiteralExpr:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_CXXNullPtrLiteralExpr:
            return RESECT_CURSOR_KIND_UNKNOWN;
        case CXCursor_FixedPointLiteral:
            return RESECT_CURSOR_KIND_UNKNOWN;
        default:
            return RESECT_CURSOR_KIND_UNKNOWN;
    }
}

resect_string resect_cursor_get_id(resect_string provided, resect_cursor cursor) {
    return resect_ensure_string_from_clang(provided, clang_getCursorUSR(cursor->handle));
}

resect_cursor_location resect_allocate_cursor_location() {
    struct resect_cursor_location *location = calloc(1, sizeof(struct resect_cursor_location));
    location->name = resect_allocate_string(0);
    return location;
}

void resect_free_cursor_location(resect_cursor_location location) {
    resect_free_string(location->name);
    free(location);
}

unsigned int resect_cursor_location_line(resect_cursor_location location) {
    return location->line;
}

unsigned int resect_cursor_location_column(resect_cursor_location location) {
    return location->column;
}

const char *resect_cursor_location_name(resect_cursor_location location) {
    return resect_string_to_c(location->name);
}

resect_cursor_location resect_cursor_get_location(resect_cursor_location provided_location, resect_cursor cursor) {
    assert(cursor != NULL);
    struct resect_cursor_location *location = provided_location == NULL ?
                                              resect_allocate_cursor_location() : provided_location;

    CXFile file;
    clang_getFileLocation(clang_getCursorLocation(cursor->handle),
                          &file,
                          &location->line,
                          &location->column,
                          NULL);

    CXString name = clang_getFileName(file);
    resect_update_string(location->name, clang_getCString(name));
    clang_disposeString(name);

    return location;
}

resect_string resect_cursor_get_name(resect_string provided, resect_cursor cursor) {
    resect_string result = resect_ensure_string(provided);
    CXString name = clang_getCursorSpelling(cursor->handle);

    resect_update_string(result, clang_getCString(name));

    clang_disposeString(name);
    return result;
}

resect_string resect_cursor_get_comment(resect_string string, resect_cursor cursor) {
    resect_string result = resect_ensure_string(string);;

    CXString comment = clang_Cursor_getRawCommentText(cursor->handle);
    resect_update_string(result, clang_getCString(comment));
    clang_disposeString(comment);

    return result;
}

resect_type resect_allocate_type() {
    return malloc(sizeof(struct resect_type));
}

void resect_free_type(resect_type type) {
    free(type);
}

resect_type_kind resect_type_get_kind(resect_type type) {
    enum CXTypeKind kind = type->handle.kind;
    switch (kind) {
        case CXType_Invalid:
        case CXType_Unexposed:
            return RESECT_TYPE_KIND_UNKNOWN;
        default:
            return (resect_type_kind) type->handle.kind;
    }
}

resect_string resect_type_get_name(resect_string provided, resect_type type) {
    resect_string result = resect_ensure_string(provided);
    CXString name = clang_getTypeSpelling(type->handle);

    resect_update_string(result, clang_getCString(name));

    clang_disposeString(name);
    return result;
}

resect_cursor resect_type_get_declaration(resect_cursor provided_result, resect_type type) {
    CXCursor declaration = clang_getTypeDeclaration(type->handle);

    resect_cursor result = provided_result == NULL ? resect_allocate_cursor() : provided_result;
    result->handle = declaration;
    return result;
}

resect_type resect_cursor_get_type(resect_type cursor_type, resect_cursor cursor) {
    resect_type result = cursor_type == NULL ? resect_allocate_type() : cursor_type;
    result->handle = clang_getCursorType(cursor->handle);
    return result;
}

long long resect_type_sizeof(resect_type type) {
    return clang_Type_getSizeOf(type->handle);
}

long long resect_type_alignof(resect_type type) {
    return clang_Type_getAlignOf(type->handle);
}

long long resect_type_offsetof(resect_type type, const char *field_name) {
    return clang_Type_getOffsetOf(type->handle, field_name);
}

void resect_parse(const char *filename, resect_visitor visitor, resect_parse_options provided_options) {
    struct resect_parse_options options;
    if (provided_options != NULL) {
        options = *provided_options;
    } else {
        options.clang_argc = 0;
        options.clang_argv = NULL;
    }
    CXIndex index = clang_createIndex(0, 1);
    CXTranslationUnit unit = clang_parseTranslationUnit(index, filename, options.clang_argv, options.clang_argc, NULL,
                                                        0, CXTranslationUnit_DetailedPreprocessingRecord |
                                                           CXTranslationUnit_KeepGoing |
                                                           CXTranslationUnit_SkipFunctionBodies);

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    resect_visitor_data visitor_data = {.visit = visitor};

    clang_visitChildren(cursor, resect_visit_cursor_children, &visitor_data);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
}