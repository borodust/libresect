#include "../resect.h"
#include "resect_private.h"

#include <stdlib.h>
#include <string.h>

/*
 * TRANSLATION CONTEXT
 */
struct resect_translation_context {
    resect_set exposed_decls;
    resect_table decl_table;
    resect_table template_parameter_table;
    resect_language language;
};

resect_translation_context resect_context_create() {
    resect_translation_context context = malloc(sizeof(struct resect_translation_context));
    context->exposed_decls = resect_set_create();
    context->decl_table = resect_table_create();
    context->template_parameter_table = resect_table_create();
    context->language = RESECT_LANGUAGE_UNKNOWN;

    return context;
}

void resect_decl_table_free(void *context, void *value) {
    resect_set deallocated = context;
    resect_decl decl = value;

    resect_decl_free(decl, deallocated);
}

void resect_context_free(resect_translation_context context, resect_set deallocated) {
    if (!resect_set_add(deallocated, context)) {
        return;
    }

    resect_table_free(context->decl_table, resect_decl_table_free, deallocated);
    resect_table_free(context->template_parameter_table, NULL, NULL);

    resect_set_free(context->exposed_decls);
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