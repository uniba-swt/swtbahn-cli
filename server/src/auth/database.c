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

#include "database.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

int db_init(char* dir) {
    char* file = malloc(sizeof(char) * (strlen(dir) + 12));
    file = strcpy(file, dir);
    file = strcat(file, "/bahn.sqlite");

    // create the file if it does not exist
    fclose(fopen(file, "ab+"));

    int result = sqlite3_open_v2(file, &db_connection, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (result == SQLITE_OK) {
        result = db_prepare();
    }

    free(file);

    return result == SQLITE_OK;
}

int db_close() {
    int result = sqlite3_close(db_connection);
    return result == SQLITE_OK;
}

int db_prepare() {
    char* err;
    int result = sqlite3_exec(db_connection,
            "create table if not exists user ("
            "   id integer primary key, "
            "   username varchar(255) not null unique, "
            "   hash blob not null, "
            "   salt blob not null,"
            "   active boolean not null,"
            "   mail varchar(255));"

            "create table if not exists user_role ("
            "   id integer primary key, "
            "   user_id integer not null,"
            "   role varchar(255) not null,"
            "   platform varchar(255) not null,"
            "   foreign key(user_id) references user(id));", NULL, NULL, &err);

    if (result != SQLITE_OK) {
        printf("failed to setup tables: %s", sqlite3_errmsg(db_connection));
        return result;
    }

    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "select count(*) from user;", -1, &stmt, NULL);
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE && result != SQLITE_ROW) {
        return result;
    }
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // no users -> create admin
    if (count <= 0) {
        // generate admin password
        char passwd[16];
        int i;
        for( i = 0; i < 12; i++) {
            passwd[i] = 33 + rand() % 94;
        }
        passwd[i] = '\0';

        // create user
        db_create_user("admin", passwd, "sekretariat.swt@uni-bamberg.de", 1);
        db_user_add_role_platform("admin", "admin", ROLE_PLATFORM_ANY);


        printf("\n\n"
               "================================================================================\n"
               "\tLogin admin/%s\n"
               "================================================================================\n"
               "\n\n", passwd);
    }

    return 0;
}

int db_check_login(char *username, char *password) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "select hash,salt from user where username=? and active=1;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, strlen(username), NULL);
    int result = sqlite3_step(stmt);
    if (result != SQLITE_ROW) {
        return 0; // no result, user does not exist
    }

    const void* hash_blob = sqlite3_column_blob(stmt, 0);
    int hash_len = sqlite3_column_bytes(stmt, 0);
    const void* salt_blob = sqlite3_column_blob(stmt, 1);
    int salt_len = sqlite3_column_bytes(stmt, 1);

    unsigned char hash[256];
    PKCS5_PBKDF2_HMAC_SHA1(password, strlen(password), salt_blob, salt_len, 5000, 256, hash);
    result = memcmp(hash_blob, hash, hash_len);

    sqlite3_finalize(stmt);

    return result == 0;
}

int db_create_user(const char *username, const char *password, const char* mail, int active) {
    // generate salt
    unsigned char salt[32];
    RAND_bytes(salt, 32);

    // hash password
    unsigned char hash[256];
    PKCS5_PBKDF2_HMAC_SHA1(password, strlen(password), salt, 32, 5000, 256, hash);

    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "insert into user (username, hash, salt, active, mail) values (?, ?, ?, ?, ?);", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, strlen(username), NULL);
    sqlite3_bind_blob(stmt, 2, hash, 256, NULL);
    sqlite3_bind_blob(stmt, 3, salt, 32, NULL);
    sqlite3_bind_int(stmt, 4, active);
    if (mail == NULL) {
        sqlite3_bind_null(stmt, 5);
    } else {
        sqlite3_bind_text(stmt, 5, mail, strlen(mail), NULL);
    }

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

int db_user_activate(const char *username, int active) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "update user set active=? where username=?;", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, active);
    sqlite3_bind_text(stmt, 2, username, strlen(username), NULL);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

db_user** db_get_all_users(int *numUsers) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "select user.username, user.active, user.mail, user_role.role, user_role.platform from user left join user_role on user.id = user_role.user_id;", -1, &stmt, NULL);

    *numUsers = 0;
    int size = 10;
    db_user** users = malloc(sizeof(db_user) * size);

    db_user* user = NULL;
    int role_size = 10;

    int result = sqlite3_step(stmt);
    while (result == SQLITE_ROW) {
        // resize array if array is full
        if (size - 1 <= *numUsers) {
            size = size + 10;
            users = realloc(users, sizeof(db_user) * size);
        }

        // get data of result set
        const char* username = (const char*) sqlite3_column_text(stmt, 0);
        int active = sqlite3_column_int(stmt, 1);
        const char* mail = NULL;
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            mail = (const char*) sqlite3_column_text(stmt, 2);
        }
        const char* role = NULL;
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            role = (const char*) sqlite3_column_text(stmt, 3);
        }
        const char* platform = NULL;
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            platform = (const char*) sqlite3_column_text(stmt, 4);
        }

        // if no user has been created before or we've reached a new one, create a new struct for it
        if (user == NULL || strcmp(user->username, username) != 0) {
            role_size = 10;
            user = malloc(sizeof(db_user));
            user->username = malloc(sizeof(char) * strlen(username));
            strcpy(user->username, username);
            if (mail != NULL) {
                user->mail = malloc(sizeof(char) * strlen(mail));
                strcpy(user->mail, mail);
            }
            user->active = active;

            user->numRoles = 0;
            user->roles = malloc(sizeof(db_role*) * role_size);
            if (role != NULL) {
                user->roles[user->numRoles] = malloc(sizeof(db_role));
                user->roles[user->numRoles]->role = malloc(sizeof(char) * strlen(role));
                strcpy(user->roles[user->numRoles]->role, role);
                user->roles[user->numRoles]->platform = malloc(sizeof(char) * strlen(platform));
                strcpy(user->roles[user->numRoles++]->platform, platform);
            }

            users[*numUsers] = user;
            *numUsers = *numUsers + 1;
        } else {
            // resize role array if array is full
            if (role_size - 1 <= user->numRoles) {
                role_size = role_size + 10;
                user->roles = realloc(user->roles, sizeof(db_role*) * role_size);
            }
            user->roles[user->numRoles] = malloc(sizeof(db_role));
            user->roles[user->numRoles]->role = malloc(sizeof(char) * strlen(role));
            strcpy(user->roles[user->numRoles]->role, role);
            user->roles[user->numRoles]->platform = malloc(sizeof(char) * strlen(platform));
            strcpy(user->roles[user->numRoles++]->platform, platform);
        }

        result = sqlite3_step(stmt);
    }

    if (result != SQLITE_DONE) {
        return NULL;
    }
    return users;
}

void db_free_all_users(db_user **users, int size) {
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < users[i]->numRoles; ++j) {
            free(users[i]->roles[j]->role);
            free(users[i]->roles[j]->platform);
            free(users[i]->roles[j]);
        }
        free(users[i]->roles);
        free(users[i]);
    }
}

int db_user_remove(const char *username) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "delete from user where username=?;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, strlen(username), NULL);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

int db_user_exists(const char *username) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "select count(*) from user where username=?;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, strlen(username), NULL);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_ROW) {
        printf("select count(*) did not return any results?! result = %d, err = %s", result, sqlite3_errmsg(db_connection));
        return 0;
    }

    int count = sqlite3_column_int(stmt, 0);

    return count > 0;
}

int db_user_has_role_platform(const char *username, const char *role, const char *platform, int exact) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "select count(*) from user_role "
                                   "left join user on user_role.user_id = user.id "
                                   "where user.username=? and (user_role.role=? and (user_role.platform=? or (? and user_role.platform='*')));", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, strlen(username), NULL);
    sqlite3_bind_text(stmt, 2, role, strlen(role), NULL);
    sqlite3_bind_text(stmt, 3, platform, strlen(platform), NULL);
    sqlite3_bind_int(stmt, 4, !exact);
    int result = sqlite3_step(stmt);
    if (result != SQLITE_ROW) {
        return 0; // should never happen, count(*) returns exactly one row
    }
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;}

int db_user_add_role_platform(const char *username, const char *role, const char *platform) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "insert into user_role (user_id, role, platform) values ((select id from user where username=?), ?, ?);", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, strlen(username), NULL);
    sqlite3_bind_text(stmt, 2, role, strlen(role), NULL);
    sqlite3_bind_text(stmt, 3, platform, strlen(platform), NULL);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

int db_user_remove_role_platform(const char *username, const char *role, const char *platform) {
    sqlite3_stmt* stmt;
    sqlite3_prepare(db_connection, "delete from user_role "
                                   "where id in ("
                                   "    select user_role.id from user_role "
                                   "    left join user on user_role.user_id = user.id "
                                   "    where user.username=? and user_role.role=? and user_role.platform=?"
                                   ");", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, strlen(username), NULL);
    sqlite3_bind_text(stmt, 2, role, strlen(role), NULL);
    sqlite3_bind_text(stmt, 3, platform, strlen(platform), NULL);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}
