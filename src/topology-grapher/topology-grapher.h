#ifndef TOPOLOGY_GRAPHER_H
#define TOPOLOGY_GRAPHER_H

#include <gtk/gtk.h>
#include <pthread.h>
#include <first.h>

extern const uint32_t WIDTH;
extern const uint32_t HEIGHT;
extern const uint32_t RADIUS;
extern const uint32_t ITERATIONS;
extern const uint32_t GRAPHER_MAX_VERTICES;
extern const uint32_t GRAPHER_MAX_EDGES;

typedef struct {
    double x, y;
} Vec2;

typedef struct {
    Vec2 pos, disp;
    gboolean dragging;
    uint32_t router_id;
    InterfaceTableEntry *interfaces;
    uint32_t num_interfaces;
} Vertex;

typedef struct {
    Vertex *v, *u;
} Edge;


typedef struct {
    Vertex vert;
    int checked;
} NeighborVert;

typedef struct {
    NeighborVert *neighbors;
    uint32_t num_neighbors;
} NeighborsState;


typedef struct {
    Vertex* vertices;
    uint32_t num_vertices;
    Edge* edges;
    uint32_t num_edges;
    uint32_t curr_iteration;
    double t;
    GtkWidget* drawing_area;
    Vertex* dragged_vertex;
    pthread_mutex_t change_graph_mutex;
} GrapherState;

#endif
