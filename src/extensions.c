#include "extensions.h"
#include "options.h"
#include "engine.h"

//////////////////////////////////////////////////////////////////////
// For sorting colors

int color_features_compare(const void* vptr_a, const void* vptr_b) {

	const color_features_t* a = (const color_features_t*)vptr_a;
	const color_features_t* b = (const color_features_t*)vptr_b;

	int u = cmp(a->user_index, b->user_index);
	if (u) { return u; }

	int w = cmp(a->wall_dist[0], b->wall_dist[0]);
	if (w) { return w; }

	int g = -cmp(a->wall_dist[1], b->wall_dist[1]);
	if (g) { return g; }

	return -cmp(a->min_dist, b->min_dist);

}

//////////////////////////////////////////////////////////////////////
// Place the game colors into a set order

void game_order_colors(game_info_t* info,
                       game_state_t* state) {

	if (g_options.order_random) {
    
		srand(now() * 1e6);
    
		for (size_t i=info->num_colors-1; i>0; --i) {
			size_t j = rand() % (i+1);
			int tmp = info->color_order[i];
			info->color_order[i] = info->color_order[j];
			info->color_order[j] = tmp;
		}

	} else { // not random

		color_features_t cf[MAX_COLORS];
		memset(cf, 0, sizeof(cf));

		for (size_t color=0; color<info->num_colors; ++color) {
			cf[color].index = color;
			cf[color].user_index = MAX_COLORS;
		}
    

		for (size_t color=0; color<info->num_colors; ++color) {
			
			int x[2], y[2];
			
			for (int i=0; i<2; ++i) {
				pos_get_coords(state->pos[color], x+i, y+i);
				cf[color].wall_dist[i] = get_wall_dist(info, x[i], y[i]);
			}

			int dx = abs(x[1]-x[0]);
			int dy = abs(y[1]-y[0]);
			
			cf[color].min_dist = dx + dy;
			
		

		}


		qsort(cf, info->num_colors, sizeof(color_features_t),
		      color_features_compare);

		for (size_t i=0; i<info->num_colors; ++i) {
			info->color_order[i] = cf[i].index;
		}
    
	}

	if (!g_options.display_quiet) {

		printf("\n************************************************"
		       "\n*               Branching Order                *\n");
		if (g_options.order_most_constrained) {
			printf("* Will choose color by most constrained\n");
		} else {
			printf("* Will choose colors in order: ");
			for (size_t i=0; i<info->num_colors; ++i) {
				int color = info->color_order[i];
				printf("%s", color_name_str(info, color));
			}
			printf("\n");
		}
		printf ("*************************************************\n\n");

	}

}



//////////////////////////////////////////////////////////////////////
// Check for dead-end regions of freespace where there is no way to
// put an active path into and out of it. Any freespace node which
// has only one free neighbor represents such a dead end. For the
// purposes of this check, cur and goal positions count as "free".

int game_check_deadends(const game_info_t* info,
                        const game_state_t* state) {

	/**
	 * FILL CODE TO DETECT DEAD-ENDS
	 */
	pos_t curr_pos;
	cell_t curr_cell;
	int curr_cell_type, curr_cell_color;

	for (int x = 0; x < info->size; x++) {
		for (int y = 0; y < info->size; y++) {
			if (game_num_free_coords(info, state, x, y) > 1) {
				// more than 1 free neighbor -> definitely non-dead-end
				continue;
			}

			curr_pos = pos_from_coords(x, y);
			curr_cell = state->cells[curr_pos];
			curr_cell_type = cell_get_type(curr_cell);
			curr_cell_color = cell_get_color(curr_cell);

			if (curr_cell_type == TYPE_PATH && curr_pos != state->pos[curr_cell_color]) {
				// path body counts as non-dead-end
				continue;
			}
			if (is_deadend_cell(info, state, curr_pos))
				return 1;
		}
	}
	
	return 0;

}



//////////////////////////////////////////////////////////////////////
// Check whether the cell is a dead-end.

int is_deadend_cell(const game_info_t* info, const game_state_t* state, pos_t curr_pos) {
	pos_t neighbor_pos;
	cell_t neighbor_cell, curr_cell;
	int neighbor_cell_type, curr_cell_type;
	int neighbor_cell_color, curr_cell_color;

	curr_cell = state->cells[curr_pos];
	curr_cell_type = cell_get_type(curr_cell);
	curr_cell_color = cell_get_color(curr_cell);

	int num_free_neighbors = 0;   // number of actual TYPE_FREE neighbors
	int can_enter_from = 0;       // valid entrance for a free cell
	int can_go_to = 0;            // valid exit for a free cell
	int init_can_go_to = 0;       // valid exit for an initial cell
	int head_can_go_to = 0;       // valid exit for a path head
	int can_enter_goal_from = 0;  // valid entrance for a goal cell
	
	for (int dir = DIR_LEFT; dir <= DIR_DOWN; dir++) {
		neighbor_pos = pos_offset_pos(info, curr_pos, dir);
		neighbor_cell = state->cells[neighbor_pos];
		neighbor_cell_type = cell_get_type(neighbor_cell);
		neighbor_cell_color = cell_get_color(neighbor_cell);

		if (!pos_valid(info, neighbor_pos))
			continue;
		
		// Find entrance and exit according to the neighbors' cell type
		switch (neighbor_cell_type) {
			case TYPE_FREE:
				num_free_neighbors++;
				break;

			case TYPE_GOAL:
				if (!color_completed(state, neighbor_cell_color)) {
					// can exit a free cell from an unreached goal cell
					can_go_to = 1;
				} else if (neighbor_cell_color == curr_cell_color) {
					// can exit a head cell from the same-colored goal cell
					head_can_go_to = 1;
				}
				break;

			case TYPE_PATH:
				if (neighbor_pos == state->pos[neighbor_cell_color]) {
					// can enter a free cell from a path head
					can_enter_from = 1;
					if (neighbor_cell_color == curr_cell_color) {
						// can enter a goal cell from a same-colored path head
						can_enter_goal_from = 1;
					}
				}
				if (neighbor_cell_color == curr_cell_color) {
					// can exit an initial cell from a same-colored path cell
					init_can_go_to = 1;
				}
				break;

			case TYPE_INIT:
				if (neighbor_pos == state->pos[neighbor_cell_color]) {
					// can enter a free cell from an unstarted initial cell
					can_enter_from = 1;
				}
				break;
		}
	}
		
	// check conditions for each cell type
	switch (curr_cell_type) {
		case TYPE_FREE:
			if (can_enter_from + can_go_to + num_free_neighbors >= 2) {
				// num_free_neighbors is a wildcard for entrance and exit
				return 0;
			}
			break;

		case TYPE_GOAL:
			if (can_enter_goal_from || num_free_neighbors) {
				return 0;
			} 
			break;

		case TYPE_INIT:
			if (init_can_go_to || num_free_neighbors) {
				return 0;
			} 
			break;

		case TYPE_PATH:
			if (head_can_go_to || num_free_neighbors) {
				return 0;
			}
			break;
	}

	return 1;
}


