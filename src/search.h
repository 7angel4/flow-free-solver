#ifndef __SEARCH__
#define __SEARCH__


#include "node.h"
#include "engine.h"
#include "queues.h"

//////////////////////////////////////////////////////////////////////
// Peforms Dijkstra  search

int game_dijkstra_search(const game_info_t* info, 
						 const game_state_t* init_state, 
						 double* elapsed_out, size_t* nodes_out, 
						 game_state_t* final_state);

//////////////////////////////////////////////////////////////////////
// Returns a tree node whose state is updated from the root state, by 
// linking the linkable straight flows

tree_node_t *link_straight_flows(const game_info_t* info,
                				 const game_state_t* init_state, 
								 tree_node_t *root, queue_t *q);


//////////////////////////////////////////////////////////////////////
// Returns true (1) if the path from the initial cell 
// to the goal cell of that color is blocked,
// false (0) otherwise

int path_blocked(const game_info_t* info,
                 const game_state_t* state, 
				 int color, int dir);


#endif
