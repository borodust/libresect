#include <assert.h>
#include <stdlib.h>

#include "resect_private.h"

/*
 * DECL GRAPH
 */
typedef struct P_resect_decl_graph_edge {
    resect_string id;
} *resect_decl_graph_edge;

typedef struct P_resect_decl_graph_node {
    resect_string id;
    resect_filter_status filter_status;
    resect_table /*resect_string*/ parents;

    resect_table /*resect_decl_graph_edge*/ edges;
} *resect_decl_graph_node;

typedef struct P_resect_decl_graph {
    resect_table node_table;
} *resect_decl_graph;

//
// EDGE
//
static resect_decl_graph_edge resect_decl_graph_edge_create(resect_string id) {
    resect_decl_graph_edge edge = malloc(sizeof(struct P_resect_decl_graph_edge));
    edge->id = resect_string_copy(id);
    return edge;
}

static void resect_decl_graph_edge_free(resect_decl_graph_edge edge) {
    resect_string_free(edge->id);
    free(edge);
}

//
// NODE
//
static resect_decl_graph_node resect_decl_graph_node_create(resect_string id) {
    resect_decl_graph_node node = malloc(sizeof(struct P_resect_decl_graph_node));
    node->id = resect_string_copy(id);
    node->filter_status = RESECT_FILTER_STATUS_IGNORED;
    node->parents = resect_table_create();
    node->edges = resect_table_create();
    return node;
}

static resect_decl_graph_edge resect_decl_graph_node_find_edge(resect_decl_graph_node node, resect_string id) {
    return resect_table_get(node->edges, resect_string_to_c(id));
}

static bool resect_decl_graph_node_add_edge(resect_decl_graph_node node, resect_string target_id) {
    if (resect_decl_graph_node_find_edge(node, target_id) != NULL) {
        return false;
    }

    resect_decl_graph_edge edge = resect_decl_graph_edge_create(target_id);
    resect_table_put(node->edges, resect_string_to_c(target_id), edge);

    return true;
}

static void edge_table_value_free(void *ctx, void *edge) { resect_decl_graph_edge_free(edge); }

static void resect_decl_graph_node_free(resect_decl_graph_node node) {
    resect_string_free(node->id);
    resect_table_free(node->parents, NULL, NULL);
    resect_table_free(node->edges, edge_table_value_free, NULL);
    free(node);
}

static bool resect_decl_graph_node_add_parent(resect_decl_graph_node node, resect_string id) {
    return resect_table_put_if_absent(node->parents, resect_string_to_c(id), (void *) resect_string_to_c(id));
}

//
// GRAPH
//
static resect_decl_graph resect_decl_graph_create() {
    resect_decl_graph graph = malloc(sizeof(struct P_resect_decl_graph));
    graph->node_table = resect_table_create();
    return graph;
}

static resect_bool resect_table_node_free(void *context, const char *key, void *node) {
    resect_decl_graph_node_free(node);
    return false;
}

static void resect_decl_graph_free(resect_decl_graph graph) {
    resect_visit_table(graph->node_table, resect_table_node_free, NULL);

    free(graph);
}

static bool resect_decl_graph_add_node(resect_decl_graph graph, resect_string id, resect_filter_status filter_status) {
    const char *key = resect_string_to_c(id);
    // ReSharper disable once CppDFANullDereference
    if (resect_table_get(graph->node_table, key) != NULL) {
        return false;
    }

    resect_decl_graph_node node = resect_decl_graph_node_create(id);
    node->filter_status = filter_status;

    resect_table_put(graph->node_table, key, node);
    return true;
}

static resect_decl_graph_node resect_decl_graph__find_node_c(resect_decl_graph graph, const char *id) {
    return resect_table_get(graph->node_table, id);
}

static resect_decl_graph_node resect_decl_graph__find_node(resect_decl_graph graph, resect_string id) {
    return resect_decl_graph__find_node_c(graph, resect_string_to_c(id));
}

static bool resect_decl_graph_has_node(resect_decl_graph graph, resect_string id) {
    return resect_decl_graph__find_node(graph, id) != NULL;
}

static bool resect_decl_graph__add_edge(resect_decl_graph graph, resect_string source_id, resect_string target_id) {
    resect_decl_graph_node node = resect_decl_graph__find_node(graph, source_id);
    if (node == NULL) {
        return false;
    }
    if (resect_decl_graph_node_find_edge(node, target_id) != NULL) {
        return false;
    }

    return resect_decl_graph_node_add_edge(node, target_id);
}


static bool resect_decl_graph_adopt(resect_decl_graph graph, resect_string parent_id, resect_string source_id) {
    resect_decl_graph_node node = resect_decl_graph__find_node(graph, source_id);
    resect_decl_graph_node parent = resect_decl_graph__find_node(graph, parent_id);

    if (node == NULL || parent == NULL) {
        return false;
    }

    if (!resect_decl_graph_node_add_parent(node, parent_id)) {
        return false;
    }

    return resect_decl_graph__add_edge(graph, parent->id, node->id);
}

/*
 * SHAKING CONTEXT
 */
typedef struct P_resect_shaking_context {
    resect_filtering_context filtering;
    resect_decl_graph decl_graph;
    resect_string root_decl_id;
    resect_collection /*resect_string*/ bound_parents; // reversed edges, not semantic decl parents

    resect_bool print_diagnostics;
} *resect_shaking_context;

resect_shaking_context resect_shaking_context_create(resect_parse_options opts) {
    resect_shaking_context context = malloc(sizeof(struct P_resect_shaking_context));
    context->filtering = resect_filtering_context_create(opts);
    context->bound_parents = resect_collection_create();
    context->decl_graph = resect_decl_graph_create();

    context->root_decl_id = resect_string_from_c("");

    resect_collection_add(context->bound_parents, resect_string_copy(context->root_decl_id));

    resect_decl_graph_add_node(context->decl_graph, context->root_decl_id, RESECT_FILTER_STATUS_INCLUDED);

    context->print_diagnostics = resect_options_is_diagnostics_enabled(opts);
    return context;
}

void resect_shaking_context_free(resect_shaking_context context) {
    resect_filtering_context_free(context->filtering);
    resect_string_free(context->root_decl_id);
    resect_string_collection_free(context->bound_parents);
    resect_decl_graph_free(context->decl_graph);
    free(context);
}

void resect_shaking_context_push_decl_link(resect_shaking_context ctx, resect_string decl_id) {
    resect_collection_add(ctx->bound_parents, resect_string_copy(decl_id));
}

resect_string resect_shaking_context_root_id(resect_shaking_context ctx) { return ctx->root_decl_id; }

void resect_shaking_context_push_root_link(resect_shaking_context ctx) {
    resect_shaking_context_push_decl_link(ctx, resect_shaking_context_root_id(ctx));
}

void resect_shaking_context_pop_link(resect_shaking_context ctx) {
    resect_string decl_id = resect_collection_pop_last(ctx->bound_parents);
    assert(decl_id != NULL && "Failed to pop parent: no more registered");
    resect_string_free(decl_id);
}

resect_string resect_shaking_context_decl_parent_id(resect_shaking_context ctx) {
    return resect_collection_peek_last(ctx->bound_parents);
}

static void resect_investigate_type(resect_visit_context visit_context, resect_shaking_context context, CXType type);

static void resect_investigate_decl(resect_visit_context visit_context, resect_shaking_context shaking_context,
                                    CXCursor cursor);

static void resect_shaking_context__init_registry_table(resect_shaking_context shaking_context, resect_table registry);

/*
 * DECLARATION
 */
void resect_decl_shake(resect_visit_context visit_context, CXCursor cursor, void *data) {
    resect_shaking_context shaking_context = data;
    resect_investigate_decl(visit_context, shaking_context, cursor);
}

static resect_filter_status resect_cursor_filter_status(resect_filtering_context filtering, CXCursor cursor) {
    resect_string full_name = resect_format_cursor_full_name(cursor);
    resect_string source = resect_format_cursor_source(cursor);

    resect_filter_status result =
            resect_filtering_status(filtering, resect_string_to_c(full_name), resect_string_to_c(source));

    resect_string_free(full_name);
    resect_string_free(source);

    return result;
}

static void resect_investigate_record(resect_visit_context visit_context, resect_shaking_context context,
                                      CXCursor cursor);

static void resect_investigate_enum(resect_visit_context visit_context, resect_shaking_context context,
                                    CXCursor cursor);

static void resect_investigate_function_type(resect_visit_context visit_context, resect_shaking_context context,
                                             CXType type);

static void resect_investigate_function(resect_visit_context visit_context, resect_shaking_context context,
                                        CXCursor cursor);

static void resect_investigate_typedef(resect_visit_context visit_context, resect_shaking_context context,
CXCursor cursor);

static void resect_investigate_parameter(resect_visit_context visit_context, resect_shaking_context context,
CXCursor cursor);

static void resect_investigate_field(resect_visit_context visit_context, resect_shaking_context context,
                                         CXCursor cursor);

static void resect_maybe_investigate_template(resect_visit_context visit_context, resect_shaking_context context,
                                              CXCursor cursor);

static void resect_investigate_template_parameter(resect_visit_context visit_context, resect_shaking_context context,
CXCursor cursor);


void resect_investigate_decl(resect_visit_context visit_context, resect_shaking_context shaking_context,
                             CXCursor cursor) {
    resect_string decl_id = resect_extract_decl_id(cursor);
    resect_decl_kind decl_kind = convert_cursor_kind(cursor);

    resect_string parent_id = resect_shaking_context_decl_parent_id(shaking_context);

    resect_filter_status filter_status = resect_cursor_filter_status(shaking_context->filtering, cursor);

    bool node_existed = resect_decl_graph_has_node(shaking_context->decl_graph, decl_id);
    if (!node_existed) {
        resect_decl_graph_add_node(shaking_context->decl_graph, decl_id, filter_status);

        resect_decl_graph_adopt(shaking_context->decl_graph, resect_shaking_context_root_id(shaking_context), decl_id);
    }

    resect_decl_graph_adopt(shaking_context->decl_graph, parent_id, decl_id);

    resect_shaking_context_push_decl_link(shaking_context, decl_id);

    if (node_existed) {
        goto done;
    }

    switch (decl_kind) {
        case RESECT_DECL_KIND_STRUCT:
        case RESECT_DECL_KIND_CLASS:
        case RESECT_DECL_KIND_UNION:
            resect_investigate_record(visit_context, shaking_context, cursor);
            break;
        case RESECT_DECL_KIND_ENUM:
            resect_investigate_enum(visit_context, shaking_context, cursor);
            break;
        case RESECT_DECL_KIND_FUNCTION:
        case RESECT_DECL_KIND_METHOD:
            resect_investigate_function(visit_context, shaking_context, cursor);
            break;
        case RESECT_DECL_KIND_TYPEDEF:
            resect_investigate_typedef(visit_context, shaking_context, cursor);
            break;
        case RESECT_DECL_KIND_PARAMETER:
            resect_investigate_parameter(visit_context, shaking_context, cursor);
            break;
        case RESECT_DECL_KIND_FIELD:
            resect_investigate_field(visit_context, shaking_context, cursor);
            break;
        case RESECT_DECL_KIND_TEMPLATE_PARAMETER:
            resect_investigate_template_parameter(visit_context, shaking_context, cursor);
            break;
        default:;
    }

    resect_maybe_investigate_template(visit_context, shaking_context, cursor);

done:
    resect_string_free(decl_id);
    resect_shaking_context_pop_link(shaking_context);
}

typedef struct {
    resect_visit_context visit_context;
    resect_shaking_context shaking_context;
} resect_investigate_child_data;

static enum CXChildVisitResult resect_investigate_record_child(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_investigate_child_data *visit_data = data;
    enum CXCursorKind cursor_kind = clang_getCursorKind(cursor);

    switch (cursor_kind) {
        case CXCursor_Constructor:
        case CXCursor_Destructor:
            resect_visit_cursor(visit_data->visit_context, cursor, visit_data->shaking_context);
            return CXChildVisit_Continue;
        default:;
    }

    resect_shaking_context_push_root_link(visit_data->shaking_context);
    resect_visit_cursor(visit_data->visit_context, cursor, visit_data->shaking_context);
    resect_shaking_context_pop_link(visit_data->shaking_context);

    return CXChildVisit_Continue;
}

static void resect_investigate_record(resect_visit_context visit_context, resect_shaking_context shaking_context,
                                      CXCursor cursor) {
    resect_investigate_child_data visit_data = {.visit_context = visit_context, .shaking_context = shaking_context};
    clang_visitChildren(cursor, resect_investigate_record_child, &visit_data);
}

static enum CXChildVisitResult resect_investigate_enum_child(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_investigate_child_data *visit_data = data;
    resect_visit_cursor(visit_data->visit_context, cursor, visit_data->shaking_context);
    return CXChildVisit_Continue;
}

static void resect_investigate_enum(resect_visit_context visit_context, resect_shaking_context context,
                                    CXCursor cursor) {
    resect_shaking_context_push_root_link(context);
    CXType enum_type = clang_getEnumDeclIntegerType(cursor);
    resect_investigate_type(visit_context, context, enum_type);

    resect_investigate_child_data visit_data = {.visit_context = visit_context, .shaking_context = context};

    clang_visitChildren(cursor, resect_investigate_enum_child, &visit_data);
    resect_shaking_context_pop_link(context);
}

enum CXChildVisitResult resect_investigate_function_parameter(CXCursor cursor, CXCursor parent, CXClientData data) {
    resect_investigate_child_data *visit_data = data;
    if (clang_getCursorKind(cursor) == CXCursor_ParmDecl) {
        resect_visit_cursor(visit_data->visit_context, cursor, visit_data->shaking_context);
    }
    return CXChildVisit_Continue;
}

static void resect_investigate_function(resect_visit_context visit_context, resect_shaking_context context,
                                        CXCursor cursor) {
    resect_investigate_function_type(visit_context, context, clang_getCursorType(cursor));

    resect_investigate_child_data visit_data = {.visit_context = visit_context, .shaking_context = context};
    clang_visitChildren(cursor, resect_investigate_function_parameter, &visit_data);
}

static void resect_investigate_typedef(resect_visit_context visit_context, resect_shaking_context context,
                                       CXCursor cursor) {
    CXType canonical_type = clang_getCanonicalType(clang_getTypedefDeclUnderlyingType(cursor));
    resect_investigate_type(visit_context, context, canonical_type);
}

static void resect_investigate_parameter(resect_visit_context visit_context, resect_shaking_context context,
                                         CXCursor cursor) {
    resect_investigate_type(visit_context, context, clang_getCursorType(cursor));
}

static void resect_investigate_template_parameter(resect_visit_context visit_context, resect_shaking_context context,
                                         CXCursor cursor) {
    resect_investigate_type(visit_context, context, clang_getCursorType(cursor));
}

static void resect_investigate_field(resect_visit_context visit_context, resect_shaking_context context,
                                     CXCursor cursor) {
    resect_investigate_type(visit_context, context, clang_getCursorType(cursor));
}

void resect_investigate_template_arguments(resect_visit_context visit_context, resect_shaking_context context,
                                           CXCursor cursor) {
    int arg_count = clang_Cursor_getNumTemplateArguments(cursor);

    for (int i = 0; i < arg_count; ++i) {
        resect_template_argument_kind arg_kind =
                convert_template_argument_kind(clang_Cursor_getTemplateArgumentKind(cursor, i));

        switch (arg_kind) {
            case RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE_EXPANSION:
            case RESECT_TEMPLATE_ARGUMENT_KIND_TEMPLATE:
            case RESECT_TEMPLATE_ARGUMENT_KIND_TYPE:
            case RESECT_TEMPLATE_ARGUMENT_KIND_DECLARATION:
                resect_investigate_type(visit_context, context, clang_Cursor_getTemplateArgumentType(cursor, i));
                break;
            default:;
        }
    }
}

static void resect_maybe_investigate_template(resect_visit_context visit_context, resect_shaking_context context,
                                              CXCursor cursor) {
    if (resect_is_specialized(cursor)) {
        resect_investigate_template_arguments(visit_context, context, cursor);

        CXCursor template_cursor = clang_getSpecializedCursorTemplate(cursor);
        resect_visit_cursor(visit_context, template_cursor, context);
    }
}

/*
 * TYPE
 */

static void resect_investigate_function_type(resect_visit_context visit_context, resect_shaking_context context,
                                             CXType type) {
    resect_investigate_type(visit_context, context, clang_getResultType(type));

    int arg_count = clang_getNumArgTypes(type);
    for (int i = 0; i < arg_count; ++i) {
        CXType arg_type = clang_getArgType(type, i);
        resect_investigate_type(visit_context, context, arg_type);
    }
}

void resect_investigate_pointer_type(resect_visit_context visit_context, resect_shaking_context context, CXType type) {
    resect_investigate_type(visit_context, context, clang_getPointeeType(type));
}

void resect_investigate_array_type(resect_visit_context visit_context, resect_shaking_context context, CXType type) {
    resect_investigate_type(visit_context, context, clang_getArrayElementType(type));
}


void resect_investigate_template_args(resect_visit_context visit_context, resect_shaking_context context, CXType type) {
    int arg_count = clang_Type_getNumTemplateArguments(type);
    for (int i = 0; i < arg_count; ++i) {
        CXType clang_arg_type = clang_Type_getTemplateArgumentAsType(type, i);

        switch (clang_arg_type.kind) {
            case CXType_Invalid:
            case CXType_Unexposed:
                break;
            default:
                resect_investigate_type(visit_context, context, clang_arg_type);
                break;
        }
    }
}

static enum CXVisitorResult visit_type_item(CXCursor cursor, CXClientData data) {
    resect_investigate_child_data *visit_data = data;

    switch (clang_getCursorKind(cursor)) {
        case CXCursor_Constructor:
        case CXCursor_Destructor:
            resect_visit_cursor(visit_data->visit_context, cursor, visit_data->shaking_context);
            return CXVisit_Continue;
        default:;
    }

    resect_shaking_context_push_root_link(visit_data->shaking_context);
    resect_visit_cursor(visit_data->visit_context, cursor, visit_data->shaking_context);
    resect_shaking_context_pop_link(visit_data->shaking_context);
    return CXVisit_Continue;
}

void resect_investigate_type(resect_visit_context visit_context, resect_shaking_context context, CXType type) {
    switch (type.kind) {
        case CXType_Elaborated:
            resect_investigate_type(visit_context, context, clang_Type_getNamedType(type));
            return;
        case CXType_Unexposed: {
            CXType canonical_type = clang_getCanonicalType(type);
            if (CXType_Unexposed != canonical_type.kind) {
                resect_investigate_type(visit_context, context, canonical_type);
            }
        } break;
        case CXType_Attributed:
            // skip attributes
            resect_investigate_type(visit_context, context, clang_Type_getModifiedType(type));
            return;
        default:;
    }

    resect_type_kind kind = convert_type_kind(type.kind);
    resect_type_category category = get_type_category(kind);

    CXCursor decl_cursor = clang_getTypeDeclaration(type);
    resect_visit_cursor(visit_context, decl_cursor, context);

    switch (kind) {
        case RESECT_TYPE_KIND_FUNCTIONPROTO:
            resect_investigate_function_type(visit_context, context, type);
            break;
        default:
            switch (category) {
                case RESECT_TYPE_CATEGORY_POINTER:
                case RESECT_TYPE_CATEGORY_REFERENCE:
                    resect_investigate_pointer_type(visit_context, context, type);
                    break;
                case RESECT_TYPE_CATEGORY_ARRAY:
                    resect_investigate_array_type(visit_context, context, type);
                    break;
                default:
                    break;
            }
    }
    resect_investigate_template_args(visit_context, context, type);

    resect_investigate_child_data visit_data = {.visit_context = visit_context, .shaking_context = context};

    if (!clang_Cursor_isNull(decl_cursor) && clang_getCursorKind(decl_cursor) != CXCursor_NoDeclFound) {
        switch (convert_cursor_kind(decl_cursor)) {
            case RESECT_DECL_KIND_STRUCT:
            case RESECT_DECL_KIND_UNION:
            case RESECT_DECL_KIND_CLASS: {
                resect_string decl_id = resect_extract_decl_id(decl_cursor);
                resect_shaking_context_push_decl_link(context, decl_id);

                clang_Type_visitFields(type, visit_type_item, &visit_data);
                clang_visitCXXBaseClasses(type, visit_type_item, &visit_data);
                clang_visitCXXMethods(type, visit_type_item, &visit_data);

                resect_shaking_context_pop_link(context);
                resect_string_free(decl_id);
            } break;

            default:;
        }
    }
}

/*
 * REGISTRY
 */
typedef struct P_resect_inclusion_registry {
    resect_table table;
} *resect_inclusion_registry;

resect_inclusion_registry resect_inclusion_registry_create(resect_shaking_context shaking_context) {
    resect_inclusion_registry registry = malloc(sizeof(struct P_resect_inclusion_registry));
    registry->table = resect_table_create();

    resect_shaking_context__init_registry_table(shaking_context, registry->table);

    return registry;
}

resect_inclusion_status decode_inclusion_status(void *encoded_value) {
    return (resect_inclusion_status) ((unsigned long) encoded_value - 1);
}

bool resect_inclusion_registry_decl_included(resect_inclusion_registry registry, const char *decl_id) {
    void *encoded_value = resect_table_get(registry->table, decl_id);
    if (encoded_value == NULL) {
        return false;
    }

    resect_inclusion_status inclusion_status = decode_inclusion_status(encoded_value);
    switch (inclusion_status) {
        case RESECT_INCLUSION_STATUS_INCLUDED:
        case RESECT_INCLUSION_STATUS_ENFORCED:
            return true;
        default:
            return false;
    }
}

void resect_inclusion_registry_free(resect_inclusion_registry registry) {
    resect_table_free(registry->table, NULL, NULL);
    free(registry);
}

static void *encode_inclusion_status(resect_inclusion_status new_status) {
    return (void *) ((unsigned long) new_status + 1);
}

static void update_registry_entry(resect_table registry, resect_string decl_id, resect_inclusion_status new_status) {
    resect_table_put(registry, resect_string_to_c(decl_id), encode_inclusion_status(new_status));
}

typedef struct resect_promotion_result {
    resect_inclusion_status current_status;
    bool promoted;
} resect_promotion_result;

static resect_promotion_result promote_registry_entry(resect_table registry, resect_string decl_id,
                                                      resect_inclusion_status new_status) {
    void *value = resect_table_get(registry, resect_string_to_c(decl_id));
    resect_inclusion_status old_status;
    if (value == NULL) {
        old_status = RESECT_INCLUSION_STATUS_UNKNOWN;
    } else {
        old_status = decode_inclusion_status(value);
    }
    if (old_status < new_status) {
        update_registry_entry(registry, decl_id, new_status);
        resect_promotion_result result = {.current_status = new_status, .promoted = true};
        return result;
    }
    resect_promotion_result result = {.current_status = old_status, .promoted = false};
    return result;
}


static bool resect_shaking_context__follow_edge(resect_decl_graph graph, resect_decl_graph_edge edge,
                                                resect_table registry, bool reinforced);

typedef struct P_resect_follow_next_edge_data {
    resect_decl_graph graph;
    resect_table registry;
    resect_bool enforced;
    resect_decl_graph_edge edge;

    resect_bool result; // excluded
} *resect_follow_next_edge_data;

static resect_bool follow_next_edge(void *ctx, const char *key, void *value) {
    resect_follow_next_edge_data visit_data = ctx;

    resect_decl_graph_edge next_edge = value;
    if (resect_shaking_context__follow_edge(visit_data->graph, next_edge, visit_data->registry, visit_data->enforced)) {
        // exclusion found, recursing out
        visit_data->result = true;
        return false;
    }
    return true;
}

/**
 * @return true, if edge is excluded
 */
static bool resect_shaking_context__follow_edge(resect_decl_graph graph, resect_decl_graph_edge edge,
                                                resect_table registry, bool reinforced) {
    resect_decl_graph_node target = resect_decl_graph__find_node(graph, edge->id);
    resect_inclusion_status new_status = RESECT_INCLUSION_STATUS_UNKNOWN;

    bool recurse = true;
    bool enforced = target->filter_status == RESECT_FILTER_STATUS_ENFORCED || reinforced;

    if (enforced) {
        new_status = RESECT_INCLUSION_STATUS_ENFORCED;
    } else {
        switch (target->filter_status) {
            case RESECT_FILTER_STATUS_INCLUDED:
                new_status = RESECT_INCLUSION_STATUS_INCLUDED;
                break;
            case RESECT_FILTER_STATUS_EXCLUDED:
                new_status = RESECT_INCLUSION_STATUS_EXCLUDED;
                recurse = false;
                break;
            case RESECT_FILTER_STATUS_IGNORED:
                new_status = RESECT_INCLUSION_STATUS_INCLUDED;
                break;
            default:;
        }
    }

    assert(new_status != RESECT_INCLUSION_STATUS_UNKNOWN);

    resect_promotion_result promotion = promote_registry_entry(registry, target->id, new_status);
    if (promotion.current_status == RESECT_INCLUSION_STATUS_EXCLUDED) {
        return true;
    }

    if (!promotion.promoted) {
        return false;
    }

    if (!recurse) {
        return false;
    }

    struct P_resect_follow_next_edge_data visit_data = {
            .graph = graph, .registry = registry, .enforced = enforced, .edge = edge, .result = false};
    resect_visit_table(target->edges, follow_next_edge, &visit_data);
    if (visit_data.result) {
        update_registry_entry(registry, target->id, RESECT_INCLUSION_STATUS_EXCLUDED);
    }
    return visit_data.result;
}

static resect_bool printf_registry(void *ctx, const char *key, void *data) {
    resect_inclusion_status status = decode_inclusion_status(data);
    switch (status) {
        case RESECT_INCLUSION_STATUS_INCLUDED:
            fprintf(stderr, "INCL: %s\n", key);
            break;
        case RESECT_INCLUSION_STATUS_ENFORCED:
            fprintf(stderr, "ENF: %s\n", key);
            break;
        default:;
    }
    return true;
}

typedef struct P_resect_visit_edge_data {
    resect_shaking_context context;
    resect_decl_graph graph;
    resect_table registry;
} *resect_visit_edge_data;

static resect_bool visit_root_edge(void *ctx, const char *key, void *value) {
    resect_visit_edge_data visit_data = ctx;
    resect_decl_graph_edge edge = value;
    resect_decl_graph graph = visit_data->graph;
    resect_table registry = visit_data->registry;

    resect_decl_graph_node node = resect_decl_graph__find_node(graph, edge->id);
    if (node->filter_status != RESECT_FILTER_STATUS_IGNORED) {
        bool excluded = resect_shaking_context__follow_edge(graph, edge, registry,
                                                            node->filter_status == RESECT_FILTER_STATUS_ENFORCED);
        if (excluded) {
            update_registry_entry(registry, node->id, RESECT_INCLUSION_STATUS_EXCLUDED);
        }
    }
    return true;
}

typedef struct P_resect_affected_parents_visit_data {
    resect_visit_edge_data visit_edge_data;
    resect_string edge_id;
    resect_set affected_edges;
    resect_set visited_nodes;
} *resect_affected_parents_visit_data;

static resect_bool revisit_affected_parent_nodes(void *ctx, const char *node_id, void *value) {
    resect_affected_parents_visit_data data = ctx;
    resect_visit_edge_data edge_data = data->visit_edge_data;

    resect_decl_graph_node node = resect_decl_graph__find_node_c(edge_data->graph, node_id);

    resect_string root_node_id = resect_shaking_context_root_id(edge_data->context);
    if (resect_string_equal_c(root_node_id, node_id)) {
        // node_id points to root
        resect_inclusion_status edge_target_inclusion_status = decode_inclusion_status(
                resect_table_get(data->visit_edge_data->registry, resect_string_to_c(data->edge_id)));
        if (edge_target_inclusion_status != RESECT_INCLUSION_STATUS_ENFORCED) {
            resect_decl_graph_edge edge_to_revisit = resect_decl_graph_node_find_edge(node, data->edge_id);
            resect_set_add(data->affected_edges, edge_to_revisit);
        }
        return true;
    }

    if (resect_set_contains(data->visited_nodes, node)) {
        // recurse out of dependency loops
        return true;
    }
    resect_set_add(data->visited_nodes, node);

    struct P_resect_affected_parents_visit_data next_data = *data;
    next_data.edge_id = node->id;
    resect_visit_table(node->parents, revisit_affected_parent_nodes, &next_data);
    return true;
}

static resect_bool visit_enforced_nodes(void *ctx, const char *node_id, void *encoded_status) {
    resect_affected_parents_visit_data data = ctx;

    if (decode_inclusion_status(encoded_status) == RESECT_INCLUSION_STATUS_ENFORCED) {
        resect_decl_graph_node node = resect_decl_graph__find_node_c(data->visit_edge_data->graph, node_id);
        struct P_resect_affected_parents_visit_data next_data = *data;
        next_data.edge_id = node->id;
        resect_visit_table(node->parents, revisit_affected_parent_nodes, &next_data);
    }

    return true;
}

static resect_bool visit_affected_edge(void *ctx, void *item) {
    resect_affected_parents_visit_data data = ctx;
    resect_decl_graph_edge edge = item;
    visit_root_edge(data->visit_edge_data, resect_string_to_c(edge->id), edge);
    return true;
}

static resect_bool reset_excluded(void *ctx, void *item) {
    resect_affected_parents_visit_data data = ctx;
    resect_decl_graph_node node = item;

    resect_table registry = data->visit_edge_data->registry;
    const char *node_id = resect_string_to_c(node->id);
    resect_inclusion_status status = decode_inclusion_status(resect_table_get(registry, node_id));
    if (status == RESECT_INCLUSION_STATUS_EXCLUDED) {
        resect_table_remove(registry, node_id);
    }
    return true;
}

static void resect_shaking_context__init_registry_table(resect_shaking_context shaking_context, resect_table registry) {
    resect_decl_graph graph = shaking_context->decl_graph;
    resect_decl_graph_node root = resect_decl_graph__find_node(graph, shaking_context->root_decl_id);
    resect_set enforced_nodes = resect_set_create();

    struct P_resect_visit_edge_data visit_data = {
            .context = shaking_context,
            .graph = graph,
            .registry = registry,
    };

    resect_visit_table(root->edges, visit_root_edge, &visit_data);

    resect_set affected_edges = resect_set_create();
    resect_set visited_nodes = resect_set_create();
    struct P_resect_affected_parents_visit_data affected_visit_data = {.visit_edge_data = &visit_data,
                                                                       .affected_edges = affected_edges,
                                                                       .visited_nodes = visited_nodes,
                                                                       .edge_id = NULL};
    resect_visit_table(registry, visit_enforced_nodes, &affected_visit_data);
    resect_visit_set(visited_nodes, reset_excluded, &affected_visit_data);
    resect_visit_set(affected_edges, visit_affected_edge, &affected_visit_data);
    resect_set_free(visited_nodes);
    resect_set_free(affected_edges);

    if (shaking_context->print_diagnostics) {
        resect_visit_table(registry, printf_registry, NULL);
    }

    resect_set_free(enforced_nodes);
}
