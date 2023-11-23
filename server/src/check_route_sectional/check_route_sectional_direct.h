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
 * - Bernhard Luedtke <https://github.com/BLuedtke>
 *
 */

#ifndef CHECK_ROUTE_SECTIONAL_DIRECT_H
#define CHECK_ROUTE_SECTIONAL_DIRECT_H

#include <stdbool.h>

/**
 * Checks if for two conflicting routes, one granted, and one requested, it is safe to grant the
 * requested route, judging from sectional route release logic.
 * 
 * @param granted_route_id granted route
 * @param requested_route_id requested route that has a conflict with the granted route
 * @return true if the requested route is safe to grant in context of the conflicting route
 * @return false if the requested route is not safe to grant in context of the conflicting route
 */
bool is_route_conflict_safe_sectional(const char *granted_route_id, const char *requested_route_id);

#endif  // CHECK_ROUTE_SECTIONAL_DIRECT_H
