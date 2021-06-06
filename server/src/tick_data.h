/*
 * Automatically generated C code by
 * KIELER SCCharts - The Key to Efficient Modeling
 *
 * http://rtsys.informatik.uni-kiel.de/kieler
 */

#ifndef TICK_DATA_H
#define TICK_DATA_H

typedef struct {
  int requested_speed;
  char requested_forwards;
  int nominal_speed;
  char nominal_forwards;
  
  char internal_variables[1024];
} TickData;

typedef struct {
    char* src_signal_id;                                                    // Input
    char* dst_signal_id;                                                    // Input
    char* train_id;                                                         // Input
    char* out;                                                              // Output

    int terminated;
} request_route_tick_data;

typedef struct {
    char* route_id;                                                    // Input
    char* train_id;                                                    // Input
    char* segment_ids[1024];                                           // Input
    int count_segments;                                                // Input

    int terminated;
} drive_route_tick_data;

#endif // TICK_DATA_H
