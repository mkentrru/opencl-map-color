#include "graph_essentials.h"

const gid_t gr_gid_reserver_undefinded = 0;
const gid_t gr_gid_reserver_border = 1;

int graph_init_grid_row(struct gid_row_t* r) {
	r->gid_row = (gid_t*)calloc(r->gid_row_size, sizeof(gid_t));
	if (r->gid_row == NULL) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}


void distruct_graph_as_row(struct graph_as_row_t* g) {
	if (g->matrix) free(g->matrix);

}

int graph_init_as_row(struct graph_as_row_t* g, size_t vertex_count, 
	unsigned char matrix_link_flag_value
) {

	g->vertex_row = (struct vertex_t*)calloc(vertex_count, sizeof(struct vertex_t));
	if (g->vertex_row == NULL) return EXIT_FAILURE;
	size_t matrix_column_size = (vertex_count + 1) / bitfield_cell_flags_count;
	if ((vertex_count + 1) % bitfield_cell_flags_count)
		matrix_column_size++;

	g->order = (struct vertex_t**)calloc(vertex_count, sizeof(struct vertex_t*));
	if (g->order == NULL) return EXIT_FAILURE;

	g->matrix_size = (vertex_count + 1) * matrix_column_size * sizeof(bitfield_cell);

	/*g->matrix = (bitfield_cell*)calloc(
		(vertex_count + 1) * matrix_column_size,
		sizeof(bitfield_cell));*/
	g->matrix = (bitfield_cell*)malloc(g->matrix_size);
	
	if (g->matrix == NULL) return EXIT_FAILURE;

	if (matrix_link_flag_value) // if set 0 as link, any flag should be 1 at first
		memset(g->matrix, 0, g->matrix_size);
	else memset(g->matrix, UINT8_MAX, g->matrix_size);
	

	bitfield_cell* p = g->matrix + matrix_column_size;
	
	bitfield_cell zero_pos = 1;
	if (!matrix_link_flag_value) zero_pos = ~zero_pos;

	for (size_t i = 1; i < vertex_count + 1; i++) {
		(g->vertex_row + i)->id = i;
		(g->vertex_row + i)->edges = p;
		(g->vertex_row + i)->links_count = 0;
		*p &= zero_pos;
		(g->vertex_row + i)->color_id = color_undefined;
		p += matrix_column_size;
		g->order[i - 1] = (g->vertex_row + i);
	}

	g->matrix_column_size = matrix_column_size;
	g->vertex_count = vertex_count;

	return EXIT_SUCCESS;
}


int graph_calc_links(struct graph_as_row_t* g, unsigned char matrix_link_flag_value) {
	for (size_t i = 1; i < g->vertex_count + 1; i++) {
		size_t lc = 0;
		for (size_t a = 1; a < g->vertex_count + 1; a++) {
			size_t pos = i * g->matrix_column_size + a / bitfield_cell_flags_count;
			bitfield_cell flag = 1 << (a % bitfield_cell_flags_count);
			if (!matrix_link_flag_value) {
				if (~g->matrix[pos] & flag) {
					lc++;
				}
			}
			else {
				if (g->matrix[pos] & flag) {
					lc++;
				}
			}
		}
		(g->vertex_row + i)->links_count = lc;
	}
	return EXIT_SUCCESS;
}


void display_colors(struct graph_as_row_t* g) {
	printf("\n");
	for (size_t b_index = 1; b_index < g->vertex_count + 1; b_index++) {
		printf("%2d: color: %2d;\n", b_index, (g->vertex_row + b_index)->color_id);
	}
}

int graph_compare_vertex_links_count(const void* a, const void* b) {
	return (*((struct vertex_t**)b))->links_count - (*((struct vertex_t**)a))->links_count;
}

int graph_sort_vertex_order(struct graph_as_row_t* g, enum SORT_GRAPH_TYPE type) {
	
	switch (type)
	{
	case BY_LINKS_COUNT:
		qsort(g->order, g->vertex_count, sizeof(struct vertex_t*), graph_compare_vertex_links_count);
		break;
	default:
		return EXIT_FAILURE;
		break;
	}

	return EXIT_SUCCESS;
}

void graph_reset_colors(struct graph_as_row_t* g) {
	for (size_t i = 1; i < g->vertex_count + 1; i++) {
		(g->vertex_row + i)->color_id = color_undefined;
	}
}


color_id_t vertex_get_neighbours_color(struct graph_as_row_t* g, size_t vid) {
	size_t n_index = 0;
	color_id_t res = color_undefined;
	for (size_t cell_index = 0; cell_index < g->matrix_column_size; cell_index++) {
		bitfield_cell mask = g->vertex_row[vid].edges[cell_index];
		size_t cell_offset = cell_index * bitfield_cell_flags_count;
		while (_BitScanForward(&n_index, mask)) {
			res |= g->vertex_row[cell_offset + n_index].color_id;
			mask ^= (1 << n_index);
		}
	}
	return res;
}

color_id_t vertex_get_available_color(struct graph_as_row_t* g, size_t vid, color_id_t* used_colors) {
	struct vertex_t* v = (g->vertex_row + vid);
	color_id_t allowed = ~vertex_get_neighbours_color(g, vid), c = 0;
	size_t bit_index = 0;

	/*
	_BitScanForward(&bit_index, allowed);
	c <<= bit_index;
	*used_colors |= c;
	*/
	
	__asm {
		mov ecx, used_colors;
		bsf	ebx, allowed;
		bts [ecx], ebx;
		bts c, ebx;
	}
	return c;
}

void mix_order(struct graph_as_row_t* g) {
	

	/*for (int i = 0; i < g->vertex_count; i++) {
		printf("%2d; ", g->order[i]->id);
	}
	printf("\n");*/

	for (int j = 0; j < 1 + g->vertex_count / 3; j++) {

		int i1 = 0, i2 = 0;
		while (i1 == i2) {
			i1 = rand() % g->vertex_count;
			i2 = rand() % g->vertex_count;
		}
		struct vertex_t* tmp = g->order[i1];
		g->order[i1] = g->order[i2];
		g->order[i2] = tmp;

	}
	/*for (int i = 0; i < g->vertex_count; i++) {
		printf("%2d; ", g->order[i]->id);
	}
	printf("\n");*/
}


int graph_coloring(struct graph_as_row_t* g) {

	graph_sort_vertex_order(g, BY_LINKS_COUNT);

	color_id_t used_colors = 0; size_t used_colors_count = 0;



	while(used_colors_count > 4 || !used_colors_count) {
		
		graph_reset_colors(g);

		used_colors = 0; used_colors_count = 0;
		struct vertex_t* cv = NULL;
		for (size_t vid = 0; vid < g->vertex_count; vid++) {
			cv = g->order[vid];
			cv->color_id = vertex_get_available_color(g, cv->id, &used_colors);
		}
		/*
		_BitScanForward(&used_colors_count, ~used_colors);
		*/
		__asm {
			not used_colors;
			bsf	ebx, used_colors;
			mov used_colors_count, ebx;
		}
		
		mix_order(g);
	}

	printf("\n\t< Colors used: %d;\n", used_colors_count);
	g->used_colors_count = used_colors_count;
	return EXIT_SUCCESS;
}

void graph_display(struct graph_as_row_t* g, unsigned char matrix_link_flag_value) {
	for (size_t i = 1; i < g->vertex_count + 1; i++) {
		printf("\n%3d (%3d/%3d):", i, (g->vertex_row + i)->id, (g->vertex_row + i)->links_count);
		for (size_t a = 1; a < g->vertex_count + 1; a++) {
			size_t pos = i * g->matrix_column_size + a / bitfield_cell_flags_count;
			bitfield_cell flag = 1 << (a % bitfield_cell_flags_count);
			if (!matrix_link_flag_value) {
				if (~g->matrix[pos] & flag) {
					printf(" %2d;", a);
				}
			}
			else {
				if (g->matrix[pos] & flag) {
					printf(" %2d;", a);
				}
			}
		}
		//printf("\n\t0x(%8x"/* %8x*/")\n", g->matrix[i * g->matrix_column_size]);// , g->matrix[i * g->matrix_column_size + 1]);
	}
	printf("\n");
}