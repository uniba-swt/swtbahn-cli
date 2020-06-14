/*
 *
 * Copyright (C) 2020 University of Bamberg, Software Technologies Research Group
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
 * - Florian Beetz <https://github.com/florian-beetz>
 *
 */

#ifndef SWTBAHN_CLI_AUTH_DB_H
#define SWTBAHN_CLI_AUTH_DB_H

#include <sqlite3.h>

typedef struct {
    char* username;
    char* mail;
    int active;

    int numRoles;
    char** roles;
} db_user;

/**
 * currently open database connection
 */
sqlite3* db_connection;

/**
 * Opens and initializes the database.
 *
 * @param dir directory to create the database in.
 *
 * @return 1 if the database is open, 0 otherwise.
 */
int db_init(char* dir);

/**
 * Closes the database.
 *
 * @return 1 if the database is closed, 0 otherwise.
 */
int db_close();

/**
 * Prepares the database for usage.
 *
 * Creates two tables (user, user_role) and creates a default admin user if no user exists in the database.
 * The login credentials are printed to stdout.
 *
 * @return 1 if the database was prepared, 0 otherwise.
 */
int db_prepare();

int db_check_login(char* username, char* password);

int db_user_has_role(char* username, char* role);

int db_create_user(char* username, char* password, char* mail, int active);

int db_user_add_role(char* username, char* role);

int db_user_activate(char* username, int active);

int db_user_remove(char* username);

int db_user_remove_role(char* username, char* role);

db_user** db_get_all_users(int* numUsers);

void db_free_all_users(db_user **users, int size);

int db_user_exists(char* username);

#endif //SWTBAHN_CLI_AUTH_DB_H
