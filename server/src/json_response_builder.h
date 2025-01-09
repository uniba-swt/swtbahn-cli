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

#ifndef JSON_RESPONSE_BUILDER_H
#define JSON_RESPONSE_BUILDER_H

#include <glib.h>
#include <stdbool.h>

/**
 * @brief Adds a json field with an identifier and a value, 
 * where the value is represented in json as a string.
 * Example. Input: field=`mystr`, value_str=`helloworld`.
 * Resulting addition to `dest`: `\n"mystr": "helloworld"`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_str string value of the json field
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_str_value(GString *dest, const char *field, const char *value_str, 
                                bool add_trailing_comma);
/**
 * @brief Adds a json field with an identifier and a string value (from int). 
 * Example. Input: field=`mystr`, value_int=`5`.
 * Resulting addition to `dest`: `\n"mystr": "5"`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_int int value of the json field (will be added as string, i.e., enclosed by "")
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_str_value_from_int(GString *dest, const char *field, int value_int, 
                                         bool add_trailing_comma);

/**
 * @brief Adds a json field with an identifier and a value.
 * Example. Input: field=`myval`, value_str=`barevaluehello`.
 * Resulting addition to `dest`: `\n"myval": barevaluehello`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_str value of the json field (will not be enclosed in, e.g., quote marks)
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_bare_value_from_str(GString *dest, const char *field, const char *value_str, 
                                          bool add_trailing_comma);

/**
 * @brief Adds a json field with a list of strings as the value of the field.
 * Example. Input: field=`mystrlist`, value_liststr=list with strings `hello`, `world`.
 * Resulting addition to `dest`: `\n"mystrlist": ["hello", "world"]`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_liststr list of strings to add as the field value
 * @param list_len length of the list of strings (i.e., length of value_liststr)
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_strlist_value(GString *dest, const char *field, const char **value_liststr, 
                                    unsigned int list_len, bool add_trailing_comma);

/**
 * @brief Adds a json field with a list of strings as the value of the field.
 * Like append_field_strlist_value, but here with GArray* instead of raw array + length.
 * See append_field_strlist_value for an example.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param g_strarray list of strings to add as the field value
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_strlist_value_from_garray_strs(GString *dest, const char *field, 
                                                     const GArray* g_strarray, 
                                                     bool add_trailing_comma);

/**
 * @brief Adds a json field with a list as the value of the field.
 * Like append_field_strlist_value and append_field_strlist_value_from_garray_strs,
 * but here the strings in the passed list are not enclosed in quote marks
 * in the `dest` string.
 * Example. Input: field=`mylist`, g_strarray=list with strings `hello`, `world`.
 * Resulting addition to `dest`: `\n"mylist": [hello, world]`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param g_strarray list of strings to add as the field value (as bare values)
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_barelist_value_from_garray_strs(GString *dest, const char *field, 
                                                      const GArray* g_strarray, 
                                                      bool add_trailing_comma);

/**
 * @brief Adds a json field with an empty list as the value of the field.
 * Example. Input: field=`mylist`.
 * Resulting addition to `dest`: `\n"mylist": []`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_emptylist_value(GString *dest, const char *field, bool add_trailing_comma);

/**
 * @brief Adds a json field with a boolean value (true or false).
 * Example. Input: field=`mybool`, value_bool=`true`.
 * Resulting addition to `dest`: `\n"mybool": true`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_bool field value
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_bool_value(GString *dest, const char *field, bool value_bool, 
                                 bool add_trailing_comma);

/**
 * @brief Adds a json field with a number value (from int).
 * Example. Input: field=`mynumber`, value_int=`5`.
 * Resulting addition to `dest`: `\n"mynumber": 5`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_int field value
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_int_value(GString *dest, const char *field, int value_int, 
                                bool add_trailing_comma);

/**
 * @brief Adds a json field with a number value (from unsigned int).
 * Example. Input: field=`mynumber`, value_uint=`-5`.
 * Resulting addition to `dest`: `\n"mynumber": -5`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_uint field value
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_uint_value(GString *dest, const char *field, unsigned int value_uint, 
                                 bool add_trailing_comma);

/**
 * @brief Adds a json field with a (real) number value (from float).
 * Example. Input: field=`myreal`, value_float=`15.124`.
 * Resulting addition to `dest`: `\n"myreal": 15.124`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @param value_float field value
 * @param add_trailing_comma if true, adds comma after the field and value
 * @return GString* modified "dest" string
 */
GString* append_field_float_value(GString *dest, const char *field, float value_float, 
                                  bool add_trailing_comma);

/**
 * @brief Adds a json field and starts a list for the value 
 * (i.e., value is incomplete in json terms, list needs to be closed later).
 * Example. Input: field=`mylist`.
 * Resulting addition to `dest`: `\n"mylist": [`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @return GString* modified "dest" string
 */
GString* append_field_start_of_list(GString *dest, const char *field);

/**
 * @brief Adds the end of a list marker (`]`).
 * Example. Resulting Addition to `dest`: `]`.
 * 
 * @param dest String to be added to
 * @param add_trailing_comma if true, adds comma after the added `]`
 * @param with_prepend_newline if true, adds a newline in front of the added `]`
 * @return GString* modified "dest" string
 */
GString* append_end_of_list(GString *dest, bool add_trailing_comma, bool with_prepend_newline);

/**
 * @brief Adds a json field and starts an object for the value 
 * (i.e., value is incomplete in json terms, object needs to be closed later).
 * Example. Input: field=`myobject`.
 * Resulting addition to `dest`: `\n"myobject": {`.
 * 
 * @param dest String to be added to
 * @param field name of the json field to add
 * @return GString* modified "dest" string
 */
GString* append_field_start_of_obj(GString *dest, const char *field);

/**
 * @brief Adds the start of an object marker (`{`).
 * Example. Resulting Addition to `dest`: `{`.
 * 
 * @param dest String to be added to
 * @param with_prepend_newline if true, adds a newline in front of the added `{`
 * @return GString* modified "dest" string
 */
GString* append_start_of_obj(GString *dest, bool with_prepend_newline);

/**
 * @brief Adds the end of an object marker (`}`).
 * Example. Resulting Addition to `dest`: `}`.
 * 
 * @param dest String to be added to
 * @param add_trailing_comma if true, adds a comma after the added `}`
 * @return GString* modified "dest" string
 */
GString* append_end_of_obj(GString *dest, bool add_trailing_comma);

#endif // JSON_RESPONSE_BUILDER_H