
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <CL/opencl.h>

#include "map_file.h"
#include "graph_essentials.h"
#include "macros.h"

struct cl_data_t {
	cl_device_id device;
	cl_context context;
	cl_command_queue command_queue;
	cl_program program;

	cl_mem cl_image_map;
	cl_mem cl_buffer_mask;
	
	cl_mem cl_buffer_gid_row_index;
	cl_mem cl_buffer_gid_row;

	mask_cell* mask_row;
	size_t vertex_count;
};

#define usedcount 1
#define MAX_KERNEL_FILE_SIZE 0x8FFF

int setup_environment(const char*, struct cl_data_t*, struct bmp_map*);
int parse_map(struct cl_data_t*, struct bmp_map*);
int apply_colors_and_mask(struct cl_data_t*, struct bmp_map*, struct graph_as_row_t*);
int build_graph(struct graph_as_row_t*, struct cl_data_t*, struct bmp_map*, unsigned char);
void distruct_environment(struct cl_data_t*, struct bmp_map*);