#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <intrin.h>

typedef unsigned long gid_t;
typedef unsigned long color_id_t;

#define gid_reserved		2

#define color_undefined		0
#define color_start_value	1

typedef uint32_t bitfield_cell;
#define bitfield_cell_flags_count (sizeof(bitfield_cell) * 8)

enum SORT_GRAPH_TYPE {
	BY_LINKS_COUNT
};

struct vertex_t {
	gid_t id; // is it needed
	color_id_t color_id;
	int links_count;
	bitfield_cell* edges;
};

struct graph_as_row_t {
	struct vertex_t* vertex_row;
	struct vertex_t** order;
	bitfield_cell* matrix;
	size_t matrix_size;
	size_t matrix_column_size;
	size_t vertex_count;
	size_t used_colors_count;
};

struct gid_row_t {
	gid_t* gid_row;
	size_t* gid_row_index;
	size_t gid_row_size;
};

int graph_init_grid_row(struct gid_row_t*);

int graph_init_as_row(struct graph_as_row_t*, size_t, unsigned char);

void distruct_graph_as_row(struct graph_as_row_t*);

int graph_calc_links(struct graph_as_row_t*, unsigned char);

void graph_reset_colors(struct graph_as_row_t*);

int graph_coloring(struct graph_as_row_t*);

void graph_display(struct graph_as_row_t*, unsigned char);