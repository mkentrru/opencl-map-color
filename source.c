
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "ocl_map_to_graph.h"


#define FATAL(CORE){printf("\nFATAL: %s failed. exiting.\n", CORE); return EXIT_FAILURE;}

#define MSG(S) printf("\n\t> %s\n", S);

int parse_arguments(int argc, char** argv, const char** input, const char** output) {
	if (argc != 3) {
		printf("Wrong arguments.\n");
		return EXIT_FAILURE;
	}

	*input = argv[1];
	*output = argv[2];

	printf("\n\t< input:  %s;"
		"\n\t< output: %s;\n", *input, *output);

	return EXIT_SUCCESS;
}


int main(int argc, char** argv) {

	const char* input_filename = NULL;
	const char* output_filename = NULL;

	struct bmp_map bmp;
	struct cl_data_t cld;
	struct graph_as_row_t g;

	clock_t TIME_ALL, TIME_PARSING, TIME_COLORING;

	MSG("Welcome to map colorer")

	MSG("Parsing arguments...")
	if (parse_arguments(argc, argv, &input_filename, &output_filename) != EXIT_SUCCESS)
		FATAL("parse_input")

	TIME_ALL = clock(); // 
	
	MSG("Reading bmp source file data...")
	if (bmp_map_setup(&bmp, input_filename, output_filename) != EXIT_SUCCESS) // 
		FATAL("bmp_map_setup")

	
	MSG("Setting up environment and shared buffers...")
	if (setup_environment("kernels.cl", &cld, &bmp) != EXIT_SUCCESS) // 
		FATAL("setup_environment")

	TIME_PARSING = clock(); // 

	MSG("Parsing bmp file to areas...")
	if (parse_map(&cld, &bmp) != EXIT_SUCCESS) //
		FATAL("parse_map")
	
	
	MSG("Building graph according to areas...")
	if (build_graph(&g, &cld, &bmp, 1) != EXIT_SUCCESS)
		FATAL("build_graph")
	
	TIME_PARSING = clock() - TIME_PARSING; //
	TIME_COLORING = clock(); //

	MSG("Coloring the graph...")
	if (graph_coloring(&g) != EXIT_SUCCESS)
		FATAL("graph_coloring")

	TIME_COLORING = clock() - TIME_COLORING;
	TIME_ALL = clock() - TIME_ALL;
	
	MSG("Applying colors to mask...")
	//if (apply_colors_and_mask(&cld, &bmp, NULL) != EXIT_SUCCESS)
	if (apply_colors_and_mask(&cld, &bmp, &g) != EXIT_SUCCESS)
		FATAL("apply_colors_and_mask")
	
	
	MSG("Putting result to bmp file...")
	if (bmp_map_put_result(&bmp) != EXIT_SUCCESS)
		FATAL("bmp_map_put_result")

	printf("\n\t< Time: all: %fs; parsing: %fs; coloring: %fs;\n",
		(float)TIME_ALL / CLOCKS_PER_SEC, (float)TIME_PARSING / CLOCKS_PER_SEC,
		(float)TIME_COLORING / CLOCKS_PER_SEC);

	MSG("That's all! Thanks!")
	

	return EXIT_SUCCESS;
}

