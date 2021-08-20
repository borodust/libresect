#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * TRANSLATION CONTEXT
 */
struct P_resect_translation_context {
    resect_set exposed_decls;
    resect_table decl_table;
    resect_table template_parameter_table;
    resect_language language;
    resect_filtering_context filtering;

    resect_collection state_stack;
    resect_collection garbage;
};

struct P_resect_garbage {
    enum P_resect_garbage_kind kind;
    void* data;
};

resect_translation_context resect_context_create(resect_parse_options opts) {
    resect_translation_context context = malloc(sizeof(struct P_resect_translation_context));
    context->exposed_decls = resect_set_create();
    context->decl_table = resect_table_create();
    context->template_parameter_table = resect_table_create();
    context->language = RESECT_LANGUAGE_UNKNOWN;

    context->filtering = resect_filtering_context_create(opts);

    context->state_stack = resect_collection_create();

    context->garbage = resect_collection_create();

    return context;
}

void resect_decl_table_free(void *context, void *value) {
    resect_set deallocated = context;
    resect_decl decl = value;

    resect_decl_free(decl, deallocated);
}

static void free_garbage_collection(resect_collection garbage_collection, resect_set deallocated) {
    resect_iterator iter = resect_collection_iterator(garbage_collection);
    while (resect_iterator_next(iter)) {
        struct P_resect_garbage *garbage = resect_iterator_value(iter);
        switch (garbage->kind) {
            case RESECT_GARBAGE_KIND_TEMPLATE_ARGUMENT:
                resect_template_argument_free(garbage->data, deallocated);
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

    assert(resect_collection_size(context->state_stack) == 0);
    resect_collection_free(context->state_stack);

    resect_filtering_context_free(context->filtering);

    resect_table_free(context->decl_table, resect_decl_table_free, deallocated);
    resect_table_free(context->template_parameter_table, NULL, NULL);

    resect_set_free(context->exposed_decls);

    free_garbage_collection(context->garbage, deallocated);

    free(context);
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

static resect_string format_cursor_full_name(CXCursor cursor) {
    if (clang_Cursor_isNull(cursor) || clang_getCursorKind(cursor) == CXCursor_TranslationUnit) {
        return resect_string_from_c("");
    }

    CXCursor parent = clang_getCursorSemanticParent(cursor);

    if (clang_Cursor_isAnonymousRecordDecl(cursor) || clang_Cursor_isAnonymous(cursor)) {
        return format_cursor_full_name(parent);
    }

    enum CXCursorKind parent_kind = clang_getCursorKind(parent);
    switch (parent_kind) {
        case CXCursor_ClassDecl:
        case CXCursor_ClassTemplate:
        case CXCursor_ClassTemplateSpecialization:
        case CXCursor_ClassTemplatePartialSpecialization:
        case CXCursor_UnionDecl:
        case CXCursor_EnumDecl:
        case CXCursor_StructDecl: {
            resect_string parent_full_name = format_cursor_full_name(parent);
            resect_string name = resect_string_from_clang(clang_getCursorSpelling(cursor));

            resect_string full_name = resect_string_format("%s::%s",
                                                           resect_string_to_c(parent_full_name),
                                                           resect_string_to_c(name));

            resect_string_free(name);
            resect_string_free(parent_full_name);

            return full_name;
        }
    }

    resect_string full_name = resect_string_from_c("");

    {
        resect_string namespace = resect_format_cursor_namespace(cursor);
        if (resect_string_length(namespace) > 0) {
            resect_string_append(full_name, resect_string_to_c(namespace));
            resect_string_append(full_name, "::");
        }
        resect_string_free(namespace);
    }

    {
        resect_string name = resect_string_from_clang(clang_getCursorSpelling(cursor));
        resect_string_append(full_name, resect_string_to_c(name));
        resect_string_free(name);
    }

    return full_name;
}

static resect_string format_cursor_source(CXCursor cursor) {
    resect_location location = resect_location_from_cursor(cursor);
    resect_string source = resect_string_from_c(resect_location_name(location));
    resect_location_free(location);

    return source;
}

resect_inclusion_status resect_cursor_inclusion_status(resect_translation_context context, CXCursor cursor) {
    resect_string full_name = format_cursor_full_name(cursor);
    resect_string source = format_cursor_source(cursor);

    resect_inclusion_status result = resect_filtering_explicit_inclusion_status(context->filtering,
                                                                                resect_string_to_c(full_name),
                                                                                resect_string_to_c(source));

    resect_string_free(full_name);
    resect_string_free(source);

    return result;
}

resect_inclusion_status resect_context_inclusion_status(resect_translation_context context) {
    return resect_filtering_inclusion_status(context->filtering);
}

void resect_context_push_inclusion_status(resect_translation_context context, resect_inclusion_status status) {
    resect_filtering_push_inclusion_status(context->filtering, status);
}

resect_inclusion_status resect_context_pop_inclusion_status(resect_translation_context context) {
    return resect_filtering_pop_inclusion_status(context->filtering);
}

void resect_register_decl(resect_translation_context context, resect_string decl_id, resect_decl decl) {
    resect_table_put_if_absent(context->decl_table, resect_string_to_c(decl_id), decl);
}

resect_decl resect_find_decl(resect_translation_context context, resect_string decl_id) {
    return resect_table_get(context->decl_table, resect_string_to_c(decl_id));
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

enum CXChildVisitResult resect_visit_context_child(CXCursor cursor,
                                                   CXCursor parent,
                                                   CXClientData data) {
    resect_translation_context context = data;
    resect_visit_child_declaration(cursor, parent, context);
    resect_context_flush_template_parameters(context);
    return CXChildVisit_Continue;
}

void *resect_context_current_state(resect_translation_context context) {
    return resect_collection_peek_last(context->state_stack);
}

void resect_context_push_state(resect_translation_context context, void *value) {
    resect_collection_add(context->state_stack, value);
}

void *resect_context_pop_state(resect_translation_context context) {
    return resect_collection_pop_last(context->state_stack);
}

void resect_register_garbage(resect_translation_context context, enum P_resect_garbage_kind kind, void* garbage) {
    struct P_resect_garbage* garbage_holder = malloc(sizeof (struct P_resect_garbage));

    garbage_holder->kind = kind;
    garbage_holder->data = garbage;

    resect_collection_add(context->garbage, garbage_holder);
}