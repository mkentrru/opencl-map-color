
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "macros.h"

#define MF_SUCCESS		EXIT_SUCCESS
#define MF_SOURCE_OPEN	((int) 0x01)
#define MF_SOURCE_TYPE	((int) 0x02)
#define MF_SOURCE_OFFS	((int) 0x03)
#define MF_SOURCE_BPPI	((int) 0x04)
#define MF_SOURCE_LSRE	((int) 0x05)

typedef uint16_t	MF_WORD;
typedef uint32_t	MF_DWORD;
typedef uint32_t	MF_LONG;
typedef uint32_t	mask_cell;

#define MF_POS_Type 0x00
#define MF_POS_Size 0x02
#define MF_POS_OffBits 0x0A
#define MF_POS_Width 0x12
#define MF_POS_Height 0x16
#define MF_POS_BitsPerPixel 0x1C


struct bmp_map {
	FILE* file;
	FILE* output;

	char* row;

	char* linear_sequence;
	size_t linear_sequence_size;

	size_t mask_size;

	size_t image_height;
	size_t image_width;
	size_t image_row_pitch;
	size_t data_offset;
	size_t row_offset;
};

void bmp_map_init(struct bmp_map*);
int bmp_map_setup(struct bmp_map*, const char*, const char*); // check callocs
int bmp_map_put_result(struct bmp_map*);

void distruct_bmp_map(struct bmp_map*);