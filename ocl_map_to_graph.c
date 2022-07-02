#include "ocl_map_to_graph.h"

int setup_device (struct cl_data_t* cld) {

	cl_platform_id platforms;
	cl_uint num_platforms;

	cl_int callres = CL_SUCCESS;
	callres = clGetPlatformIDs(
		usedcount,
		&platforms,
		&num_platforms
	);
	check(callres != CL_SUCCESS, "No platforms detected", callres)
	//printf("Detected: %d platform;\n", num_platforms);

	cl_uint num_devices = 0;
	callres = clGetDeviceIDs(
		platforms,
		CL_DEVICE_TYPE_DEFAULT,
		usedcount,
		&cld->device,
		&num_devices
	);
	check(callres != CL_SUCCESS, "No devices detected", callres)
	//printf("Detected: %d devices;\n", num_devices);

	cld->context = clCreateContext(
		NULL,
		usedcount,
		&cld->device,
		NULL, // create
		NULL,
		&callres
	);
	check(callres != CL_SUCCESS, "Cannot create context", callres)
	//printf("Context created.\n");

	cld->command_queue = clCreateCommandQueueWithProperties(
		cld->context, 
		cld->device,
		NULL,
		&callres
	);
	check(callres != CL_SUCCESS, "Cannot create command queue", callres)
	//printf("Created command queue.\n");

	return EXIT_SUCCESS;
}

int setup_program (const char* kernel_file_name, struct cl_data_t* cld) {
	char* kernel_source = NULL;
	FILE* kernel_file = NULL;
	size_t kernel_file_size = 0;
	cl_int cl_callres = CL_SUCCESS;

	kernel_file = fopen(kernel_file_name, "r");
	check(kernel_file == NULL, "Cannot open kernel file", EXIT_FAILURE)

	kernel_source = (char*)calloc(MAX_KERNEL_FILE_SIZE, sizeof(char));
	check(kernel_source == NULL, "Cannot allocate memory for kernel code", EXIT_FAILURE)

	kernel_file_size = fread(kernel_source, sizeof(char), MAX_KERNEL_FILE_SIZE, kernel_file);

	cld->program = clCreateProgramWithSource(
		cld->context, //context,
		usedcount,
		&kernel_source,
		&kernel_file_size,
		&cl_callres
	);
	check(cl_callres != CL_SUCCESS, "Cannot create program", cl_callres)
	//printf("Created program.\n");

	cl_callres = clBuildProgram(
		cld->program, //program,
		usedcount,
		&cld->device, //&device,
		NULL,
		NULL,
		NULL
	);
	check(cl_callres != CL_SUCCESS, "Cannot build program", cl_callres)
	//printf("Builded program.\n");
	
	return EXIT_SUCCESS;
}

int setup_shared_buffers(struct cl_data_t* cld, struct bmp_map* bmp) {
	cl_int cl_callres = CL_SUCCESS;

	cl_image_format map_format = {
		CL_RGBA,
		CL_UNSIGNED_INT8
	};

	cl_image_desc map_desc = {
		CL_MEM_OBJECT_IMAGE2D,
		bmp->image_width,
		bmp->image_height,
		1, // depth
		1, // images array size
		bmp->image_row_pitch,
		0, // slice
		0, // mip level?
		0, // samples??
		NULL // buffer for 1D
	};

	cld->cl_image_map = clCreateImage(
		cld->context,
		CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		&map_format,
		&map_desc,
		bmp->linear_sequence,
		&cl_callres
	);
	check(cl_callres != CL_SUCCESS, "Cannot create image", cl_callres)
	//printf("Created image buffer.\n");

	cld->mask_row = (mask_cell*)calloc(bmp->mask_size, sizeof(mask_cell));
	check(cld->mask_row == NULL, "Cannot allocate memory for mask", EXIT_FAILURE)

	//printf("Allocated mask row.\n");

	cld->cl_buffer_mask = clCreateBuffer(
		cld->context,
		CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		bmp->mask_size * sizeof(mask_cell),
		cld->mask_row,
		&cl_callres
	);
	check(cl_callres != CL_SUCCESS, "Cannot create mask buffer", cl_callres)
	//printf("Created mask buffer.\n");

	return EXIT_SUCCESS;
}

void init_setup_environment(struct cl_data_t* cld) {
	memset(cld, 0, sizeof(struct cl_data_t));
}

void distruct_environment(struct cl_data_t* cld, struct bmp_map* bmp) {
	distruct_bmp_map(bmp);
	if (cld->device) clReleaseDevice(cld->device);
	if (cld->context) clReleaseContext(cld->context);
	//if (cld->command_queue) clReleaseCommandQueue(cld->command_queue);
	if (cld->program) clReleaseProgram(cld->program);
	if (cld->cl_image_map) clReleaseMemObject(cld->cl_image_map);
	if (cld->cl_buffer_mask) clReleaseMemObject(cld->cl_buffer_mask);
	if (cld->cl_buffer_gid_row_index)clReleaseMemObject(cld->cl_buffer_gid_row_index);
	if (cld->cl_buffer_gid_row) clReleaseMemObject(cld->cl_buffer_gid_row);
	//if (cld->mask_row) free(cld->mask_row);
}

int setup_environment(const char* kernel_file_name, struct cl_data_t* cld, struct bmp_map* bmp) {
	int callres = EXIT_SUCCESS;
	init_setup_environment(cld);
	// choose device
	if (setup_device(cld) != EXIT_SUCCESS) {
		distruct_environment(cld, bmp);
		return EXIT_FAILURE;
	}

	// create and build program
	if (setup_program(kernel_file_name, cld) != EXIT_SUCCESS) {
		distruct_environment(cld, bmp);
		return EXIT_FAILURE;
	}

	// create buffers
	if (setup_shared_buffers(cld, bmp) != EXIT_SUCCESS) {
		distruct_environment(cld, bmp);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int cl_mask_border(struct cl_data_t* cld, struct bmp_map* bmp) {
	cl_int cl_callres = CL_SUCCESS;
	cl_kernel mask_border = NULL;
	int callres = EXIT_SUCCESS;

	mask_border = clCreateKernel(cld->program, "mask_border", &cl_callres);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create mask_border kernel", cl_callres)

	cl_callres |= clSetKernelArg(mask_border, 0, sizeof(cl_mem), (void*)&cld->cl_image_map);
	cl_callres |= clSetKernelArg(mask_border, 1, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set mask_border kernel args", cl_callres)

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		mask_border,
		2, NULL, // offset
		(size_t[2]) { bmp->image_width, bmp->image_height }, //g size
		NULL, 0, NULL, NULL);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel mask_border execution error", cl_callres)
	clFinish(cld->command_queue);
free_temporary_resources:
	if (mask_border) clReleaseKernel(mask_border);

	return callres;
}

int cl_premask_area(struct cl_data_t* cld, struct bmp_map* bmp, size_t spread_timeout) {
	int callres = EXIT_SUCCESS;
	cl_int cl_callres = CL_SUCCESS;
	cl_kernel premask_area = NULL;

	premask_area = clCreateKernel( cld->program, "premask_area", &cl_callres );
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create premask_area kernel", cl_callres)

	cl_callres |= clSetKernelArg(premask_area, 0, sizeof(size_t), (void*)&bmp->image_width);
	cl_callres |= clSetKernelArg(premask_area, 1, sizeof(size_t), (void*)&bmp->image_height);
	cl_callres |= clSetKernelArg(premask_area, 2, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);
	cl_callres |= clSetKernelArg(premask_area, 3, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row_index);
	cl_callres |= clSetKernelArg(premask_area, 4, sizeof(size_t), (void*)&spread_timeout);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set premask_area kernel args", cl_callres)

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,
		premask_area,
		1, // dims
		NULL, // offset
		&bmp->mask_size, //g size
		NULL, // l size
		0, // num events
		NULL,
		NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel premask_area execution error", cl_callres)
	clFinish(cld->command_queue);

free_temporary_resources:
	if (premask_area) clReleaseKernel(premask_area);
	return callres;
}

int init_gid_row_index(struct cl_data_t* cld, struct gid_row_t* r) {
	cl_int cl_callres = CL_SUCCESS;
	r->gid_row_size = 0;
	r->gid_row = NULL;
	r->gid_row_index = (size_t*)calloc(1, sizeof(size_t));
	check(r->gid_row_index == NULL, "Cannot allocate memory for gid row index", EXIT_FAILURE)

	* (r->gid_row_index) = gid_reserved;

	cld->cl_buffer_gid_row_index = clCreateBuffer(
		cld->context,
		CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		sizeof(gid_t),
		r->gid_row_index,
		&cl_callres
	);
	check(cl_callres != CL_SUCCESS, "Cannot create gid_row_index buffer", cl_callres)
	
	clFinish(cld->command_queue);
	return EXIT_SUCCESS;
}

int cl_set_gid_row(struct cl_data_t* cld, struct gid_row_t* r) {
	cl_int cl_callres = CL_SUCCESS;
	int callres = EXIT_SUCCESS;
	cl_kernel set_gid_row = NULL;

	cl_callres = clEnqueueReadBuffer(
		cld->command_queue, //command_queue
		cld->cl_buffer_gid_row_index,
		CL_TRUE,
		0,
		sizeof(size_t),
		r->gid_row_index,
		0,
		NULL,
		NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot read cl_buffer_gid_row_index buffer", cl_callres)

	r->gid_row_size = *(r->gid_row_index);
	//printf("Note: gid_row_size: %u;\n", r->gid_row_size);
	callres = graph_init_grid_row(r);
	check_goto_temp(callres == EXIT_FAILURE, "Cannot init gid row", EXIT_FAILURE);

	cld->cl_buffer_gid_row = clCreateBuffer(
		cld->context,
		CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		r->gid_row_size * sizeof(gid_t),
		r->gid_row,
		&cl_callres
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create buffer for gid_row", cl_callres)

	set_gid_row = clCreateKernel(cld->program, "set_gid_row", &cl_callres);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create set_gid_row kernel", cl_callres)

	cl_callres |= clSetKernelArg(set_gid_row, 0, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set set_gid_row kernel args", cl_callres)
	

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		set_gid_row,
		1, // dims
		NULL, // offset
		&r->gid_row_size, //g size
		NULL, // l size
		0, // num events
		NULL,
		NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel set_gid_row execution error", cl_callres)
	clFinish(cld->command_queue);

free_temporary_resources:
	if (set_gid_row) clReleaseKernel(set_gid_row);
	return callres;
}

int cl_normalise_mask_area(struct cl_data_t* cld, struct bmp_map* bmp) {
	cl_int cl_callres = CL_SUCCESS;
	int callres = EXIT_SUCCESS;
	size_t normalize_size = bmp->image_width + bmp->image_height;

	cl_kernel normalise_mask_area = NULL;
	cl_kernel apply_parent_gid = NULL;

	normalise_mask_area = clCreateKernel(
		cld->program, //program,
		"normalise_mask_area",
		&cl_callres
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create normalise_mask_area kernel", cl_callres)

	apply_parent_gid = clCreateKernel(
		cld->program, //program,
		"apply_parent_gid",
		&cl_callres
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create apply_parent_gid kernel", cl_callres)

	/*cl_mem cl_buffer_semaphor = clCreateBuffer(
		cld->context,
		CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
		sizeof(int),
		NULL,
		&cl_callres
	);
	check(cl_callres != CL_SUCCESS, "Cannot create semaphore buffer", cl_callres)

	int sema_default_value = 0;
	cl_callres = clEnqueueWriteBuffer(
		cld->command_queue,
		cl_buffer_semaphor,
		CL_TRUE,
		0,
		sizeof(int),
		&sema_default_value,
		0, NULL, NULL
	);

	check(cl_callres != CL_SUCCESS, "Cannot write to semaphore buffer", cl_callres)
	*/
	
	cl_callres |= clSetKernelArg(normalise_mask_area, 0, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);
	cl_callres |= clSetKernelArg(normalise_mask_area, 1, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row);
	cl_callres |= clSetKernelArg(normalise_mask_area, 2, sizeof(size_t), (void*)&bmp->image_width);
	cl_callres |= clSetKernelArg(normalise_mask_area, 3, sizeof(size_t), (void*)&bmp->image_height);
	//clSetKernelArg(normalise_mask_area, 4, sizeof(cl_mem), (void*)&cl_buffer_semaphor);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set normalise_mask_area kernel args", cl_callres)
	

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		normalise_mask_area,
		1, // dims
		NULL,
		&normalize_size, //g size
		NULL, // l size
		0, NULL, NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel normalise_mask_area execution error", cl_callres)
	
	cl_callres |= clSetKernelArg(apply_parent_gid, 0, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);
	cl_callres |= clSetKernelArg(apply_parent_gid, 1, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set apply_parent_gid kernel args", cl_callres)

	callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		apply_parent_gid,
		1, // dims
		NULL,
		&bmp->mask_size, //g size
		NULL, // l size
		0, NULL, NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel apply_parent_gid execution error", cl_callres)
	clFinish(cld->command_queue);

free_temporary_resources:
	if (normalise_mask_area) clReleaseKernel(normalise_mask_area);
	if (apply_parent_gid) clReleaseKernel(apply_parent_gid);

	return callres;
}

int cl_fix_gid(struct cl_data_t* cld, struct gid_row_t* r) {
	cl_int cl_callres = CL_SUCCESS;
	int callres = EXIT_SUCCESS;
	size_t reserved_gids = gid_reserved;

	cl_kernel normalise_gid = NULL;
	cl_kernel fix_gid = NULL;

	*(r->gid_row_index) = 1;
	cl_callres = clEnqueueWriteBuffer(
		cld->command_queue,
		cld->cl_buffer_gid_row_index,
		CL_TRUE, 0,
		sizeof(size_t),
		r->gid_row_index,
		0, NULL, NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot write to cl_buffer_gid_row_index buffer", cl_callres)

	fix_gid = clCreateKernel(
		cld->program,
		"fix_gid",
		&cl_callres
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create fix_gid kernel", cl_callres)

	normalise_gid = clCreateKernel(
		cld->program,
		"normalise_gid",
		&cl_callres
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create normalise_gid kernel", cl_callres)


	cl_callres |= clSetKernelArg(normalise_gid, 0, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set normalise_gid kernel args", cl_callres)
	
		cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		normalise_gid,
		1, // dims
		NULL, // offset
		&r->gid_row_size, //g size
		NULL, // l size
		0, // num events
		NULL,
		NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel normalise_gid execution error", cl_callres)

	cl_callres |= clSetKernelArg(fix_gid, 0, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row);
	cl_callres |= clSetKernelArg(fix_gid, 1, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row_index);
	cl_callres |= clSetKernelArg(fix_gid, 2, sizeof(size_t), (void*)&reserved_gids);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set fix_gid kernel args", cl_callres)


	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,
		fix_gid, 1, NULL,
		&r->gid_row_size,
		NULL, 0, NULL, NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel fix_gid execution error", cl_callres)

	cl_callres = clEnqueueReadBuffer(
		cld->command_queue, //command_queue
		cld->cl_buffer_gid_row_index,
		CL_TRUE, 0, sizeof(size_t),
		r->gid_row_index, 0, NULL, NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot read cl_buffer_gid_row_index buffer", cl_callres)
	
	clFinish(cld->command_queue);
	
	printf("\n\t< Areas found: %u;\n", *r->gid_row_index - 1);
	cld->vertex_count = *r->gid_row_index - 1;
free_temporary_resources:
	if (normalise_gid) clReleaseKernel(normalise_gid);
	if (fix_gid) clReleaseKernel(fix_gid);

	return callres;
}

int cl_finalize_mask(struct cl_data_t* cld, struct bmp_map* bmp, struct gid_row_t* r) {
	cl_int cl_callres = CL_SUCCESS;
	int callres = EXIT_SUCCESS;

	cl_kernel finalize_mask = NULL;

	finalize_mask = clCreateKernel(
		cld->program, //program,
		"finalize_mask",
		&callres
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot create finalize_mask kernel", cl_callres)

	cl_callres |= clSetKernelArg(finalize_mask, 0, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);
	cl_callres |= clSetKernelArg(finalize_mask, 1, sizeof(cl_mem), (void*)&cld->cl_buffer_gid_row);
	check_goto_temp(cl_callres != CL_SUCCESS, "Cannot set finalize_mask kernel args", cl_callres)

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		finalize_mask,
		1, // dims
		NULL, // offset
		&bmp->mask_size, //g size
		NULL, // l size
		0, // num events
		NULL,
		NULL
	);
	check_goto_temp(cl_callres != CL_SUCCESS, "Kernel finalize_mask execution error", cl_callres)
	clFinish(cld->command_queue);
free_temporary_resources:
	if (finalize_mask) clReleaseKernel(finalize_mask);

	return callres;
}

void display_mask(struct cl_data_t* cld, struct bmp_map* bmp, struct gid_row_t* r) {
	clFinish(cld->command_queue);

	clEnqueueReadBuffer(
		cld->command_queue, //command_queue,
		cld->cl_buffer_mask, //mask,
		CL_TRUE,
		0,
		bmp->mask_size * sizeof(mask_cell),
		cld->mask_row,
		0,
		NULL,
		NULL
	);

	clEnqueueReadBuffer(
		cld->command_queue, //command_queue,
		cld->cl_buffer_gid_row, //mask,
		CL_TRUE,
		0,
		r->gid_row_size * sizeof(gid_t),
		r->gid_row,
		0,
		NULL,
		NULL
	);

	clFinish(cld->command_queue);
	printf("\t\t");
	for (size_t i = 0; i < bmp->image_width; i++) printf("_%3d", i);

	for (size_t idx = 0; idx < bmp->mask_size; idx++) {
		if (idx % bmp->image_width == 0) printf("\n\t_%3d\t", idx / bmp->image_width);
		if (cld->mask_row[idx] == 0) printf(" %3c", '-');
		//else if (cld->mask_row[idx] == 1)printf(" %3c", ' ');
		else printf(" %3d", cld->mask_row[idx]);
	}
	
	printf("\n+\n");
	
	/*for (size_t i = 0; i < r->gid_row_size; i++) {
		printf("%d -> %d\n", i, r->gid_row[i]);
	}*/
	
	/*
	for (size_t idx = 0; idx < bmp->mask_size; idx++) {
		if (idx % bmp->image_width == 0) printf("\n");
		if (cld->mask_row[idx] == 0) printf(" %3c", '-');
		else if (cld->mask_row[idx] == 1)printf(" %3c", ' ');
		else printf(" %3d", r->gid_row[cld->mask_row[idx]]);
	}
	printf("\n");*/
}
	
int cl_debug_output(struct cl_data_t* cld, struct bmp_map* bmp) {
	cl_int cl_callres = CL_SUCCESS;
	cl_kernel debug_output = NULL;
	
	printf("\n\t< Debug output\n");

	debug_output = clCreateKernel(cld->program, "debug_output", &cl_callres);
	check(cl_callres != CL_SUCCESS, "Cannot create mask_border kernel", cl_callres)

	clSetKernelArg(debug_output, 0, sizeof(cl_mem), (void*)&cld->cl_image_map);
	clSetKernelArg(debug_output, 1, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		debug_output,
		2, NULL, // offset
		(size_t[2]) { bmp->image_width, bmp->image_height }, //g size
		NULL, 0, NULL, NULL
	);
	check(cl_callres != CL_SUCCESS, "Kernel debug_output execution error", cl_callres)

	clFinish(cld->command_queue);
	return EXIT_SUCCESS;
}


//void distruct_environment
void distruct_parse_map(struct cl_data_t* cld, struct bmp_map* bmp) {
	distruct_environment(cld, bmp);
}

int parse_map(struct cl_data_t* cld, struct bmp_map* bmp) {
	struct gid_row_t gr = { NULL, NULL, 0 };

	int callres = EXIT_SUCCESS;
	size_t spread_timeout = 1000;

	callres = cl_mask_border(cld, bmp); //
	if(callres != EXIT_SUCCESS) {
		distruct_parse_map(cld, bmp);
		temp
	}
	
	callres = init_gid_row_index(cld, &gr); //
	if (callres != EXIT_SUCCESS) {
		distruct_parse_map(cld, bmp);
		temp
	}

	callres = cl_premask_area(cld, bmp, spread_timeout); //
	if (callres != EXIT_SUCCESS) {
		distruct_parse_map(cld, bmp);
		temp
	}

	callres = cl_set_gid_row(cld, &gr); //
	if (callres != EXIT_SUCCESS) {
		distruct_parse_map(cld, bmp);
		temp
	}

	callres = cl_normalise_mask_area(cld, bmp); // 
	if (callres != EXIT_SUCCESS) {
		distruct_parse_map(cld, bmp);
		temp
	}

	callres = cl_fix_gid(cld, &gr); //
	if (callres != EXIT_SUCCESS) {
		distruct_parse_map(cld, bmp);
		temp
	}

	callres = cl_finalize_mask(cld, bmp, &gr); //
	if (callres != EXIT_SUCCESS) {
		distruct_parse_map(cld, bmp);
		temp
	}

	clFinish(cld->command_queue);

	if (cld->cl_buffer_gid_row_index)clReleaseMemObject(cld->cl_buffer_gid_row_index);
	if (cld->cl_buffer_gid_row) clReleaseMemObject(cld->cl_buffer_gid_row);

free_temporary_resources:
	if (gr.gid_row) free(gr.gid_row);
	if (gr.gid_row_index) free(gr.gid_row_index);
	return callres;
}


int cl_build_matrix(struct graph_as_row_t* g, struct cl_data_t* cld, 
	struct bmp_map* bmp, unsigned char matrix_link_flag_value) {
	
	cl_int cl_callres = CL_SUCCESS;
	cl_kernel build_matrix;
	
	build_matrix = clCreateKernel(
		cld->program, //program,
		"build_matrix",
		&cl_callres
	);
	check(cl_callres != CL_SUCCESS, "Cannot create fix_gid kernel", cl_callres)
		//printf("Created build_matrix kernel.\n");

	//cl_mem cl_buffer_matrix = clCreateBuffer(
	//	cld->context,
	//	CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
	//	g->matrix_size, //(g->vertex_count + 1) * g->matrix_column_size * sizeof(bitfield_cell),
	//	g->matrix,
	//	&cl_callres
	//);

	cl_mem cl_buffer_matrix = clCreateBuffer(
		cld->context,
		CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
		g->matrix_size, //(g->vertex_count + 1) * g->matrix_column_size * sizeof(bitfield_cell),
		g->matrix,
		&cl_callres
	);
	clFinish(cld->command_queue);

	clSetKernelArg(build_matrix, 0, sizeof(size_t), (void*)&bmp->image_width);
	clSetKernelArg(build_matrix, 1, sizeof(size_t), (void*)&bmp->image_height);
	clSetKernelArg(build_matrix, 2, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);
	clSetKernelArg(build_matrix, 3, sizeof(cl_mem), (void*)&cl_buffer_matrix);
	clSetKernelArg(build_matrix, 4, sizeof(size_t), (void*)&g->matrix_column_size);
	clSetKernelArg(build_matrix, 5, sizeof(unsigned char), (void*)&matrix_link_flag_value);

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		build_matrix,
		1, // dims
		NULL, // offset
		&bmp->mask_size, //g size
		NULL, // l size
		0, // num events
		NULL,
		NULL
	);
	//printf("Kernel execution build_matrix: %d;\n", callres);

	clFinish(cld->command_queue);

	/*cl_callres = clEnqueueReadBuffer(
		cld->command_queue, //command_queue
		cl_buffer_matrix,
		CL_FALSE,
		0,
		(cld->vertex_count + 1) * g->matrix_column_size * sizeof(bitfield_cell),
		g->matrix,
		0,
		NULL,
		NULL
	);*/


	cl_callres = clEnqueueReadBuffer(
		cld->command_queue, //command_queue,
		cld->cl_buffer_mask, //mask,
		CL_FALSE,
		0,
		bmp->mask_size * sizeof(mask_cell),
		cld->mask_row,
		0,
		NULL,
		NULL
	);

	clFinish(cld->command_queue);
	return EXIT_SUCCESS;
}

void distruct_build_graph(struct graph_as_row_t* g, struct cl_data_t* cld, struct bmp_map* bmp) {
	distruct_parse_map(cld, bmp);
	if (g->vertex_row) free(g->vertex_row);
	if (g->order) free(g->order);
	if (g->matrix) free(g->matrix);
}

int build_graph(struct graph_as_row_t* g, 
	struct cl_data_t* cld, struct bmp_map* bmp, 
	unsigned char matrix_link_flag_value
) {
	
	if (graph_init_as_row(g, cld->vertex_count, matrix_link_flag_value) != EXIT_SUCCESS) {
		printf("Cannot init graph");
		distruct_build_graph(g, cld, bmp);
		return EXIT_FAILURE;
	}
	//printf("Inited graph.\n");

	if (cl_build_matrix(g, cld, bmp, matrix_link_flag_value) != EXIT_SUCCESS) {
		printf("Cannot build matrix");
		distruct_build_graph(g, cld, bmp);
		return EXIT_FAILURE;
	}
	//printf("Builded graph matrix.\n");

	graph_calc_links(g, matrix_link_flag_value);

	//graph_display(g, matrix_link_flag_value);

	return EXIT_SUCCESS;
}

int cl_apply_colors(struct cl_data_t* cld, struct bmp_map* bmp, struct graph_as_row_t* g) {
	cl_int cl_callres = CL_SUCCESS;
	cl_kernel apply_colors = NULL;
	uint8_t* vertex_color = NULL;
	cl_mem cl_buffer_vertex_color;

	vertex_color = (uint8_t*)calloc(g->vertex_count + 1, sizeof(uint8_t));
	check(vertex_color == NULL, "Cannot allocate vertex to color buffer", EXIT_FAILURE)


	for (size_t vid = 1; vid < g->vertex_count + 1; vid++) {
		vertex_color[vid] = g->vertex_row[vid].color_id;
	}

	cl_buffer_vertex_color = clCreateBuffer(
		cld->context,
		CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
		g->vertex_count + 1,
		vertex_color,
		&cl_callres
	);
	check(cl_callres != CL_SUCCESS, "Cannot create vertex_color buffer", cl_callres)
	

	
	//printf("Applying colors to image object\n");

	apply_colors = clCreateKernel(cld->program, "apply_colors", &cl_callres);
	check(cl_callres != CL_SUCCESS, "Cannot create apply_colors kernel", cl_callres)

	clSetKernelArg(apply_colors, 0, sizeof(cl_mem), (void*)&cld->cl_image_map);
	clSetKernelArg(apply_colors, 1, sizeof(cl_mem), (void*)&cld->cl_buffer_mask);
	clSetKernelArg(apply_colors, 2, sizeof(cl_mem), (void*)&cl_buffer_vertex_color);

	clFinish(cld->command_queue);

	cl_callres = clEnqueueNDRangeKernel(
		cld->command_queue,//command_queue,
		apply_colors,
		2, NULL, // offset
		(size_t[2]) {bmp->image_width, bmp->image_height}, //g size
		NULL, 0, NULL, NULL
	);
	check(cl_callres != CL_SUCCESS, "Kernel apply_colors execution error", cl_callres)

	clFinish(cld->command_queue);
	return EXIT_SUCCESS;
}


int apply_colors_and_mask(struct cl_data_t* cld, struct bmp_map* bmp, struct graph_as_row_t* g) {
	cl_int cl_callres = CL_SUCCESS;

	if(g == NULL) cl_debug_output(cld, bmp);
	else {
		cl_apply_colors(cld, bmp, g);
	}
	cl_callres = clEnqueueReadImage(
		cld->command_queue, //command_queue,
		cld->cl_image_map, //map,
		CL_FALSE,
		(size_t[3]) { 0, 0, 0 },
		(size_t[3]) { bmp->image_width, bmp->image_height, 1 },
		bmp->image_row_pitch, 0,
		bmp->linear_sequence, 0, NULL, NULL
	);

	clFinish(cld->command_queue);
	return EXIT_SUCCESS;
}