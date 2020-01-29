//
// Created by Tri Nguyen on 28/01/2020.
//

#ifndef CYAMLPARSER_INTERLOCKING_PARSER_H
#define CYAMLPARSER_INTERLOCKING_PARSER_H

#endif //CYAMLPARSER_INTERLOCKING_PARSER_H

#include <stdbool.h>
#include <glib.h>

char* concat_str(const char *str1, const char *str2);

GArray* parse_interlocking_table(const char *config_dir);