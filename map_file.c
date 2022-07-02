#include "map_file.h"

void bmp_map_init(struct bmp_map* f) {
	memset(f, 0, sizeof(struct bmp_map));
}

int read_bytes(FILE* f, void* dst, size_t size, long offset) {
	fseek(f, offset, SEEK_SET);
	return fread(dst, sizeof(char), size, f);
}

int bmp_map_read_data(struct bmp_map* f, const char* name) {
	f->file = fopen(name, "r");
	check(f->file == NULL, "Cannot open source bmp file", MF_SOURCE_OPEN)

		MF_WORD type = 0;
	read_bytes(f->file, &type, sizeof(MF_WORD), MF_POS_Type);
	//if (type != 0x4d42) return MF_SOURCE_TYPE; // 'MB' signature
	check(type != 0x4d42, "Source bmp file has wrong signature", MF_SOURCE_TYPE)

		MF_DWORD data_offset = 0;
	read_bytes(f->file, &data_offset, sizeof(MF_DWORD), MF_POS_OffBits);
	//if (data_offset == 0) return MF_SOURCE_OFFS;
	check(data_offset == 0, "Wrong bmp file data offset", MF_SOURCE_OFFS)

		f->data_offset = data_offset;

	MF_DWORD size = 0;
	read_bytes(f->file, &size, sizeof(MF_DWORD), MF_POS_Size);
	if (size != 0) {
		f->linear_sequence_size = (size_t)(size - data_offset);
	}

	MF_LONG width = 0;
	read_bytes(f->file, &width, sizeof(MF_LONG), MF_POS_Width);

	MF_LONG height = 0;
	read_bytes(f->file, &height, sizeof(MF_LONG), MF_POS_Height);

	f->image_width = (size_t)width;
	f->image_height = (size_t)height;

	MF_WORD bitsperpix = 0;
	read_bytes(f->file, &bitsperpix, sizeof(MF_WORD), MF_POS_BitsPerPixel);
	//if (bitsperpix != 32) return MF_SOURCE_BPPI;
	check(bitsperpix != 32, "32-bit bmp files only", MF_SOURCE_BPPI)

		size_t row_pitch = width;
	if (row_pitch % 4) row_pitch = (row_pitch / 4 + 1) * 4; // aligning

	check(f->linear_sequence_size == 0, "Wrong data size in bmp file", 0)

		f->linear_sequence = (char*)calloc(f->linear_sequence_size, sizeof(char));
	check(f->linear_sequence == NULL, "Cannot allocate memory for image data", 0)

		f->mask_size = f->image_height * f->image_width;

	if (f->linear_sequence_size != read_bytes(
		f->file,
		f->linear_sequence,
		f->linear_sequence_size,
		f->data_offset)
		)
		return MF_SOURCE_LSRE;

	return EXIT_SUCCESS;
}

void distruct_bmp_map(struct bmp_map* f) {
	if (f->file) fclose(f->file);
	if (f->output) fclose(f->output);
	if (f->linear_sequence) free(f->linear_sequence);
}

int open_bmp_output(struct bmp_map* f, const char* out) {

	f->output = fopen(out, "w+");
	if (f->output == NULL) {
		printf("Cannot open output file.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int bmp_map_setup(struct bmp_map* f, const char* name, const char* out) {
	bmp_map_init(f);
	
	if (bmp_map_read_data(f, name) != EXIT_SUCCESS) {
		distruct_bmp_map(f);
		return EXIT_FAILURE;
	}

	if(open_bmp_output(f, out) != EXIT_SUCCESS){
		distruct_bmp_map(f);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int bmp_map_put_result(struct bmp_map* bmp) {
	char* head_buffer = (char*)calloc(bmp->data_offset, sizeof(char));
	if (head_buffer) {
		rewind(bmp->file);
		fread(head_buffer, sizeof(char), bmp->data_offset, bmp->file);
		fwrite(head_buffer, sizeof(char), bmp->data_offset, bmp->output);
		free(head_buffer);
		fwrite(bmp->linear_sequence, sizeof(char), bmp->linear_sequence_size, bmp->output);
	}
	return EXIT_SUCCESS;
}
