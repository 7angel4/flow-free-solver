#include "search.h"
#include "options.h"
#include "queues.h"
#include "extensions.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////
// Initialize Maximum number of nodes allowed, given a MB bound


void initialize_search( size_t* max_nodes,
			const game_info_t* info,
			const game_state_t* init_state ){

	*max_nodes = g_options.search_max_nodes;
	if (! (*max_nodes) ) {
		*max_nodes = floor( g_options.search_max_mb * MEGABYTE /
				   sizeof(tree_node_t) );
	}

	if (!g_options.display_quiet) {
		
		printf("\n************************************************"
		       "\n*               Initializing Search            *\n");

		
		printf("* Will search up to %'zu nodes (%'.2f MB) \n",
		       *max_nodes, *max_nodes*(double)sizeof(tree_node_t)/MEGABYTE);
  
		printf("* Num Free cells at start is %'d\n\n",
		       init_state->num_free);

		printf("* Initial State:\n");
		game_print(info, init_state);

		printf ("*************************************************\n\n");

	}

}

//////////////////////////////////////////////////////////////////////
// Check if node contains a state with:
//    a) no free cell
//    b) all colors connected by a path

int is_solved(tree_node_t* node, const game_info_t* info){
	if ( node->state.num_free == 0 
			&& node->state.completed == (1 << info->num_colors) - 1 ) {

		return 1;
					
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////
// Animate sequence of moves up to node

void report_solution( const tree_node_t* node, const game_info_t* info ){

		assert(node);

		printf("Number of moves=%'g, Free cells=%'d\n",
		       node->cost_to_node,
		       node->state.num_free);
		
		printf("\n");
		game_print(info, &node->state);
		
		animate_solution(info, node);
		delay_seconds(1.0);
}

//////////////////////////////////////////////////////////////////////
// Peforms Dijkstra  search

int game_dijkstra_search(const game_info_t* info,
                const game_state_t* init_state,
                double* elapsed_out,
                size_t* nodes_out,
                game_state_t* final_state) {


	// Max_nodes that fit in memory
	size_t max_nodes;

	
	initialize_search( &max_nodes, info, init_state );

	// Create Root node
	tree_node_t* root = node_create(NULL, info, init_state);

	// Create Priority Queue
	heapq_t pq = heapq_create(max_nodes);

	int result = SEARCH_IN_PROGRESS;
	const tree_node_t* solution_node = NULL;

	// Record the timestamp search starts
	double start = now();

	/**
	 * FILL IN THE CODE BELOW TO PERFORM DIJKSTRA OVER THE POSSIBLE 
	 * MOVES TO SOLVE FLOW GAME
	 */
	
	int nextColour;
	tree_node_t *node, *child;
	int dir;

	// flat queue to keep track of the nodes created
	queue_t q = queue_create(max_nodes);
	queue_enqueue(&q, root);

	/* Optimization: check initial cells that can be 
	   directly linked to their goal cells */
	if (g_options.with_optimization) {
		root = link_straight_flows(info, init_state, root, &q);
	
		// already found solution?
		if ( is_solved(root, info) ) {          
			result = SEARCH_SUCCESS;
			solution_node = root;
			*final_state = solution_node->state;
		}
	}

	heapq_enqueue(&pq, root);

	// While no solution found
	while (!solution_node) {

		if (heapq_empty(&pq)) {
			// PQ is empty but no solution -> unsolvable puzzle
			result = SEARCH_UNREACHABLE;
			break;
		}

		// Remove node from Queue, in order to generate its successors
		node = heapq_deque(&pq);

		// Get next color to explore its 4 directions
		// (using game_next_move_color function in engine.h)
		nextColour = game_next_move_color(info, &(node->state));

		for (dir = DIR_LEFT; dir <= DIR_DOWN; dir++) {
			// Check move in that direction is possible 
			// Within the rules of the game (see engine.h)
			if (game_can_move(info, &(node->state), nextColour, dir)) {
				// Create child node
				child = node_create(node, info, &(node->state));
				// If no more space in memory, end search (more nodes in pq than max_nodes)
				if (heapq_count(&pq) > max_nodes) {
					result = SEARCH_FULL;
					break;
				}

				// Update child state given the direction
				game_make_move(info, &(child->state), nextColour, dir);
				// Remove node if new position creates a deadend
				if (g_options.node_check_deadends 
						&& game_check_deadends(info, &child->state)) {
					free(child);
					continue;	
				}
				
				// Check if game is solved
				if ( is_solved(child, info) ) {          
					result = SEARCH_SUCCESS;
					solution_node = child;
					// enqueue the solution node to free nodes altogether later
					queue_enqueue(&q, child);
					*final_state = solution_node->state;
					break;     
				}

				// Add child to the queues
				heapq_enqueue(&pq, child);
				queue_enqueue(&q, child);
			}
		}
	}


	/**
	 * END OF FILL IN CODE SECTION
	 */
				
	// Get Stats
	double elapsed = now() - start;
	if (elapsed_out) { *elapsed_out = elapsed; }
	if (nodes_out)   { *nodes_out = heapq_count(&pq); }

	// Report solution
	if ( result == SEARCH_SUCCESS
	    && g_options.display_animate
	    && !g_options.display_quiet )
		report_solution( solution_node, info );

	// Report next node in Queue
	if (result == SEARCH_FULL && g_options.display_diagnostics) {
		
		printf("here's the lowest cost thing on the queue:\n");		
		node_diagnostics(info, heapq_peek(&pq));				
	}

	// CODE BELOW IS FILLED IN
	// free the nodes
	for (int i = 0; i < queue_count(&q); i++) {
		free(q.start[i]);
		q.start[i] = NULL;
	}
	queue_destroy(&q);
	heapq_destroy(&pq);

	return result;

  
}

//////////////////////////////////////////////////////////////////////
// Returns a tree node whose state is updated from the root state, by 
// linking the linkable straight flows

tree_node_t *link_straight_flows(const game_info_t* info,
                				 const game_state_t* init_state, 
								 tree_node_t *root, queue_t *q) {

	int x, y, goal_x, goal_y;
	int num_colors = info->num_colors;
	
	// record the directions to move for each color
	int *directions = malloc(num_colors * sizeof(int));
	assert(directions);
	for (int i = 0; i < num_colors; i++) {
		directions[i] = -1;
	}

	// flag to check if there are any straight flows at all
	int no_straight_flows = 1;
	int dir;

	for (int i = 0; i < num_colors; i++) {
		pos_get_coords(info->init_pos[i], &x, &y);
		pos_get_coords(info->goal_pos[i], &goal_x, &goal_y);
		if (x == goal_x || y == goal_y) {
			// possible straight flows detected
			if (x == goal_x && y != goal_y) {
				if (y < goal_y)  // initial cell above the goal cell
					dir = DIR_DOWN;
				else
					dir = DIR_UP;
			} else if (y == goal_y && x != goal_x) {
				if (x < goal_x)
					dir = DIR_RIGHT;
				else
					dir = DIR_LEFT;
			}
			
			if (!path_blocked(info, &(root->state), i, dir)) {
				// this straight flow is linkable
				directions[i] = dir;
				no_straight_flows = 0;
			}
		}
	}

	if (no_straight_flows) {
		// no linkable straight flows, don't waste more time!
		free(directions);
		return root;
	}

	// have linkable straight flows, keep going
	tree_node_t *curr_node = root;
	tree_node_t *child;
	int color;

	for (int i = 0; i < num_colors; i++) {
		if (directions[i] == -1)
			continue;
		
		color = i;  // for clarity
		dir = directions[i];

		// while we have not reached the goal cell 
		// and can still move in that direction
		while (!color_completed(&(curr_node->state), color) 
				&& game_can_move(info, &(curr_node->state), color, dir)) {
			// keep updating the state to move towards the goal
			child = node_create(curr_node, info, &(curr_node->state));
			game_make_move(info, &(child->state), color, dir);
			queue_enqueue(q, child);
			curr_node = child;
		}
	}

	free(directions);
	return curr_node;
}


//////////////////////////////////////////////////////////////////////
// Returns true (1) if the path from the initial cell 
// to the goal cell of that color is blocked,
// false (0) otherwise

int path_blocked(const game_info_t* info,
                 const game_state_t* state, 
				 int color, int dir) {

	pos_t curr_pos, goal_pos;

	curr_pos = pos_offset_pos(info, info->init_pos[color], dir);
	goal_pos = info->goal_pos[color];

	// while there's nothing blocking the path to the goal cell
	while (game_pos_is_free(info, state, curr_pos) && curr_pos != goal_pos) {
		// move forward
		curr_pos = pos_offset_pos(info, curr_pos, dir);
	}

	if (curr_pos != goal_pos) {
		// path blocked
		return 1;
	}

	return 0;
}
