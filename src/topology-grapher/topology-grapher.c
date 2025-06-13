#include <first.h>
#include "topology-grapher.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

const uint32_t WIDTH = 600;
const uint32_t HEIGHT = 600;
const uint32_t RADIUS = 5;
const uint32_t ITERATIONS = 500;
const uint32_t GRAPHER_MAX_VERTICES = 200;
const uint32_t GRAPHER_MAX_EDGES = 300;


// ---- Physics Functions ----
double fr(double x) {
    return sqrt(WIDTH * HEIGHT) / x;
}

double fa(double x) {
    return (x * x) / sqrt(WIDTH * HEIGHT);
}

double cool(double t) {
    return t * 0.99;
}

// ---- Drawing ----
gboolean draw_graph(GtkWidget *widget, cairo_t *cr, gpointer data) {
    GrapherState *grapher_state = (GrapherState*) data;
    cairo_set_line_width(cr, 1);

    pthread_mutex_lock(&grapher_state->change_graph_mutex);
    for (int i = 0; i < grapher_state->num_edges; i++) {
        cairo_move_to(cr, grapher_state->edges[i].v->pos.x, grapher_state->edges[i].v->pos.y);
        cairo_line_to(cr, grapher_state->edges[i].u->pos.x, grapher_state->edges[i].u->pos.y);
    }
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);

    for (int i = 0; i < grapher_state->num_vertices; i++) {
        Vertex *v = &grapher_state->vertices[i];

        cairo_arc(cr, v->pos.x, v->pos.y, RADIUS, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0.2, 0.4, 0.8);
        cairo_fill(cr);

        char label[20];
        snprintf(label, sizeof(label), "%u.%u.%u.%u",
            v->interfaces[0].interface_ip[0],
            v->interfaces[0].interface_ip[1],
            v->interfaces[0].interface_ip[2],
            v->interfaces[0].interface_ip[3]
        );
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_move_to(cr, v->pos.x + RADIUS + 2, v->pos.y - RADIUS - 2);
        cairo_show_text(cr, label);
    }
    pthread_mutex_unlock(&grapher_state->change_graph_mutex);

    return FALSE;
}

// ---- Simulation ----
gboolean simulate(gpointer data) {
    GrapherState *grapher_state = (GrapherState *) data;

    if (grapher_state->curr_iteration++ >= ITERATIONS) return FALSE;

    pthread_mutex_lock(&grapher_state->change_graph_mutex);
    for (int i = 0; i < grapher_state->num_vertices; i++) {
        if (!grapher_state->vertices[i].dragging) {
            grapher_state->vertices[i].disp.x = 0;
            grapher_state->vertices[i].disp.y = 0;
        }
    }

    // Repulsive
    for (int i = 0; i < grapher_state->num_vertices; i++) {
        for (int j = 0; j < grapher_state->num_vertices; j++) {
            if (i == j) continue;
            double dx = grapher_state->vertices[i].pos.x - grapher_state->vertices[j].pos.x;
            double dy = grapher_state->vertices[i].pos.y - grapher_state->vertices[j].pos.y;
            double dist = sqrt(dx * dx + dy * dy) + 0.1;
            double force = fr(dist);
            if (!grapher_state->vertices[i].dragging) {
                grapher_state->vertices[i].disp.x += (dx / dist) * force;
                grapher_state->vertices[i].disp.y += (dy / dist) * force;
            }
        }
    }

    // Attractive
    for (int i = 0; i < grapher_state->num_edges; i++) {
        Vertex *v = grapher_state->edges[i].v;
        Vertex *u = grapher_state->edges[i].u;
        double dx = v->pos.x - u->pos.x;
        double dy = v->pos.y - u->pos.y;
        double dist = sqrt(dx * dx + dy * dy) + 0.1;
        double force = fa(dist);
        if (!v->dragging) {
            v->disp.x -= (dx / dist) * force;
            v->disp.y -= (dy / dist) * force;
        }
        if (!u->dragging) {
            u->disp.x += (dx / dist) * force;
            u->disp.y += (dy / dist) * force;
        }
    }

    for (int i = 0; i < grapher_state->num_vertices; i++) {
        if (!grapher_state->vertices[i].dragging) {
            double dx = grapher_state->vertices[i].disp.x;
            double dy = grapher_state->vertices[i].disp.y;
            double dist = sqrt(dx * dx + dy * dy);
            double disp = fmin(dist, grapher_state->t);
            if (dist > 1e-8) {
                grapher_state->vertices[i].pos.x += (dx / dist) * disp;
                grapher_state->vertices[i].pos.y += (dy / dist) * disp;
            }

            if (grapher_state->vertices[i].pos.x < 0) grapher_state->vertices[i].pos.x = 0;
            if (grapher_state->vertices[i].pos.y < 0) grapher_state->vertices[i].pos.y = 0;
            if (grapher_state->vertices[i].pos.x > WIDTH) grapher_state->vertices[i].pos.x = WIDTH;
            if (grapher_state->vertices[i].pos.y > HEIGHT) grapher_state->vertices[i].pos.y = HEIGHT;
        }
    }

    grapher_state->t = cool(grapher_state->t);
    pthread_mutex_unlock(&grapher_state->change_graph_mutex);

    gtk_widget_queue_draw(grapher_state->drawing_area);
    return TRUE;
}

gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    GrapherState *grapher_state = (GrapherState *) data;

    for (int i = 0; i < grapher_state->num_vertices; i++) {
        double dx = event->x - grapher_state->vertices[i].pos.x;
        double dy = event->y - grapher_state->vertices[i].pos.y;
        if (sqrt(dx * dx + dy * dy) < 10) {
            grapher_state->vertices[i].dragging = TRUE;
            grapher_state->dragged_vertex = &grapher_state->vertices[i];
            break;
        }
    }
    return TRUE;
}

gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    GrapherState *grapher_state = (GrapherState *) data;

    if (grapher_state->dragged_vertex) {
        grapher_state->dragged_vertex->dragging = FALSE;
        grapher_state->dragged_vertex = NULL;
    }
    return TRUE;
}

gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    GrapherState *grapher_state = (GrapherState *) data;

    if (grapher_state->dragged_vertex) {
        grapher_state->dragged_vertex->pos.x = event->x;
        grapher_state->dragged_vertex->pos.y = event->y;
        gtk_widget_queue_draw(widget);
    }
    return TRUE;
}

void handle_reset(GtkButton *btn, gpointer data) {
    GrapherState *grapher_state = (GrapherState *) data;

    grapher_state->t = 5.0;
    grapher_state->curr_iteration = 0;
    // TODO check this
    g_timeout_add(16, simulate, grapher_state);
}


// topology change utils for grapher
int add_vertex_to_graph(GrapherState *grapher_state, uint8_t *interface_ip) {
    pthread_mutex_lock(&grapher_state->change_graph_mutex);
    if (grapher_state->num_vertices >= GRAPHER_MAX_VERTICES) {
        return -1;
    }

    grapher_state->vertices[grapher_state->num_vertices].num_interfaces = 0;
    grapher_state->vertices[grapher_state->num_vertices].interfaces = malloc(MAX_NUM_INTERFACES * sizeof(InterfaceTableEntry));

    memcpy(
        grapher_state->vertices[grapher_state->num_vertices].interfaces[0].interface_ip,
        interface_ip,
        4
    );
    grapher_state->vertices[grapher_state->num_vertices].num_interfaces += 1;

    grapher_state->vertices[grapher_state->num_vertices].pos.x = rand() % WIDTH;
    grapher_state->vertices[grapher_state->num_vertices].pos.y = rand() % HEIGHT;
    grapher_state->vertices[grapher_state->num_vertices].disp.x = 0;
    grapher_state->vertices[grapher_state->num_vertices].disp.y = 0;
    grapher_state->vertices[grapher_state->num_vertices].dragging = FALSE;
    grapher_state->num_vertices += 1;
    pthread_mutex_unlock(&grapher_state->change_graph_mutex);
    return 0;
}

int remove_edge_between_interfaces(GrapherState *grapher_state, uint8_t *ip_one, uint8_t *ip_two) {
    if (grapher_state->num_edges == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < grapher_state->num_edges; i++) {
        Edge *curr_edge = &grapher_state->edges[i];
        if (
            (match_ips(curr_edge->v->interfaces[0].interface_ip, ip_one) && (match_ips(curr_edge->u->interfaces[0].interface_ip, ip_two))) ||
            (match_ips(curr_edge->v->interfaces[0].interface_ip, ip_two) && (match_ips(curr_edge->u->interfaces[0].interface_ip, ip_one)))
        ) {
            if (i == grapher_state->num_edges - 1) {
                memset(&grapher_state->edges[i], 0, sizeof(Edge));
            }
            else {
                memcpy(
                    &grapher_state->edges[i],
                    &grapher_state->edges[grapher_state->num_edges - 1],
                    sizeof(Edge)
                );
                memset(&grapher_state->edges[grapher_state->num_edges - 1], 0, sizeof(Edge));
            }

            grapher_state->num_edges -= 1;
            return 0;
        }
    }

    return -1;
}

int add_edge_to_graph(GrapherState *grapher_state, int index_one, int index_two) {
    if (grapher_state->num_edges >= GRAPHER_MAX_EDGES) {
        return -1;
    }

    grapher_state->edges[grapher_state->num_edges].v = &grapher_state->vertices[index_one];
    grapher_state->edges[grapher_state->num_edges].u = &grapher_state->vertices[index_two];
    grapher_state->num_edges += 1;
    return 0;
}


int graph_contains_interface_ip(GrapherState *grapher_state, uint8_t *interface_ip) {
    for (uint32_t i = 0; i < grapher_state->num_vertices; i++) {
        if (match_ips(grapher_state->vertices[i].interfaces[0].interface_ip, interface_ip)) {
            return 1;
        }
    }

    return 0;
}

NeighborsState *get_neighbors_of_vertex(GrapherState *grapher_state, uint8_t *interface_ip) {
    NeighborsState *neighbors_state = malloc(sizeof(NeighborsState));
    neighbors_state->neighbors = malloc(GRAPHER_MAX_VERTICES * sizeof(NeighborVert));
    neighbors_state->num_neighbors = 0;

    for (uint32_t i = 0; i < grapher_state->num_edges; i++) {
        if (match_ips(grapher_state->edges[i].v->interfaces[0].interface_ip, interface_ip)) {
            memcpy(
                &neighbors_state->neighbors[neighbors_state->num_neighbors].vert,
                grapher_state->edges[i].u,
                sizeof(Vertex)
            );
            neighbors_state->neighbors[neighbors_state->num_neighbors].checked = 0;
            neighbors_state->num_neighbors += 1;
        }
        else if (match_ips(grapher_state->edges[i].u->interfaces[0].interface_ip, interface_ip)) {
            memcpy(
                &neighbors_state->neighbors[neighbors_state->num_neighbors].vert,
                grapher_state->edges[i].v,
                sizeof(Vertex)
            );
            neighbors_state->neighbors[neighbors_state->num_neighbors].checked = 0;
            neighbors_state->num_neighbors += 1;
        }
    }

    return neighbors_state;
}

int get_index_of_vertex_in_graph(GrapherState *grapher_state, uint8_t *interface_ip) {
    for (uint32_t i = 0; i < grapher_state->num_vertices; i++) {
        if (match_ips(grapher_state->vertices[i].interfaces[0].interface_ip, interface_ip)) {
            return i;
        }
    }

    return -1;
}

NeighborVert* find_vertex_in_neighbors_state(NeighborsState *neighbors_state, uint8_t *interface_ip) {
    for (uint32_t i = 0; i < neighbors_state->num_neighbors; i++) {
        if (match_ips(neighbors_state->neighbors[i].vert.interfaces[0].interface_ip, interface_ip)) {
            return &neighbors_state->neighbors[i];
        }
    }

    return NULL;
}



void *grapher_listen(void *arg_grapher_state) {
    GrapherState *grapher_state = (GrapherState *) arg_grapher_state;

    int sock;
    struct sockaddr_in listen_addr, sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    uint8_t rec_buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        free(grapher_state->vertices);
        free(grapher_state->edges);
        pthread_mutex_destroy(&grapher_state->change_graph_mutex);
        free(grapher_state);
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    int setsock_res = setsockopt(sock,
            SOL_SOCKET, SO_REUSEADDR,
            &reuse, sizeof(reuse)
    );
    if (setsock_res < 0) {
        perror("setsockopt with SO_REUSEADDR failed");
        close(sock);
        free(grapher_state->vertices);
        free(grapher_state->edges);
        pthread_mutex_destroy(&grapher_state->change_graph_mutex);
        free(grapher_state);
        exit(EXIT_FAILURE);
    }

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(BROADCAST_PORT);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bind_res = bind(sock,
        (struct sockaddr*) &listen_addr,
        sizeof(listen_addr)
    );
    if (bind_res < 0) {
        perror("bind failed");
        close(sock);
        free(grapher_state->vertices);
        free(grapher_state->edges);
        pthread_mutex_destroy(&grapher_state->change_graph_mutex);
        free(grapher_state);
        exit(EXIT_FAILURE);
    }

    while (1) {
        int bytes_received = recvfrom(sock,
                rec_buffer, BUFFER_SIZE - 1,
                0,
                (struct sockaddr*) &sender_addr,
                &addr_len
        );
        if (bytes_received < 0) {
            perror("recvform failed");
            close(sock);
            free(grapher_state->vertices);
            free(grapher_state->edges);
            pthread_mutex_destroy(&grapher_state->change_graph_mutex);
            free(grapher_state);
            exit(EXIT_FAILURE);
        }

        if (bytes_received == 1 && rec_buffer[0] == 1) {
            continue;
        }

        RouterState *rec_router_state = malloc(sizeof(RouterState));
        rec_router_state->router_table = malloc(ROUTER_TABLE_MAX_SIZE * sizeof(RouterTableEntry));
        rec_router_state->interfaces = malloc(MAX_NUM_INTERFACES * sizeof(InterfaceTableEntry));
        memcpy(rec_router_state->interfaces[0].interface_ip, rec_buffer, 4);
        memcpy(&rec_router_state->router_id, rec_buffer + 4, 4);
        memcpy(&rec_router_state->num_entries, rec_buffer + 8, 4);
        memcpy(
            rec_router_state->router_table,
            rec_buffer + 12,
            rec_router_state->num_entries * sizeof(RouterTableEntry)
        );
        
        // add vertex for received router if not in graph
        if (!graph_contains_interface_ip(
            grapher_state,
            rec_router_state->interfaces[0].interface_ip
        )) {
            add_vertex_to_graph(grapher_state, rec_router_state->interfaces[0].interface_ip);
        }

        // the received router table is from a host (it has only one entry -> itself)
        // we shouldn't update his neighbors in the graph
        if (rec_router_state->num_entries == 1) {
            continue;
        }

        int curr_router_index_in_graph =
            get_index_of_vertex_in_graph(grapher_state, rec_router_state->interfaces[0].interface_ip);
        if (curr_router_index_in_graph < 0) {
            free(rec_router_state->router_table);
            free(rec_router_state->interfaces);
            free(rec_router_state);
            free(grapher_state->vertices);
            free(grapher_state->edges);
            pthread_mutex_destroy(&grapher_state->change_graph_mutex);
            free(grapher_state);
            exit(EXIT_FAILURE);
        }


        // get neighbors of current router vertex
        NeighborsState *neighbors_state =
            get_neighbors_of_vertex(grapher_state, rec_router_state->interfaces[0].interface_ip);

        pthread_mutex_lock(&grapher_state->change_graph_mutex);
        for (uint32_t i = 0; i < rec_router_state->num_entries; i++) {
            if (rec_router_state->router_table[i].metric == 1) {
                uint8_t *ip_to_find = rec_router_state->router_table[i].destination;
                NeighborVert *found_vertex =
                    find_vertex_in_neighbors_state(neighbors_state, ip_to_find);

                if (found_vertex != NULL) {
                    // neighbor already exists. make sure it stays in the graph
                    found_vertex->checked = 1;
                } else {
                    // if vertex exists in the graph but is not a neighbor then
                    // vertex needs to be added as a neighbor to the current router's vertex
                    int found_index_in_graph = 
                        get_index_of_vertex_in_graph(grapher_state, ip_to_find);
                    if (found_index_in_graph < 0) {
                        continue;
                    }

                    add_edge_to_graph(
                        grapher_state,
                        curr_router_index_in_graph,
                        found_index_in_graph
                    );
                }
            }
        }

        for (uint32_t i = 0; i < neighbors_state->num_neighbors; i++) {
            if (neighbors_state->neighbors[i].checked == 0) {
                // this vertex is no longer a neighbor of the router vertex.
                remove_edge_between_interfaces(
                    grapher_state,
                    rec_router_state->interfaces[0].interface_ip,
                    neighbors_state->neighbors[i].vert.interfaces[0].interface_ip
                );
            }
        }
        pthread_mutex_unlock(&grapher_state->change_graph_mutex);

        // free neighbors state
        free(neighbors_state->neighbors);
        free(neighbors_state);

        // free received router state
        free(rec_router_state->router_table);
        free(rec_router_state->interfaces);
        free(rec_router_state);
    }

    close(sock);
    log_printf("grapher_listen ended\n");
    return NULL;
}

GrapherState *startup_grapher() {
    GrapherState *grapher_state = malloc(sizeof(GrapherState));
    grapher_state->vertices = malloc(GRAPHER_MAX_VERTICES * sizeof(Vertex));
    grapher_state->edges = malloc(GRAPHER_MAX_EDGES * sizeof(Edge));
    pthread_mutex_init(&grapher_state->change_graph_mutex, NULL);

    grapher_state->num_vertices = 0;
    grapher_state->num_edges = 0;
    grapher_state->num_edges = 0;
    grapher_state->curr_iteration = 0;
    grapher_state->t = 5.0;
    grapher_state->drawing_area = gtk_drawing_area_new();
    grapher_state->dragged_vertex = NULL;

    // to delete
    uint8_t test_ip_one[4] = { 0, 0, 0, 0 };
    add_vertex_to_graph(grapher_state, test_ip_one);
    // end to delete

    return grapher_state;
}

int split_threads(GrapherState *grapher_state) {
    int is_thread_error = 0;
    pthread_t threads[1];

    int rc_one = pthread_create(&threads[0], NULL, grapher_listen, (void*) grapher_state);
    if (rc_one) {
        free(grapher_state->vertices);
        free(grapher_state->edges);
        pthread_mutex_destroy(&grapher_state->change_graph_mutex);
        free(grapher_state);
        return -1;
    }

    return 0;
}


// ---- Main ----
int main(int argc, char *argv[]) {
    srand(time(NULL));
    gtk_init(&argc, &argv);

    GrapherState *grapher_state = startup_grapher();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Force Graph GTK");
    gtk_window_set_default_size(GTK_WINDOW(window), WIDTH, HEIGHT);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    gtk_widget_set_size_request(grapher_state->drawing_area, WIDTH, HEIGHT);
    gtk_box_pack_start(GTK_BOX(vbox), grapher_state->drawing_area, TRUE, TRUE, 0);

    GtkWidget *button = gtk_button_new_with_label("Reset");
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

    g_signal_connect(grapher_state->drawing_area, "draw", G_CALLBACK(draw_graph), grapher_state);
    g_signal_connect(grapher_state->drawing_area, "button-press-event", G_CALLBACK(on_button_press), grapher_state);
    g_signal_connect(grapher_state->drawing_area, "button-release-event", G_CALLBACK(on_button_release), grapher_state);
    g_signal_connect(grapher_state->drawing_area, "motion-notify-event", G_CALLBACK(on_motion_notify), grapher_state);

    gtk_widget_add_events(grapher_state->drawing_area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect(button, "clicked", G_CALLBACK(handle_reset), grapher_state);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), grapher_state);

    gtk_widget_show_all(window);
    g_timeout_add(16, simulate, grapher_state);

    int split_threads_rc = split_threads(grapher_state);
    if (split_threads_rc < 0) {
        exit(EXIT_FAILURE);
    }

    gtk_main();

    free(grapher_state->vertices);
    free(grapher_state->edges);
    pthread_mutex_destroy(&grapher_state->change_graph_mutex);
    free(grapher_state);
    return 0;
}
