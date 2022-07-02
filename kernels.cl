typedef int mask_cell;
typedef uint4 color_t;
typedef int gid_t;
typedef uint bitfield_cell;

__kernel void mask_border(
	__read_only image2d_t map,
	__global mask_cell* mask
){
	const sampler_t bmpmap_sample = 
	CLK_NORMALIZED_COORDS_FALSE |
	CLK_ADDRESS_CLAMP_TO_EDGE 	|
	CLK_FILTER_NEAREST;
	const mask_cell mask_cell_border = 0x01;
	color_t c_black = (0x00, 0x00, 0x00, 0xFF);

	int2 mapcoord = (int2)(get_global_id(0), get_global_id(1));
	int maskcoord = mapcoord.s1 * get_image_width(map) + mapcoord.s0;
	
	color_t c = read_imageui(map, bmpmap_sample, mapcoord);
	if((c != c_black).s0) mask[maskcoord] = mask_cell_border;
}

__kernel void set_gid_row(
	__global gid_t* row
){
	size_t idx = get_global_id(0);
	row[idx] = idx;
}

int4 get_neighbours(
	__const size_t width,
	__const size_t height,
	__global mask_cell* mask,
	const int idx
){
	const mask_cell mask_cell_border = 0x01;
	int4 res = (int4)(-1, -1, -1, -1); // t0. l1. b2. r3
	int c = idx / width;
	if(c > 0 && mask[idx - width] != mask_cell_border) 
		res.s0 = idx - width;
	if(c < height - 1 && mask[idx + width] != mask_cell_border) 
		res.s2 = idx + width;
	
	c = idx % width;
	
	if(c > 0 && mask[idx - 1] != mask_cell_border) 
		res.s1 = idx - 1;
	if(c < width - 1 && mask[idx + 1] != mask_cell_border) 
		res.s3 = idx + 1;
	return res;
}

bool is_start_point(
	const int4 n, 	// t0. l1. b2. r3
	gid_t idx,
	const size_t spread_timeout
){
	//if(idx % spread_timeout == 0) return true;
	//if(n.s0 == -1 && n.s1 == -1 || n.s2 == -1 && n.s3 == -1) return true;
	return (n.s0 == -1 && n.s1 == -1);
}

size_t allocate_gid_idx(__global size_t* gid){
	return (size_t) atomic_inc(gid);
}

mask_cell get_the_smallest(mask_cell r, __global mask_cell* mask, int nidx){
	mask_cell v = 0;
	if(nidx != -1){
		v = mask[nidx];
		if(v > 1){
			r = v;
		}
	}
	return r;
}

mask_cell wait_for_the_smallest(__global mask_cell* mask, int4 n){
		// t0. l1. b2. r3
	mask_cell r = 0, t = 0;
	r = get_the_smallest(r, mask, n.s0);
	t = get_the_smallest(r, mask, n.s1);
	if(r < t) r = t;
	t = get_the_smallest(r, mask, n.s2);
	if(r < t) r = t;
	r = get_the_smallest(r, mask, n.s3);
	if(r < t) r = t;
	return r;
}

__kernel void premask_area(
	__const size_t width,
	__const size_t height,
	__global mask_cell* mask,
	__global size_t* gid_idx,
	__const size_t spread_timeout
){
	const mask_cell mask_cell_border = 0x01;
	size_t idx = get_global_id(0);
	
	if(mask[idx] == mask_cell_border) return;
	
	int4 n = get_neighbours(width, height, mask, idx); // t0. l1. b2. r3

	bool start_point = is_start_point(n, idx, spread_timeout);

	if(start_point)
		mask[idx] = allocate_gid_idx(gid_idx);
 
	barrier(CLK_GLOBAL_MEM_FENCE);

 	mask_cell v = 0;
	if(!start_point){
		int i = 0;
		while(v < 1 && i < spread_timeout){ 
			v = wait_for_the_smallest(mask, n);
			if(v > 1) {
				mask[idx] = v;
			}
			i++;
			barrier(CLK_GLOBAL_MEM_FENCE);
		}
	}
}

gid_t get_parent_gid(
	__global gid_t* row,
	gid_t id
){
	//int i = 0;
	while(id != row[id]) {
		id = row[id];
		//i++;
	}
	return id;
}





void sema_down(__global int* semaphor){
  int occupied = atom_xchg(semaphor, 1);
  uint timeout = 0;
  while(occupied > 0 && timeout < 50000)
  {
   occupied = atom_xchg(semaphor, 1);
   timeout++;
  }
}

void sema_up(__global int* semaphor){
  int old_state = atom_xchg(semaphor, 0);
}

void normalize_neighbours(
	__global gid_t* row,
	mask_cell cv,
	mask_cell pv
){
	

	barrier(CLK_GLOBAL_MEM_FENCE);
	gid_t old_id = get_parent_gid(row, cv), 
		new_id = get_parent_gid(row, pv), t = 0;
	if(new_id != old_id){	
		if(new_id > old_id){
			t = new_id; 
			new_id = old_id;
			old_id = t;
		}
		barrier(CLK_GLOBAL_MEM_FENCE);
		row[old_id] = new_id;
	}

}

__kernel void normalise_mask_area(
	__global mask_cell* mask,
	__global gid_t* row,
	__const size_t width,
	__const size_t height
){
	const mask_cell mask_cell_border = 0x01;
	size_t idx = get_global_id(0);
	
	//		vertical
	size_t edge = height;
	size_t d = width;
	size_t pos = idx + d; // to skip first

	//		horisontal	
	if(idx >= width){
		edge = width;
		d = 1;
		pos = (idx - width) * width + d; // to skip first
	}
	mask_cell cv = 0, pv = 0;

	for(size_t i = 0; i < edge - 1; i++, pos += d){
		cv = mask[pos], pv = mask[pos - d];
		if(cv == pv)
			continue;
		if(cv == mask_cell_border || pv == mask_cell_border)
			continue;
		if(cv == 0 || pv == 0)
			continue;

		//printf("\t (%2d | %4d) %d -> %d;\n", idx, pos, cv, pv);
		normalize_neighbours(row, cv, pv);
	
	}
	
}

__kernel void apply_parent_gid( // todo: ???
	__global mask_cell* mask,
	__global gid_t* row
){
	size_t idx = get_global_id(0);
	mask[idx] = get_parent_gid(row, mask[idx]);
}


__kernel void normalise_gid(
	__global gid_t* row
){
	size_t idx = get_global_id(0);
	row[idx] = get_parent_gid(row, idx);
}

__kernel void fix_gid(
	__global gid_t* row,
	__global size_t* gid_idx,
	__const size_t reserved_gids
){
	size_t idx = get_global_id(0);
	if(idx < reserved_gids) return;
	if(row[idx] == idx) row[idx] = atomic_inc(gid_idx);
}

__kernel void finalize_mask(
	__global mask_cell* mask,
	__global gid_t* row
){
	const mask_cell mask_cell_border = 0x01;
	size_t idx = get_global_id(0);
	mask_cell cv = mask[idx];
	if(cv == mask_cell_border){
		mask[idx] = 0;
	}
	else{
		mask[idx] = row[cv];
	}

}



mask_cell reach_area(
	__global mask_cell* mask,
	int pos,
	__const int d,
	__const int limit
){
	mask_cell res = 0;
	int idx = pos;
	int i = 0;
	for(i; i < limit && mask[pos] == 0; i++){
		pos += d;
		res = mask[pos];
	}
	return res;
}

#define bc_bits (sizeof(bitfield_cell) * 8)

size_t get_matrix_idx (
	__const size_t matrix_column_size,
	__const gid_t lv,
	__const gid_t rv
){
	return (lv * matrix_column_size) + rv / bc_bits;
}

bitfield_cell get_matrix_cell_mask(
	__const gid_t v,
	__const uchar matrix_link_flag_value
){
	bitfield_cell mask = 1 << (v % bc_bits);
	if(matrix_link_flag_value){
		return mask;
	}
	return ~mask;
}


void set_link(
	__global bitfield_cell* matrix,
	__const size_t matrix_column_size,
	gid_t lv,
	gid_t rv,
	__const uchar matrix_link_flag_value
){
	// add rv to lv
	size_t idx = get_matrix_idx(matrix_column_size, lv, rv);
	bitfield_cell mask = get_matrix_cell_mask(rv, matrix_link_flag_value);
	if(matrix_link_flag_value) atomic_or(matrix + idx, mask);
	else atomic_and(matrix + idx, mask);

	// add lv to rv
	idx = get_matrix_idx(matrix_column_size, rv, lv);
	mask = get_matrix_cell_mask(lv, matrix_link_flag_value);
	if(matrix_link_flag_value) atomic_or(matrix + idx, mask);
	else atomic_and(matrix + idx, mask);
}


__kernel void build_matrix(
	__const size_t width,
	__const size_t height,
	__global mask_cell* mask,
	__global bitfield_cell* matrix,
	__const size_t matrix_column_size,
	__const uchar matrix_link_flag_value
){ 
	
	size_t idx = get_global_id(0);
	size_t px = idx % width,
			py = idx / width;
	if(mask[idx] != 0 || 
		px == 0 || px == width - 1 ||
		py == 0 || py == height - 1) return;

	gid_t v = 0, nv = 0;

	int mask_size = width * height;

	// vert backward (down)
	v = reach_area(mask, idx, -width, py); 
	if(v != 0){
		// vert forward (up)
		nv = reach_area(mask, idx, width, height - py - 1);
		if(nv != 0 && v != nv){
			set_link(matrix, matrix_column_size, v, nv, 
				matrix_link_flag_value);
		}
	}

	// hori backward (left)
	v = reach_area(mask, idx, -1, px);
	if(v != 0){
		nv = reach_area(mask, idx, 1, width - px - 1);
		if(nv != 0 && v != nv){
			set_link(matrix, matrix_column_size, v, nv,
				matrix_link_flag_value);
		}
	}
}

__kernel void debug_output(
	__write_only image2d_t map,
	__global mask_cell* mask
){
	const sampler_t bmpmap_sample = 
	CLK_NORMALIZED_COORDS_FALSE |
	CLK_ADDRESS_CLAMP_TO_EDGE 	|
	CLK_FILTER_NEAREST;
	const mask_cell mask_cell_border = 0x01;
	color_t c_black = (uint4)(0xAA, 0x00, 0x00, 0xFF);

	int2 mapcoord = (int2)(get_global_id(0), get_global_id(1));
	int maskcoord = mapcoord.s1 * get_image_width(map) + mapcoord.s0;

	uint v = mask[maskcoord];
	uchar c = 20 + v % 0xF0;
	
	

	if(v == 0) 
		write_imageui(map, mapcoord, (uint4)(0xEE, 0x10, 0x88, 0xFF));
	else write_imageui(map, mapcoord, (uint4)(c, c, c, 0xFF));
}

__kernel void apply_colors(
	__write_only image2d_t map,
	__global mask_cell* mask,
	__global uchar* color
){
	int2 mapcoord = (int2)(get_global_id(0), get_global_id(1));
	int maskcoord = mapcoord.s1 * get_image_width(map) + mapcoord.s0;

	uchar color_id = color[mask[maskcoord]];
	uchar defvalue = 0xEE, secvalue = 0x55;
	if(color_id == 1) 
		write_imageui(map, mapcoord, (uint4)(defvalue, secvalue, secvalue, 0xFF));
	else if(color_id == 2) 
		write_imageui(map, mapcoord, (uint4)(secvalue, defvalue, secvalue, 0xFF));
	else if(color_id == 4) 
		write_imageui(map, mapcoord, (uint4)(secvalue, secvalue, defvalue, 0xFF));
	else if(color_id == 8) 
		write_imageui(map, mapcoord, (uint4)(secvalue, defvalue, defvalue, 0xFF));
	else if(color_id == 16) 
		write_imageui(map, mapcoord, (uint4)(defvalue, defvalue, secvalue, 0xFF));
	else if(color_id == 32) 
		write_imageui(map, mapcoord, (uint4)(defvalue, secvalue, defvalue, 0xFF));
	else
		write_imageui(map, mapcoord, (uint4)(0x00, 0x00, 0x00, 0xFF));
}
