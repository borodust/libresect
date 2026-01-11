#include "../resect.h"
#include "resect_private.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <clang-c/Index.h>

/*
 * LOCATION
 */
struct P_resect_location {
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

resect_string resect_location_to_string(resect_location location) {
    return resect_string_format("%s:%d:%d",
                                resect_location_name(location),
                                resect_location_line(location),
                                resect_location_column(location));
}

resect_location resect_location_from_cursor(CXCursor cursor) {
    resect_location result = malloc(sizeof(struct P_resect_location));

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
 * TEMPLATE ARGUMENT
 */
struct P_resect_template_argument {
    int position;
    resect_template_argument_kind kind;
    resect_type type;
    long long int value;
};


resect_template_argument_kind convert_template_argument_kind(enum CXTemplateArgumentKind kind) {
    switch (kind) {
        case CXTemplateArgumentKind_Null:
            return RESECT_TEMPLATE_ARGUMENT_KIND_NULL;
        case CXTemplateArgumentKind_Type:
            return RESECT_TEMPLATE_ARGUMENT_KIND_TYPE;
        case CXTemplateArgumentKind_Declaration:
            return RESECT_TEMPLATE_ARGUMENT_KIND_DECLARATION;
        case CXTemplateArgumentKind_NullPtr:
            return RESECT_TEMPLATE_ARGUMENT_KIND_NULL_PTR;
        case CXTemplateArgumentKind_Integral:
            return RESECT_TEMPLATE_ARGUMENT_KIND_INTEGRAL;
        case CXTemplateArgumentKind_Template:
            return RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE;
        case CXTemplateArgumentKind_TemplateExpansion:
            return RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE_EXPANSION;
        case CXTemplateArgumentKind_Expression:
            return RESECT_TEMPLATE_ARGUMENT_KIND_EXPRESSION;
        case CXTemplateArgumentKind_Pack:
            return RESECT_TEMPLATE_ARGUMENT_KIND_PACK;
        default:
            return RESECT_TEMPLATE_ARGUMENT_KIND_UNKNOWN;
    }
}

resect_template_argument resect_template_argument_create(resect_template_argument_kind kind,
                                                         resect_type type,
                                                         long long int value,
                                                         int arg_number) {
    resect_template_argument arg = malloc(sizeof(struct P_resect_template_argument));

    arg->position = arg_number;
    arg->kind = kind;
    arg->type = type;
    arg->value = value;

    return arg;
}

void resect_template_argument_free(resect_template_argument arg, resect_set deallocated) {
    if (!resect_set_add(deallocated, arg)) {
        return;
    }
    if (arg->type != NULL) {
        resect_type_free(arg->type, deallocated);
    }

    free(arg);
}

void resect_init_template_args_from_cursor(resect_visit_context visit_context,
                                           resect_translation_context context,
                                           resect_collection args,
                                           CXCursor cursor) {
    int arg_count = clang_Cursor_getNumTemplateArguments(cursor);

    for (int i = 0; i < arg_count; ++i) {
        resect_template_argument_kind arg_kind =
                convert_template_argument_kind(clang_Cursor_getTemplateArgumentKind(cursor, i));

        resect_type arg_type = NULL;
        long long int arg_value = 0;

        switch (arg_kind) {
            case RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE_EXPANSION:
            case RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE:
            case RESECT_TEMPLATE_ARGUMENT_KIND_TYPE:
            case RESECT_TEMPLATE_ARGUMENT_KIND_DECLARATION:
                arg_type = resect_type_create(visit_context,
                                              context,
                                              clang_Cursor_getTemplateArgumentType(cursor, i));
                break;
            case RESECT_TEMPLATE_ARGUMENT_KIND_INTEGRAL:
                arg_value = clang_Cursor_getTemplateArgumentValue(cursor, i);
                break;
            default: ;
        }

        resect_collection_add(args, resect_template_argument_create(arg_kind, arg_type, arg_value, i));
    }
}

void resect_template_argument_collection_free(resect_collection args, resect_set deallocated) {
    resect_iterator template_arg_iter = resect_collection_iterator(args);
    while (resect_iterator_next(template_arg_iter)) {
        resect_template_argument_free(resect_iterator_value(template_arg_iter), deallocated);
    }
    resect_iterator_free(template_arg_iter);
    resect_collection_free(args);
}

resect_template_argument_kind resect_template_argument_get_kind(resect_template_argument arg) {
    return arg->kind;
}

resect_type resect_template_argument_get_type(resect_template_argument arg) {
    return arg->type;
}

long long resect_template_argument_get_value(resect_template_argument arg) {
    return arg->value;
}

int resect_template_argument_get_position(resect_template_argument arg) {
    return arg->position;
}

/*
 * DECLARATION
 */

typedef struct P_resect_decl_child_visit_data {
    resect_visit_context visit_context;
    resect_translation_context translation_context;
    resect_decl parent;
} *resect_decl_child_visit_data;

typedef struct P_resect_decl_visit_data {
    resect_translation_context context;
    resect_decl_result result;
} *resect_decl_visit_data;

struct P_resect_decl {
    resect_decl_kind kind;
    resect_string id;
    resect_string name;
    resect_string namespace;
    resect_location location;
    resect_string mangled_name;
    resect_string comment;
    resect_access_specifier access;
    resect_linkage_kind linkage;

    resect_bool is_template;
    resect_collection template_parameters;
    resect_collection template_arguments;
    resect_decl template;
    resect_bool partial;
    resect_bool forward;
    resect_collection specializations;

    resect_decl owner;
    resect_type type;

    resect_string source;

    void *data;
    resect_data_deallocator data_deallocator;
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

resect_linkage_kind convert_linkage(enum CXLinkageKind kind) {
    switch (kind) {
        case CXLinkage_NoLinkage:
            return RESECT_LINKAGE_KIND_NO_LINKAGE;
        case CXLinkage_Internal:
            return RESECT_LINKAGE_KIND_INTERNAL;
        case CXLinkage_UniqueExternal:
            return RESECT_LINKAGE_KIND_UNIQUE_EXTERNAL;
        case CXLinkage_External:
            return RESECT_LINKAGE_KIND_EXTERNAL;
        default:
            return RESECT_LINKAGE_KIND_UNKNOWN;
    }
}

resect_bool resect_is_forward_declaration(CXCursor cursor) {
    CXCursor definition = clang_getCursorDefinition(cursor);

    return clang_equalCursors(definition, clang_getNullCursor())
           || !clang_equalCursors(cursor, definition);
}

resect_bool resect_is_specialized(CXCursor cursor) {
    if (clang_equalCursors(clang_getSpecializedCursorTemplate(cursor), clang_getNullCursor())) {
        return resect_false;
    }
    return resect_true;
}

void resect_extract_decl_namespace(resect_collection namespace_queue, CXCursor cursor) {
    CXCursor parent = clang_getCursorSemanticParent(cursor);
    if (!clang_Cursor_isNull(parent)) {
        resect_extract_decl_namespace(namespace_queue, parent);
    }
    if (clang_getCursorKind(cursor) == CXCursor_Namespace && !clang_Cursor_isInlineNamespace(cursor)) {
        resect_collection_add(namespace_queue, resect_string_from_clang(clang_getCursorSpelling(cursor)));
    }
}

resect_string resect_format_cursor_namespace(CXCursor cursor) {
    resect_collection namespace_queue = resect_collection_create();
    resect_extract_decl_namespace(namespace_queue, cursor);

    resect_string result = resect_string_from_c("");

    int i = 0;
    resect_iterator iter = resect_collection_iterator(namespace_queue);
    while (resect_iterator_next(iter)) {
        resect_string namespace = resect_iterator_value(iter);
        if (i > 0) {
            resect_string_append(result, "::");
        }
        resect_string_append(result, resect_string_to_c(namespace));
        ++i;
    }
    resect_iterator_free(iter);
    resect_string_collection_free(namespace_queue);
    return result;
}

static void resect_decl_visit(resect_visit_context visit_context,
                              CXCursor cursor, resect_decl_visit_data data) {
    resect_visit_cursor(visit_context, cursor, data);
}

resect_decl resect_find_owner(resect_visit_context visit_context,
                              resect_translation_context context,
                              CXCursor cursor) {
    CXCursor owning_cursor = resect_find_declaration_owning_cursor(cursor);
    if (clang_Cursor_isNull(owning_cursor)) {
        return NULL;
    }

    return resect_decl_create(visit_context, context, owning_cursor).decl;
}

static bool is_kind_exportable(resect_decl_kind kind) {
    switch (kind) {
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_CLASS:
        case RESECT_DECL_KIND_UNION:
        case RESECT_DECL_KIND_ENUM:
        case RESECT_DECL_KIND_FUNCTION:
        case RESECT_DECL_KIND_VARIABLE:
        case RESECT_DECL_KIND_TYPEDEF:
        case RESECT_DECL_KIND_MACRO:
            return true;
        default:
            return false;
    }
}

bool is_cursor_anonymous(CXCursor cursor) {
    switch (convert_cursor_kind(cursor)) {
        case RESECT_DECL_KIND_UNKNOWN:
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_UNION:
        case RESECT_DECL_KIND_CLASS:
        case RESECT_DECL_KIND_ENUM:
        case RESECT_DECL_KIND_UNDECLARED:
            break;
        default:
            return false;
    }

    if (clang_Cursor_isAnonymousRecordDecl(cursor) || clang_Cursor_isAnonymous(cursor)) {
        return true;
    }

    bool result = false;
    resect_string name = resect_string_from_clang(clang_getCursorSpelling(cursor));
    if (resect_string_length(name) == 0) {
        result = true;
    }
    resect_string_free(name);
    return result;
}

void resect_decl_init_template_from_cursor(resect_visit_context visit_context,
                                           resect_decl decl,
                                           resect_translation_context context,
                                           CXCursor cursor) {
    resect_init_template_args_from_cursor(visit_context, context, decl->template_arguments, cursor);

    if (resect_is_specialized(cursor)) {
        resect_decl_result decl_result = resect_decl_create(visit_context,
                                                            context,
                                                            clang_getSpecializedCursorTemplate(cursor));
        decl->template = decl_result.decl;
    }
}

struct P_function_class_mangling_result {
    CXCursor cursor;
    resect_string mangling;
};

static enum CXChildVisitResult find_mangled_name(CXCursor cursor,
                                                 CXCursor parent,
                                                 CXClientData data) {
    struct P_function_class_mangling_result *mangling_result = data;
    CXCursor source = mangling_result->cursor;
    CXCursor semantic_parent = clang_getCursorSemanticParent(cursor);
    if (clang_getCursorKind(cursor) == CXCursor_Constructor && clang_equalCursors(source, semantic_parent)) {
        assert(mangling_result->mangling == NULL);
        CXStringSet *manglings = clang_Cursor_getCXXManglings(cursor);
        mangling_result->mangling = manglings->Count > 0
                                        ? resect_string_from_c(clang_getCString(manglings->Strings[0]))
                                        : NULL;
        clang_disposeStringSet(manglings);
    } else {
        clang_visitChildren(cursor, find_mangled_name, data);
    }

    return mangling_result->mangling == NULL ? CXChildVisit_Continue : CXChildVisit_Break;
}

static resect_string get_cursor_mangling(resect_string decl_name, resect_string decl_namespace, CXCursor cursor) {
    if (convert_cursor_kind(cursor) == RESECT_DECL_KIND_CLASS
        && clang_Type_getSizeOf(clang_getCursorType(cursor)) > 0) {
        struct P_function_class_mangling_result mangling_result = {.cursor = cursor, .mangling = NULL};
        clang_visitChildren(cursor, find_mangled_name, &mangling_result);

        if (mangling_result.mangling != NULL) {
            return mangling_result.mangling;
        }
    }

    return resect_string_from_clang(clang_Cursor_getMangling(cursor));
}

resect_bool resect_cursor_is_template(CXCursor cursor) {
    switch (clang_getCursorKind(cursor)) {
        case CXCursor_FunctionTemplate:
        case CXCursor_ClassTemplate:
        case CXCursor_ClassTemplatePartialSpecialization:
            return resect_true;
        default:
            return resect_false;
    }
}

void resect_decl_init_rest_from_cursor(resect_decl decl,
                                       resect_translation_context context,
                                       CXCursor cursor) {
    if (is_cursor_anonymous(cursor)) {
        decl->name = resect_string_from_c("");
    } else {
        decl->name = resect_string_from_clang(clang_getCursorSpelling(cursor));
    }
    decl->location = resect_location_from_cursor(cursor);
    decl->comment = resect_string_from_clang(clang_Cursor_getRawCommentText(cursor));

    decl->namespace = resect_format_cursor_namespace(cursor);
    decl->access = convert_access_specifier(clang_getCXXAccessSpecifier(cursor));
    decl->linkage = convert_linkage(clang_getCursorLinkage(cursor));
    if (resect_string_length(decl->name) == 0) {
        decl->mangled_name = resect_string_from_c("");
    } else {
        decl->mangled_name = get_cursor_mangling(decl->name, decl->namespace, cursor);
    }

    decl->template = NULL;
    decl->template_parameters = resect_collection_create();
    decl->template_arguments = resect_collection_create();
    decl->specializations = resect_collection_create();
    decl->partial = cursor.kind == CXCursor_ClassTemplatePartialSpecialization;
    decl->forward = resect_is_forward_declaration(cursor);

    decl->is_template = resect_cursor_is_template(cursor);

    decl->owner = NULL;

    CXPrintingPolicy pp = clang_getCursorPrintingPolicy(cursor);
    clang_PrintingPolicy_setProperty(pp, CXPrintingPolicy_PolishForDeclaration, 1);
    clang_PrintingPolicy_setProperty(pp, CXPrintingPolicy_SuppressScope, 1);
    clang_PrintingPolicy_setProperty(pp, CXPrintingPolicy_FullyQualifiedName, 1);
    decl->source = resect_string_from_clang(clang_getCursorPrettyPrinted(cursor, pp));
    clang_PrintingPolicy_dispose(pp);

    decl->data_deallocator = NULL;
    decl->data = NULL;
}

void resect_decl_register_specialization(resect_decl decl, resect_type specialization) {
    assert(decl != NULL
        && resect_decl_is_template(decl)
        && specialization != NULL);

    resect_collection_add(decl->specializations, specialization);
}

resect_decl resect_decl_get_root_template(resect_decl decl) {
    if (resect_collection_size(decl->template_arguments) > 0) {
        return resect_decl_get_root_template(decl->template);
    }

    if (decl->is_template) {
        return decl;
    }

    return NULL;
}

void resect_record_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                        CXCursor cursor);

void resect_enum_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                      CXCursor cursor);

void resect_enum_constant_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                               CXCursor cursor);

void resect_function_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                          CXCursor cursor);

void resect_variable_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                          CXCursor cursor);

void resect_typedef_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                         CXCursor cursor);

void resect_field_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                       CXCursor cursor);

void resect_method_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                        CXCursor cursor);

void resect_macro_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                       CXCursor cursor);

void resect_template_parameter_init(resect_visit_context visit_context, resect_translation_context context,
                                    resect_decl decl, CXCursor cursor);

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

void resect_decl__create(resect_visit_context visit_context,
                         resect_translation_context context,
                         CXCursor cursor,
                         resect_decl_result *result);


resect_decl_visit_data resect_decl_visit_data_create(resect_translation_context context) {
    resect_decl_visit_data data = malloc(sizeof(struct P_resect_decl_visit_data));
    data->context = context;
    data->result.kind = RESECT_DECL_KIND_UNKNOWN;
    data->result.decl = NULL;
    return data;
}

void resect_visit_decl_data_free(resect_decl_visit_data data) {
    free(data);
}

void resect_decl_parse(resect_visit_context visit_context, CXCursor cursor, void *data) {
    resect_decl_visit_data visit_data = data;
    resect_decl__create(visit_context, visit_data->context, cursor, &visit_data->result);
    resect_context_flush_template_parameters(visit_data->context);
}

resect_decl_result resect_decl_create(resect_visit_context visit_context,
                                      resect_translation_context context,
                                      CXCursor cursor) {
    struct P_resect_decl_visit_data visit_data = {
        .context = context,
        .result = {.kind = RESECT_DECL_KIND_UNKNOWN, .decl = NULL}
    };
    resect_decl_visit(visit_context, cursor, &visit_data);
    return visit_data.result;
}

void resect_decl__create(resect_visit_context visit_context,
                         resect_translation_context context,
                         CXCursor cursor,
                         resect_decl_result *result) {
    result->decl = NULL;
    result->kind = convert_cursor_kind(cursor);

    resect_string decl_id = resect_extract_decl_id(cursor);

    if (!resect_is_decl_included(context, decl_id)) {
        goto done;
    }

    resect_decl registered_decl = resect_find_decl(context, decl_id);
    if (registered_decl != NULL) {
        result->decl = registered_decl;
        goto done;
    }

    resect_decl decl = malloc(sizeof(struct P_resect_decl));
    memset(decl, 0, sizeof(struct P_resect_decl));

    decl->id = resect_string_copy(decl_id);
    decl->kind = result->kind;

    resect_register_decl(context, decl->id, decl);
    resect_register_decl_language(context, convert_language(clang_getCursorLanguage(cursor)));

    resect_decl_init_rest_from_cursor(decl, context, cursor);

    decl->type = resect_type_create(visit_context, context, clang_getCursorType(cursor));

    switch (decl->kind) {
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_CLASS:
        case RESECT_DECL_KIND_UNION:
        case RESECT_DECL_KIND_ENUM:
        case RESECT_DECL_KIND_VARIABLE:
        case RESECT_DECL_KIND_TYPEDEF:
        case RESECT_DECL_KIND_FIELD:
        case RESECT_DECL_KIND_METHOD:
            decl->owner = resect_find_owner(visit_context, context, cursor);
            break;
        default: ;
    }

    switch (decl->kind) {
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_CLASS:
        case RESECT_DECL_KIND_UNION:
            resect_record_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_ENUM:
            resect_enum_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_ENUM_CONSTANT:
            resect_enum_constant_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_FUNCTION:
            resect_function_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_VARIABLE:
            resect_variable_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_TYPEDEF:
            resect_typedef_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_FIELD:
            resect_field_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_METHOD:
            resect_method_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_MACRO:
            resect_macro_init(visit_context, context, decl, cursor);
            break;
        case RESECT_DECL_KIND_TEMPLATE_PARAMETER:
            resect_template_parameter_init(visit_context, context, decl, cursor);
            break;
        default: ;
    }

    resect_decl_init_template_from_cursor(visit_context, decl, context, cursor);

    result->decl = decl;

    if (is_kind_exportable(decl->kind)) {
        resect_export_decl(context, decl);
    }

done:
    resect_string_free(decl_id);
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
    resect_template_argument_collection_free(decl->template_arguments, deallocated);
    resect_type_collection_free(decl->specializations, deallocated);

    if (decl->template != NULL) {
        resect_decl_free(decl->template, deallocated);
    }
    if (decl->owner != NULL) {
        resect_decl_free(decl->owner, deallocated);
    }

    resect_string_free(decl->source);

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

resect_bool resect_decl_is_anonymous(resect_decl decl) {
    return resect_string_equal_c(decl->name, "");
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

resect_decl resect_decl_get_owner(resect_decl decl) {
    return decl->owner;
}

resect_decl resect_decl_get_template(resect_decl decl) {
    return decl->template;
}

resect_bool resect_decl_is_partially_specialized(resect_decl decl) {
    return decl->partial;
}

resect_bool resect_decl_is_template(resect_decl decl) {
    return decl->is_template;
}

resect_collection resect_decl_template_parameters(resect_decl decl) {
    return decl->template_parameters;
}

resect_collection resect_decl_template_specializations(resect_decl decl) {
    return decl->specializations;
}

resect_collection resect_decl_template_arguments(resect_decl decl) {
    return decl->template_arguments;
}

const char *resect_decl_get_source(resect_decl decl) {
    return resect_string_to_c(decl->source);
}

resect_linkage_kind resect_decl_get_linkage(resect_decl decl) {
    return decl->linkage;
}

resect_bool resect_decl_is_forward(resect_decl decl) {
    return decl->forward;
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
typedef struct P_resect_field_data {
    resect_bool bitfield;
    long long width;
    long long offset;
} *resect_field_data;

typedef struct P_resect_record_data {
    resect_collection fields;
    resect_collection methods;
    resect_collection parents;
    resect_bool abstract;
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

resect_bool resect_record_is_abstract(resect_decl decl) {
    assert(resect_is_struct(decl));
    resect_record_data data = decl->data;
    return data->abstract;
}

resect_collection resect_record_parents(resect_decl decl) {
    assert(resect_is_struct(decl));
    resect_record_data data = decl->data;
    return data->parents;
}

long long resect_field_decl_get_offset(resect_decl field_decl) {
    assert(field_decl->kind == RESECT_DECL_KIND_FIELD);
    resect_field_data data = field_decl->data;
    return data->offset;
}

resect_bool resect_field_decl_is_bitfield(resect_decl field_decl) {
    assert(field_decl->kind == RESECT_DECL_KIND_FIELD);
    resect_field_data data = field_decl->data;
    return data->bitfield;
}

long long resect_field_decl_get_width(resect_decl field_decl) {
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

void resect_field_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                       CXCursor cursor) {
    resect_field_data data = malloc(sizeof(struct P_resect_field_data));

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
        CXType type = clang_getCursorType(cursor);

        resect_type parent_type = resect_type_create(visit_data->visit_context,
                                                     visit_data->translation_context, type);
        if (resect_type_get_declaration(parent_type) != NULL) {
            resect_collection_add(record_data->parents, parent_type);
        }

        return CXChildVisit_Continue;
    }

    resect_decl_result decl_result = resect_decl_create(visit_data->visit_context,
                                                        visit_data->translation_context, cursor);

    if (decl_result.kind == RESECT_DECL_KIND_TEMPLATE_PARAMETER
        && decl_result.decl != NULL) {
        return CXChildVisit_Break;
    }

    if (decl_result.decl != NULL) {
        resect_decl decl = decl_result.decl;
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
            default: ;
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
    resect_type_collection_free(record_data->parents, deallocated);

    free(data);
}

void resect_record_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                        CXCursor cursor) {
    resect_record_data data = malloc(sizeof(struct P_resect_record_data));
    data->methods = resect_collection_create();
    data->fields = resect_collection_create();
    data->parents = resect_collection_create();
    data->abstract = convert_bool_from_uint(clang_CXXRecord_isAbstract(cursor));

    decl->data_deallocator = resect_record_data_free;
    decl->data = data;

    struct P_resect_decl_child_visit_data visit_data = {
        .visit_context = visit_context, .translation_context = context, .parent = decl
    };
    clang_visitChildren(cursor, resect_visit_record_child, &visit_data);
}

/*
 * TYPEDEF
 */
typedef struct P_resect_typedef_data {
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

void resect_typedef_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                         CXCursor cursor) {
    resect_typedef_data data = malloc(sizeof(struct P_resect_typedef_data));

    CXType canonical_type = clang_getCanonicalType(clang_getTypedefDeclUnderlyingType(cursor));

    data->aliased_type = resect_type_create(visit_context, context, canonical_type);

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
typedef struct P_resect_function_data {
    resect_bool variadic;
    resect_storage_class storage_class;
    resect_collection parameters;
    resect_function_calling_convention calling_convention;
    resect_type result_type;
    resect_bool inlined;
} *resect_function_data;

resect_type resect_function_get_result_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->result_type;
}

resect_storage_class resect_function_get_storage_class(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->storage_class;
}

resect_bool resect_function_is_variadic(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->variadic;
}

resect_bool resect_function_is_inlined(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->inlined;
}

resect_collection resect_function_parameters(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_FUNCTION);
    resect_function_data data = decl->data;
    return data->parameters;
}

void resect_visit_function_child(resect_decl_child_visit_data visit_data,
                                 CXCursor cursor,
                                 resect_function_data data) {
    if (cursor.kind == CXCursor_UnexposedExpr) {
        return;
    }
    resect_decl_result decl_result = resect_decl_create(visit_data->visit_context,
                                                        visit_data->translation_context, cursor);

    if (decl_result.decl != NULL) {
        resect_decl decl = decl_result.decl;
        switch (decl->kind) {
            case RESECT_DECL_KIND_PARAMETER:
                resect_collection_add(data->parameters, decl);
                break;
            case RESECT_DECL_KIND_TEMPLATE_PARAMETER: {
                if (cursor.kind != CXCursor_TypeRef && cursor.kind != CXCursor_DeclRefExpr) {
                    resect_collection_add(visit_data->parent->template_parameters, decl);
                }
            }
            break;
            default: ;
        }
    }
}

enum CXChildVisitResult resect_visit_function_parameter(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl_child_visit_data visit_data = data;
    resect_function_data function_data = visit_data->parent->data;

    resect_visit_function_child(visit_data, cursor, function_data);

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

resect_storage_class convert_storage_class(enum CX_StorageClass storage_class) {
    switch (storage_class) {
        case CX_SC_None:
            return RESECT_STORAGE_CLASS_NONE;
        case CX_SC_Extern:
            return RESECT_STORAGE_CLASS_EXTERN;
        case CX_SC_Static:
            return RESECT_STORAGE_CLASS_STATIC;
        case CX_SC_PrivateExtern:
            return RESECT_STORAGE_CLASS_PRIVATE_EXTERN;
        case CX_SC_OpenCLWorkGroupLocal:
            return RESECT_STORAGE_CLASS_OPENCL_WORKGROUP_LOCAL;
        case CX_SC_Auto:
            return RESECT_STORAGE_CLASS_AUTO;
        case CX_SC_Register:
            return RESECT_STORAGE_CLASS_REGISTER;
        default:
            return RESECT_STORAGE_CLASS_UNKNOWN;
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

resect_function_data resect_function_data_create(resect_visit_context visit_context, resect_translation_context context,
                                                 CXCursor cursor) {
    resect_function_data data = malloc(sizeof(struct P_resect_function_data));

    CXType functionType = clang_getCursorType(cursor);
    data->parameters = resect_collection_create();
    data->storage_class = convert_storage_class(clang_Cursor_getStorageClass(cursor));
    data->calling_convention = convert_calling_convention(clang_getFunctionTypeCallingConv(functionType));
    data->variadic = clang_isFunctionTypeVariadic(functionType) != 0 ? resect_true : resect_false;
    data->result_type = resect_type_create(visit_context, context, clang_getResultType(functionType));
    data->inlined = convert_bool_from_uint(clang_Cursor_isFunctionInlined(cursor));

    return data;
}

void resect_function_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                          CXCursor cursor) {
    resect_function_data function_data = resect_function_data_create(visit_context, context, cursor);

    decl->data_deallocator = resect_function_data_free;
    decl->data = function_data;

    struct P_resect_decl_child_visit_data visit_data = {
        .visit_context = visit_context, .translation_context = context, .parent = decl
    };
    clang_visitChildren(cursor, resect_visit_function_parameter, &visit_data);
}

/*
 * ENUM
 */
typedef struct P_resect_enum_constant_data {
    resect_bool is_unsigned;
    unsigned long long unsigned_value;
    long long value;
} *resect_enum_constant_data;


void resect_enum_constant_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }

    free(data);
}

long long resect_enum_constant_value(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_ENUM_CONSTANT);
    resect_enum_constant_data data = decl->data;
    return data->value;
}

unsigned long long resect_enum_constant_unsigned_value(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_ENUM_CONSTANT);
    resect_enum_constant_data data = decl->data;
    return data->unsigned_value;
}

resect_bool resect_enum_constant_is_unsigned(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_ENUM_CONSTANT);
    resect_enum_constant_data data = decl->data;
    return data->is_unsigned;
}

void resect_enum_constant_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                               CXCursor cursor) {
    resect_enum_constant_data data = malloc(sizeof(struct P_resect_enum_constant_data));

    CXType enum_value_type = clang_getEnumDeclIntegerType(clang_getCursorSemanticParent(cursor));

    bool is_unsigned = false;
    switch (enum_value_type.kind) {
        case CXType_UChar:
        case CXType_UShort:
        case CXType_UInt:
        case CXType_ULong:
        case CXType_ULongLong:
        case CXType_UInt128:
        case CXType_UAccum:
        case CXType_UShortAccum:
        case CXType_ULongAccum:
            is_unsigned = true;
            break;
        default:
            is_unsigned = false;
    }

    if (is_unsigned) {
        data->unsigned_value = clang_getEnumConstantDeclUnsignedValue(cursor);
        data->value = (long long) data->unsigned_value;
        data->is_unsigned = resect_true;
    } else {
        data->value = clang_getEnumConstantDeclValue(cursor);
        data->unsigned_value = (unsigned long long) data->value;
        data->is_unsigned = resect_false;
    }

    decl->data_deallocator = resect_enum_constant_free;
    decl->data = data;
}

typedef struct P_resect_enum_data {
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

enum CXChildVisitResult resect_visit_enum_constant(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl_child_visit_data visit_data = data;
    resect_enum_data enum_data = visit_data->parent->data;

    resect_decl decl = resect_decl_create(visit_data->visit_context, visit_data->translation_context, cursor).decl;
    if (decl != NULL && decl->kind == RESECT_DECL_KIND_ENUM_CONSTANT) {
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

void resect_enum_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                      CXCursor cursor) {
    resect_enum_data data = malloc(sizeof(struct P_resect_enum_data));
    data->constants = resect_collection_create();
    CXType enum_type = clang_getEnumDeclIntegerType(cursor);

    data->type = resect_type_create(visit_context, context, enum_type);
    decl->data_deallocator = resect_enum_data_free;
    decl->data = data;

    struct P_resect_decl_child_visit_data visit_data = {
        .visit_context = visit_context, .translation_context = context, .parent = decl
    };

    clang_visitChildren(cursor, resect_visit_enum_constant, &visit_data);
}

/*
 * VARIABLE
 */
typedef struct P_resect_variable_data {
    resect_variable_kind kind;
    resect_string string_value;
    long long int_value;
    double float_value;
    resect_storage_class storage_class;
} *resect_variable_data;

resect_type resect_variable_get_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_VARIABLE);
    return decl->type;
}

resect_variable_kind resect_variable_get_kind(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_VARIABLE);
    resect_variable_data data = decl->data;
    return data->kind;
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

resect_storage_class resect_variable_get_storage_class(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_VARIABLE);
    resect_variable_data data = decl->data;
    return data->storage_class;
}

void resect_variable_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }

    resect_variable_data var_data = data;
    resect_string_free(var_data->string_value);

    free(data);
}

void resect_variable_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                          CXCursor cursor) {
    CXEvalResult value = clang_Cursor_Evaluate(cursor);
    resect_variable_data data = malloc(sizeof(struct P_resect_variable_data));

    data->storage_class = convert_storage_class(clang_Cursor_getStorageClass(cursor));
    data->string_value = resect_string_from_c("");

    switch (clang_EvalResult_getKind(value)) {
        case CXEval_Int:
            data->kind = RESECT_VARIABLE_TYPE_INT;
            data->int_value = clang_EvalResult_getAsLongLong(value);
            break;
        case CXEval_Float:
            data->kind = RESECT_VARIABLE_TYPE_FLOAT;
            data->float_value = clang_EvalResult_getAsDouble(value);
            break;
        case CXEval_CFStr:
        case CXEval_ObjCStrLiteral:
        case CXEval_StrLiteral:
            data->kind = RESECT_VARIABLE_TYPE_STRING;
            resect_string_update(data->string_value, clang_EvalResult_getAsStr(value));
            break;
        case CXEval_Other:
            data->kind = RESECT_VARIABLE_TYPE_OTHER;
            resect_string_update(data->string_value, clang_EvalResult_getAsStr(value));
        default:
            data->kind = RESECT_VARIABLE_TYPE_UNKNOWN;
    }

    decl->data = data;
    decl->data_deallocator = resect_variable_data_free;

    clang_EvalResult_dispose(value);
}

/*
 * MACRO
 */
typedef struct P_resect_macro_data {
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

void resect_macro_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                       CXCursor cursor) {
    resect_macro_data data = malloc(sizeof(struct P_resect_macro_data));

    data->is_function_like = clang_Cursor_isMacroFunctionLike(cursor) != 0 ? resect_true : resect_false;

    decl->data_deallocator = resect_macro_data_free;
    decl->data = data;
}

/*
 * METHOD
 */
typedef struct P_resect_method_data {
    resect_function_data function_data;
    resect_bool pure_virtual;
    resect_bool virtual;
    resect_bool non_mutating;
    resect_bool deleted;
} *resect_method_data;

resect_type resect_method_get_result_type(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->result_type;
}

resect_storage_class resect_method_get_storage_class(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->storage_class;
}

resect_bool resect_method_is_variadic(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->variadic;
}

resect_bool resect_method_is_pure_virtual(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->pure_virtual;
}

resect_bool resect_method_is_virtual(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->virtual;
}

resect_bool resect_method_is_const(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->non_mutating;
}

resect_bool resect_method_is_deleted(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->deleted;
}

resect_collection resect_method_parameters(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_METHOD);
    resect_method_data data = decl->data;
    return data->function_data->parameters;
}

enum CXChildVisitResult resect_visit_method_parameter(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_decl_child_visit_data visit_data = data;
    resect_method_data method_data = visit_data->parent->data;

    resect_visit_function_child(visit_data, cursor, method_data->function_data);

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

void resect_method_init(resect_visit_context visit_context, resect_translation_context context, resect_decl decl,
                        CXCursor cursor) {
    resect_method_data data = malloc(sizeof(struct P_resect_method_data));

    data->function_data = resect_function_data_create(visit_context, context, cursor);

    data->virtual = clang_CXXMethod_isVirtual(cursor) > 0;
    data->pure_virtual = clang_CXXMethod_isPureVirtual(cursor) > 0;
    data->non_mutating = clang_CXXMethod_isConst(cursor) > 0;
    data->deleted = clang_CXXMethod_isDeleted(cursor) > 0;

    decl->data_deallocator = resect_method_data_free;
    decl->data = data;

    struct P_resect_decl_child_visit_data visit_data = {
        .visit_context = visit_context, .translation_context = context, .parent = decl
    };

    clang_visitChildren(cursor, resect_visit_method_parameter, &visit_data);
}

/*
 * TEMPLATE PARAMETER
 */
typedef struct P_resect_template_parameter_data {
    resect_template_parameter_kind kind;
} *resect_template_parameter_data;

void resect_template_parameter_data_free(void *data, resect_set deallocated) {
    if (data == NULL || !resect_set_add(deallocated, data)) {
        return;
    }
    free(data);
}

resect_template_parameter_kind convert_template_parameter_kind(enum CXCursorKind kind) {
    switch (kind) {
        case CXCursor_TemplateTemplateParameter:
            return RESECT_TEMPLATE_PARAMETER_KIND_TEMPLATE;
        case CXCursor_TemplateTypeParameter:
            return RESECT_TEMPLATE_PARAMETER_KIND_TYPE;
        case CXCursor_NonTypeTemplateParameter:
            return RESECT_TEMPLATE_PARAMETER_KIND_NON_TYPE;
        default:
            return RESECT_TEMPLATE_PARAMETER_KIND_UNKNOWN;
    }
}

void resect_template_parameter_init(resect_visit_context visit_context, resect_translation_context context,
                                    resect_decl decl, CXCursor cursor) {
    resect_template_parameter_data data = malloc(sizeof(struct P_resect_template_parameter_data));

    data->kind = convert_template_parameter_kind(clang_getCursorKind(cursor));

    decl->data_deallocator = resect_template_parameter_data_free;
    decl->data = data;

    resect_register_template_parameter(context, decl->name, decl);
}

resect_template_parameter_kind resect_template_parameter_get_kind(resect_decl decl) {
    assert(decl->kind == RESECT_DECL_KIND_TEMPLATE_PARAMETER);
    resect_template_parameter_data data = decl->data;
    return data->kind;
}
