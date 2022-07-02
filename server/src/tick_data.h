/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#ifndef TICK_DATA_H
#define TICK_DATA_H

#include <stdbool.h>

typedef struct {
  int requested_speed;      // Input
  char requested_forwards;  // Input
  
  int nominal_speed;        // Output
  char nominal_forwards;    // Output
  
  char internal_variables[1024];
} TickData_train_engine;

typedef struct {
  char *src_signal_id;      // Input
  char *dst_signal_id;      // Input
  char *train_id;           // Input
  
  char *route_id;           // Output
  bool terminated;          // Output (internal variable)
} TickData_interlocker;

typedef struct {
  char *route_id;           // Input
  char *train_id;           // Input
  char *segment_ids[1024];  // Input
  int count_segments;       // Input

  bool terminated;          // Output (internal variable)
} TickData_drive_route;

#endif	// TICK_DATA_H
