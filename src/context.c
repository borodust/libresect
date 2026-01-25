#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * TYPE REGISTRY
 */

typedef struct P_resect_type_stack_value {
    CXType clang_type;
    resect_type resect_type;
} P_resect_type_stack_value;

typedef struct P_resect_type_registry {
    resect_table type_stack_table;
} *resect_type_registry;


resect_type_registry resect_type_registry_create() {
    resect_type_registry registry = malloc(sizeof(struct P_resect_type_registry));
    registry->type_stack_table = resect_table_create();
    return registry;
}

void resect_type_registry_table_stack_destructor(void *context, void *value) {
    resect_collection type_stack = value;
    resect_iterator iter = resect_collection_iterator(type_stack);
    while (resect_iterator_next(iter)) {
        P_resect_type_stack_value *stack = resect_iterator_value(iter);
        free(stack);
    }
    resect_iterator_free(iter);
    resect_collection_free(type_stack);
}

/**
 * This doesn't free registered resect types
 * @param registry
 */
void resect_type_registry_free(resect_type_registry registry) {
    resect_table_free(registry->type_stack_table, resect_type_registry_table_stack_destructor, NULL);
    free(registry);
}

bool resect_type_registry_add(resect_type_registry registry, resect_string type_fqn,
                              CXType clang_type,
                              resect_type resect_type) {
    bool result = false;
    const char *key = resect_string_to_c(type_fqn);

    resect_collection type_stack = resect_table_get(registry->type_stack_table, key);
    if (type_stack == NULL) {
        type_stack = resect_collection_create();
        resect_table_put(registry->type_stack_table, key, type_stack);
    }

    resect_iterator iter = resect_collection_iterator(type_stack);
    while (resect_iterator_next(iter)) {
        P_resect_type_stack_value *stack = resect_iterator_value(iter);
        if (clang_equalTypes(stack->clang_type, clang_type)) {
            goto done;
        }
    }

    P_resect_type_stack_value* new_value = malloc(sizeof(P_resect_type_stack_value));
    new_value->clang_type = clang_type;
    new_value->resect_type = resect_type;
    resect_collection_add(type_stack, new_value);
    result = true;

done:
    resect_iterator_free(iter);
    return result;
}

resect_type resect_type_registry_find(resect_type_registry registry, resect_string type_fqn, CXType clang_type) {
    resect_type result = NULL;
    const char *key = resect_string_to_c(type_fqn);

    resect_collection type_stack = resect_table_get(registry->type_stack_table, key);
    if (type_stack == NULL) {
        return NULL;
    }

    resect_iterator iter = resect_collection_iterator(type_stack);
    while (resect_iterator_next(iter)) {
        P_resect_type_stack_value *stack = resect_iterator_value(iter);
        if (clang_equalTypes(stack->clang_type, clang_type)) {
            result = stack->resect_type;
            goto done;
        }
    }

done:
    resect_iterator_free(iter);
    return result;
}

/*
 * TRANSLATION CONTEXT
*/
struct P_resect_translation_context {
    resect_set exposed_decls;
    resect_table decl_table;
    resect_type_registry type_registry;
    resect_table template_parameter_table;
    resect_language language;

    resect_inclusion_registry inclusion_registry;

    resect_collection garbage;

    CXPrintingPolicy printing_policy;

    resect_pattern decl_name_pattern;
};

struct P_resect_garbage {
    enum P_resect_garbage_kind kind;
    void *data;
};

resect_translation_context resect_context_create(resect_parse_options opts,
                                                 resect_inclusion_registry registry) {
    resect_translation_context context = malloc(sizeof(struct P_resect_translation_context));
    context->exposed_decls = resect_set_create();
    context->decl_table = resect_table_create();
    context->type_registry = resect_type_registry_create();
    context->template_parameter_table = resect_table_create();
    context->language = RESECT_LANGUAGE_UNKNOWN;

    context->inclusion_registry = registry;

    context->garbage = resect_collection_create();
    context->printing_policy = NULL;

    context->decl_name_pattern = resect_pattern_create_c("^operator.+|[~\\w]+");

    return context;
}

void resect_decl_table_free(void *context, void *value) {
    resect_set deallocated = context;
    resect_decl decl = value;

    resect_decl_free(decl, deallocated);
}

void resect_type_table_free(void *context, void *value) {
    resect_set deallocated = context;
    resect_type type = value;

    resect_type_free(type, deallocated);
}

static void free_garbage_collection(resect_collection garbage_collection, resect_set deallocated) {
    resect_iterator iter = resect_collection_iterator(garbage_collection);
    while (resect_iterator_next(iter)) {
        struct P_resect_garbage *garbage = resect_iterator_value(iter);
        switch (garbage->kind) {
            case RESECT_GARBAGE_KIND_TEMPLATE_ARGUMENT:
                resect_template_argument_free(garbage->data, deallocated);
                break;
            case RESECT_GARBAGE_KIND_DECL:
                resect_decl_free(garbage->data, deallocated);
                break;
            case RESECT_GARBAGE_KIND_TYPE:
                resect_type_free(garbage->data, deallocated);
                break;
            default:
                // FIXME: add better error reporting
                assert(!"Unexpected garbage kind");
        }
    }
    resect_iterator_free(iter);
    resect_collection_free(garbage_collection);
}

void resect_context_free(resect_translation_context context, resect_set deallocated) {
    if (!resect_set_add(deallocated, context)) {
        return;
    }

    resect_table_free(context->decl_table, resect_decl_table_free, deallocated);
    resect_type_registry_free(context->type_registry);
    resect_table_free(context->template_parameter_table, NULL, NULL);

    resect_set_free(context->exposed_decls);

    free_garbage_collection(context->garbage, deallocated);

    resect_pattern_free(context->decl_name_pattern);

    free(context);
}

bool resect_is_decl_included(resect_translation_context context, resect_string decl_id) {
    return resect_inclusion_registry_decl_included(context->inclusion_registry, resect_string_to_c(decl_id));
}

void resect_export_decl(resect_translation_context context, resect_decl decl) {
    resect_set_add(context->exposed_decls, decl);
}

void resect_register_decl_language(resect_translation_context context, resect_language language) {
    if (context->language == RESECT_LANGUAGE_UNKNOWN
        || context->language == RESECT_LANGUAGE_C && language != RESECT_LANGUAGE_C) {
        context->language = language;
    }
}

resect_language resect_get_assumed_language(resect_translation_context context) {
    return context->language;
}

resect_string resect_format_cursor_source(CXCursor cursor) {
    resect_location location = resect_location_from_cursor(cursor);
    resect_string source = resect_string_from_c(resect_location_name(location));
    resect_location_free(location);

    return source;
}

void resect_register_decl(resect_translation_context context, resect_string decl_id, resect_decl decl) {
    resect_table_put_if_absent(context->decl_table, resect_string_to_c(decl_id), decl);
}

resect_decl resect_find_decl(resect_translation_context context, resect_string decl_id) {
    return resect_table_get(context->decl_table, resect_string_to_c(decl_id));
}

bool resect_register_type(resect_translation_context context, CXType clang_type, resect_type resect_type) {
    resect_string fqn = resect_string_fqn_from_type(context, clang_type);
    bool result = resect_type_registry_add(context->type_registry, fqn, clang_type, resect_type);
    resect_string_free(fqn);
    return result;
}

resect_type resect_find_type(resect_translation_context context, CXType clang_type) {
    resect_string fqn = resect_string_fqn_from_type(context, clang_type);
    resect_type result = resect_type_registry_find(context->type_registry, fqn, clang_type);
    resect_string_free(fqn);
    return result;
}

void resect_register_template_parameter(resect_translation_context context, resect_string name, resect_decl decl) {
    resect_table_put_if_absent(context->template_parameter_table, resect_string_to_c(name), decl);
}

resect_decl resect_find_template_parameter(resect_translation_context context, resect_string name) {
    return resect_table_get(context->template_parameter_table, resect_string_to_c(name));
}

void resect_context_flush_template_parameters(resect_translation_context context) {
    resect_table_free(context->template_parameter_table, NULL, NULL);
    context->template_parameter_table = resect_table_create();
}

resect_collection resect_create_decl_collection(resect_translation_context context) {
    resect_collection collection = resect_collection_create();
    resect_set_add_to_collection(context->exposed_decls, collection);
    return collection;
}

void resect_context_init_printing_policy(resect_translation_context context, CXCursor cursor) {
    assert(context->printing_policy == NULL);
    context->printing_policy = clang_getCursorPrintingPolicy(cursor);
    clang_PrintingPolicy_setProperty(context->printing_policy,
                                     CXPrintingPolicy_FullyQualifiedName,
                                     resect_true);
}

void resect_context_release_printing_policy(resect_translation_context context) {
    if (context->printing_policy) {
        clang_PrintingPolicy_dispose(context->printing_policy);
        context->printing_policy = NULL;
    }
}

CXPrintingPolicy resect_context_get_printing_policy(resect_translation_context context) {
    return context->printing_policy;
}

void resect_register_garbage(resect_translation_context context, enum P_resect_garbage_kind kind, void *garbage) {
    struct P_resect_garbage *garbage_holder = malloc(sizeof(struct P_resect_garbage));

    garbage_holder->kind = kind;
    garbage_holder->data = garbage;

    resect_collection_add(context->garbage, garbage_holder);
}

bool resect_context_extract_valid_decl_name(resect_translation_context context,
                                            resect_string name,
                                            resect_string out) {
    return resect_pattern_find(context->decl_name_pattern, name, out);
}

/*
 * COMMON VISIT
*/
typedef struct P_resect_visit_context {
    resect_declaration_visitor visitor;
} *resect_visit_context;

typedef struct {
    resect_visit_context context;
    void *data;
} resect_child_visitor_data;

void resect_visit_cursor_for_declaration(resect_visit_context context, CXCursor cursor, void *data);

enum CXChildVisitResult resect_visit_cursor_child_for_declaration(CXCursor cursor,
                                                                  CXCursor parent,
                                                                  CXClientData data);

resect_visit_context resect_visit_context_create(resect_declaration_visitor visitor) {
    resect_visit_context context = malloc(sizeof(struct P_resect_visit_context));
    context->visitor = visitor;
    return context;
}

void resect_visit_context_free(resect_visit_context context) {
    free(context);
}

void resect_visit_cursor_children(resect_visit_context context, CXCursor cursor, void *data) {
    resect_child_visitor_data visitor_data = {.context = context, .data = data};
    clang_visitChildren(cursor, resect_visit_cursor_child_for_declaration, &visitor_data);
}

void resect_visit_cursor(resect_visit_context context, CXCursor cursor, void *data) {
    resect_visit_cursor_for_declaration(context, cursor, data);
}

enum CXChildVisitResult resect_visit_cursor_child_for_declaration(CXCursor cursor,
                                                                  CXCursor parent,
                                                                  CXClientData data) {
    resect_child_visitor_data *visitor_data = data;
    resect_visit_cursor_for_declaration(visitor_data->context, cursor, visitor_data->data);
    return CXChildVisit_Continue;
}

CXCursor resect_find_declaration_owning_cursor(CXCursor cursor) {
    if (clang_Cursor_isNull(cursor)) {
        return cursor;
    }

    CXCursor parent = clang_getCursorSemanticParent(cursor);
    switch (convert_cursor_kind(parent)) {
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_UNION:
        case RESECT_DECL_KIND_CLASS:
            return parent;
        default: ;
    }

    return resect_find_declaration_owning_cursor(parent);
}

void resect_visit_cursor_for_declaration(resect_visit_context context, CXCursor cursor, void *data) {
    if (clang_Cursor_isNull(cursor) || cursor.kind == CXCursor_NoDeclFound) {
        return;
    }

    resect_child_visitor_data visitor_data = {.context = context, .data = data};

    enum CXCursorKind cursor_kind = clang_getCursorKind(cursor);

    if (cursor_kind >= CXCursor_FirstInvalid // ignore invalid cursors
        && cursor_kind <= CXCursor_LastInvalid
        || cursor_kind >= CXCursor_FirstAttr // ignore attributes
        && cursor_kind <= CXCursor_LastAttr
        || cursor_kind >= CXCursor_FirstRef // ignore references
        && cursor_kind <= CXCursor_LastRef
        || cursor_kind >= CXCursor_FirstStmt // ignore statements
        && cursor_kind <= CXCursor_LastStmt
        || cursor_kind >= CXCursor_FirstExpr // ignore expressions
        && cursor_kind <= CXCursor_LastExpr
        || cursor_kind == CXCursor_LinkageSpec) {
        /* ignore some kinds for now, but just in case check children */
        clang_visitChildren(cursor, resect_visit_cursor_child_for_declaration, &visitor_data);
        return;
    }

    if (resect_is_forward_declaration(cursor)) {
        CXCursor definition = clang_getCursorDefinition(cursor);
        if ((clang_getCursorKind(definition) < CXCursor_FirstInvalid
             || clang_getCursorKind(definition) > CXCursor_LastInvalid)
            && !clang_equalCursors(cursor, definition)) {
            resect_visit_cursor_for_declaration(context, definition, data);
            return;
        }
    }

    switch (cursor_kind) {
        case CXCursor_FieldDecl:
        case CXCursor_ParmDecl:
        case CXCursor_TemplateTemplateParameter:
        case CXCursor_TemplateTypeParameter:
        case CXCursor_NonTypeTemplateParameter: {
            CXCursor parent = clang_getCursorSemanticParent(cursor);
            if (clang_getCursorKind(parent) == CXCursor_TranslationUnit) {
                /* the hell is that even, but that happens, lets check children though */
                clang_visitChildren(cursor, resect_visit_cursor_child_for_declaration, &visitor_data);
                return;
            }
        }
        default: ;
    }

    switch (cursor_kind) {
        case CXCursor_Namespace:
        case CXCursor_UnexposedDecl:
        case CXCursor_MacroExpansion:
        case CXCursor_InclusionDirective:
            /* we might encounter exposed declarations within unexposed one, e.g. inside extern "C"/"C++" block */
            clang_visitChildren(cursor, resect_visit_cursor_child_for_declaration, &visitor_data);
            return;
        default: ;
    }

    context->visitor(context, cursor, data);
}
