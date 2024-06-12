/*
 *
 * Copyright (C) 2024 University of Bamberg, Software Technologies Research Group
 * <https://www.uni-bamberg.de/>, <http://www.swt-bamberg.de/>
 * 
 * This file is part of the SWTbahn command line interface (swtbahn-cli), which is
 * a client-server application to interactively control a BiDiB model railway.
 *
 * swtbahn-cli is licensed under the GNU GENERAL PUBLIC LICENSE (Version 3), see
 * the LICENSE file at the project's top-level directory for details or consult
 * <http://www.gnu.org/licenses/>.
 *
 * swtbahn-cli is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * swtbahn-cli is a RESEARCH PROTOTYPE and distributed WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * The following people contributed to the conception and realization of the
 * present swtbahn-cli (in alphabetic order by surname):
 *
 * - Bernhard Luedtke
 *
 */

#ifndef JSON_RESPONSE_BUILDER_H
#define JSON_RESPONSE_BUILDER_H

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
GString* append_end_of_list(GString *dest, bool add_trailing_comma, bool with_prepend_newline);

GString* append_field_start_of_obj(GString *dest, const char *field);
GString* append_start_of_obj(GString *dest, bool with_prepend_newline);
GString* append_end_of_obj(GString *dest, bool add_trailing_comma);

#endif // JSON_RESPONSE_BUILDER_H