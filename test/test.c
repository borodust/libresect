//
// Created by borodust on 12/27/19.
//
#include "../resect.h"
#include <stdio.h>


void print_record_fields(resect_type type, resect_collection fields) {
    resect_iterator field_iter = resect_collection_iterator(fields);
    while (resect_iterator_next(field_iter)) {
        resect_decl field = resect_iterator_value(field_iter);
        printf(" FIELD: %s {offset: %lld} \n", resect_decl_get_name(field), resect_field_get_offset(field));
    }
    resect_iterator_free(field_iter);
}

void print_enum_constants(resect_decl decl) {
    resect_collection constants = resect_enum_constants(decl);
    resect_iterator constant_iter = resect_collection_iterator(constants);
    while (resect_iterator_next(constant_iter)) {
        resect_decl constant = resect_iterator_value(constant_iter);
        printf(" CONSTANT: %s = %lld\n", resect_decl_get_name(constant), resect_enum_constant_value(constant));
    }
    resect_iterator_free(constant_iter);
}

void print_parameters(resect_decl decl) {
    resect_collection params = resect_function_parameters(decl);
    resect_iterator param_iter = resect_collection_iterator(params);
    while (resect_iterator_next(param_iter)) {
        resect_decl param = resect_iterator_value(param_iter);
        printf(" PARAMETER: %s\n", resect_decl_get_name(param));
    }
    resect_iterator_free(param_iter);
}

int main(int argc, char **argv) {
    char *filename = argc > 1 ? argv[1] : "/usr/include/stdlib.h";

    resect_parse_options options = resect_options_create();
    resect_options_add_include_path(options, "/usr/local/include/");

    resect_translation_context context = resect_parse(filename, options);

    resect_options_free(options);

    resect_collection decls = resect_context_declarations(context);
    resect_iterator decl_iter = resect_collection_iterator(decls);
    while (resect_iterator_next(decl_iter)) {
        resect_decl decl = resect_iterator_value(decl_iter);

        switch (resect_decl_get_kind(decl)) {
            case RESECT_DECL_KIND_STRUCT:
                printf("STRUCT: %s\n", resect_decl_get_name(decl));
                print_record_fields(resect_decl_get_type(decl), resect_struct_fields(decl));
                break;
            case RESECT_DECL_KIND_UNION:
                printf("UNION: %s\n", resect_decl_get_name(decl));
                print_record_fields(resect_decl_get_type(decl), resect_union_fields(decl));
                break;
            case RESECT_DECL_KIND_ENUM:
                printf("ENUM: %s\n", resect_decl_get_name(decl));
                print_enum_constants(decl);
                break;
            case RESECT_DECL_KIND_FUNCTION:
                printf("FUNCTION: %s\n", resect_decl_get_name(decl));
                print_parameters(decl);
                break;
            case RESECT_DECL_KIND_VARIABLE:
                printf("VARIABLE: %s\n", resect_decl_get_name(decl));
                break;
            case RESECT_DECL_KIND_TYPEDEF:
                printf("TYPEDEF: %s\n", resect_decl_get_name(decl));
                break;
        }
    }
    resect_iterator_free(decl_iter);

    resect_free(context);

    return 0;
}

