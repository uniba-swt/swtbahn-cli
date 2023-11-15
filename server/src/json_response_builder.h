///AUTHOR: Bernhard Luedtke
#pragma once

#include <glib.h>
#include <stdbool.h>

GString* append_field_str_value(GString *dest, const char *field, const char *value_str, bool add_trailing_comma);
GString* append_field_bare_value_from_str(GString *dest, const char *field, const char *value_str, bool add_trailing_comma);
GString* append_field_strlist_value(GString *dest, const char *field, const char **value_liststr, unsigned int list_len, bool add_trailing_comma);
GString* append_field_strlist_value_garray_strs(GString *dest, const char *field, const GArray* g_strarray, bool add_trailing_comma);
GString* append_field_barelist_value_from_garray_strs(GString *dest, const char *field, const GArray* g_strarray, bool add_trailing_comma);
GString* append_field_emptylist_value(GString *dest, const char *field, bool add_trailing_comma);
GString* append_field_bool_value(GString *dest, const char *field, bool value_bool, bool add_trailing_comma);
GString* append_field_int_value(GString *dest, const char *field, int value_int, bool add_trailing_comma);
GString* append_field_uint_value(GString *dest, const char *field, unsigned int value_uint, bool add_trailing_comma);
GString* append_field_float_value(GString *dest, const char *field, float value_float, bool add_trailing_comma);

GString* append_field_start_of_list(GString *dest, const char *field);
GString* append_start_of_list(GString *dest);
GString* append_end_of_list(GString *dest, bool add_trailing_comma);

GString* append_field_with_start_of_obj(GString *dest, const char *field);
GString* append_start_of_obj(GString *dest);
GString* append_end_of_obj(GString *dest, bool add_trailing_comma);