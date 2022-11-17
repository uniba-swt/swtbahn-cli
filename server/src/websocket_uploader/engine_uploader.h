/*
 *
 * Copyright (C) 2022 University of Bamberg, Software Technologies Research Group
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

#ifndef ENGINE_UPLOADER_H
#define ENGINE_UPLOADER_H

#include <glib.h>
#include <stdbool.h>

typedef struct {
   bool success;
   GString* srv_result_full_msg;
} verif_result;

/**
 * Verifies sctx engine model located in file at f_filepath.
 * 
 * @param f_filepath Path to the file containing the .sctx model to be verified
 * @return verif_result Result of the verification
 */
verif_result verify_engine_model(const char* f_filepath);

#endif // ENGINE_UPLOADER_H