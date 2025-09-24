/*
 *
 * Copyright (C) 2025 University of Bamberg, Software Technologies Research Group
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
 * - Bernhard Luedtke <https://github.com/bluedtke>
 *
 */

#include "json_response_builder.h"


GString* append_field_str_value(GString *dest, const char *field, 
                                const char *value_str, bool add_trailing_comma) {
	if (dest == NULL || field == NULL || value_str == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": \"%s\"%s", 
	                       field, value_str, add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_str_value_from_int(GString *dest, const char *field, 
                                         int value_int, bool add_trailing_comma) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": \"%d\"%s", 
	                       field, value_int, add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_literal_value_from_str(GString *dest, const char *field, 
                                             const char *value_str, bool add_trailing_comma) {
	if (dest == NULL || field == NULL || value_str == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": %s%s", 
	                       field, value_str, add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_strlist_value(GString *dest, const char *field, 
                                    const char **value_liststr, unsigned int list_len, 
                                    bool add_trailing_comma) {
	if (dest == NULL || field == NULL || value_liststr == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": [", field);
	
	for(unsigned int i = 0; i < list_len; ++i) {
		const char * list_elem = value_liststr[i];
		if (list_elem != NULL) {
			// append to json list and add "," if not last element
			g_string_append_printf(dest, "\"%s\"%s", list_elem, (i+1 < list_len) ? ", " : "");
		}
	}
	g_string_append_printf(dest, "]%s", add_trailing_comma ? "," : "");
	
	return dest;
}

static GString* append_field_value_garray_strs_base(GString *dest, const char *field, 
                                                    const GArray* g_strarray, 
                                                    bool add_value_quote_marks, 
                                                    bool add_trailing_comma) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	if (g_strarray == NULL || g_strarray->len == 0) {
		g_string_append_printf(dest, "\n\"%s\": []%s", field, add_trailing_comma ? "," : "");
		return dest;
	}
	const char *format_str_value_elem = add_value_quote_marks ? "\"%s\"%s" : "%s%s";
	
	g_string_append_printf(dest, "\n\"%s\": [", field);
	for (unsigned int i = 0; i < g_strarray->len; ++i) {
		g_string_append_printf(dest, format_str_value_elem, 
		                       g_array_index(g_strarray, char *, i), 
		                       (i+1 < g_strarray->len) ? ", " : "");
	}
	g_string_append_printf(dest, "]%s", add_trailing_comma ? "," : "");
	
	return dest;
}

GString* append_field_strlist_value_from_garray_strs(GString *dest, const char *field, 
                                                     const GArray* g_strarray, 
                                                     bool add_trailing_comma) {
	return append_field_value_garray_strs_base(dest, field, g_strarray, true, add_trailing_comma);
}

GString* append_field_literallist_value_from_garray_strs(GString *dest, const char *field, 
                                                         const GArray* g_strarray, 
                                                         bool add_trailing_comma) {
	return append_field_value_garray_strs_base(dest, field, g_strarray, false, add_trailing_comma);
}

GString* append_field_emptylist_value(GString *dest, const char *field, bool add_trailing_comma) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": []%s", field, add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_bool_value(GString *dest, const char *field, bool value_bool, 
                                 bool add_trailing_comma) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": %s%s", 
	                       field, value_bool ? "true" : "false", add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_int_value(GString *dest, const char *field, int value_int, 
                                bool add_trailing_comma) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": %d%s", 
	                       field, value_int, add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_uint_value(GString *dest, const char *field, unsigned int value_uint, 
                                 bool add_trailing_comma) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": %u%s", 
	                       field, value_uint, add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_float_value(GString *dest, const char *field, 
                                  float value_float, bool add_trailing_comma) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": %f%s", 
	                       field, value_float, add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_start_of_list(GString *dest, const char *field) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": [", field);
	return dest;
}

GString* append_end_of_list(GString *dest, bool add_trailing_comma, bool with_prepend_newline) {
	if (dest == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "%s]%s", 
	                       with_prepend_newline ? "\n" : "", add_trailing_comma ? "," : "");
	return dest;
}

GString* append_field_start_of_obj(GString *dest, const char *field) {
	if (dest == NULL || field == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n\"%s\": {", field);
	return dest;
}

GString* append_start_of_obj(GString *dest, bool with_prepend_newline) {
	if (dest == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "%s{", with_prepend_newline ? "\n" : "");
	return dest;
}

GString* append_end_of_obj(GString *dest, bool add_trailing_comma) {
	if (dest == NULL) {
		return NULL;
	}
	g_string_append_printf(dest, "\n}%s", add_trailing_comma ? "," : "");
	return dest;
}
