//
// Created by borodust on 12/27/19.
//
#include "resect.h"
#include <stdio.h>

struct {
    resect_cursor_location location;
    resect_string id;
    resect_string comment;
    resect_string debug_info;
    resect_type type;
} visitor_data;

resect_visit_result print_cursor(resect_cursor cursor, resect_cursor parent) {
    resect_cursor_get_id(visitor_data.id, cursor);
    resect_cursor_location location = resect_cursor_get_location(visitor_data.location, cursor);
    resect_string comment = resect_cursor_get_comment(visitor_data.comment, cursor);
    resect_type type = resect_cursor_get_type(visitor_data.type, cursor);
    resect_string debug_info = resect_cursor_get_debug_info(visitor_data.debug_info, cursor);

    printf("\n\nCursor %d: \"%s:%d:%d\"\nID: %s\nComment: \"%s\"\nSize: %d, Alignment: %d\nDebug: %s",
           resect_cursor_get_kind(cursor),
           resect_cursor_location_name(location),
           resect_cursor_location_line(location),
           resect_cursor_location_column(location),
           resect_string_to_c(visitor_data.id),
           resect_string_to_c(comment),
           (int) resect_type_sizeof(type),
           (int) resect_type_alignof(type),
           resect_string_to_c(debug_info));

    return RESECT_VISIT_RESULT_RECURSE;
}

int main(int argc, char **argv) {
    char *filename = argc > 1 ? argv[1] : "/usr/include/stdlib.h";

    visitor_data.location = resect_allocate_cursor_location();
    visitor_data.id = resect_allocate_string(256);
    visitor_data.comment = resect_allocate_string(256);
    visitor_data.type = resect_allocate_type();
    visitor_data.debug_info = resect_allocate_string(256);

    resect_parse(filename, print_cursor, NULL);

    resect_free_cursor_location(visitor_data.location);
    resect_free_string(visitor_data.comment);
    resect_free_string(visitor_data.debug_info);
    resect_free_type(visitor_data.type);

    return 0;
}