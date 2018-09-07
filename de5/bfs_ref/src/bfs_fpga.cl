#include "knobs.h"

typedef struct
{
	int starting;
	int no_of_edges;
} Node;


//--------------------------------------------------------
/**
 * Run one iteration of the BFS algorithm. Find all the nodes at the next level in the graph.
 * @param[in]  g_graph_nodes          The list of nodes in the graph.
 * @param[in]  g_graph_edges          The list of edges in the graph.
 * @param[in]  g_graph_frontier_mask           A binary mask to indicate which nodes must be processed at this iteration.
 * @param[out] g_graph_updating_mask  A binary mask to indicate which nodes to process the next iteration.
 * @param[in]  g_graph_visited_mask        A binary mask to indicate which nodes have already been visited in the past.
 * @param[in,out] g_cost              Encode the level of each node. Output of the computation.
 * @param[in]  no_of_nodes            Number of nodes in the graph.
 **/
//--------------------------------------------------------
#ifdef ALTERA_CL
__attribute__ ((reqd_work_group_size(KNOB_NUM_WORK_ITEMS_1,1,1)))
__attribute__ ((num_simd_work_items(KNOB_SIMD_1)))
__attribute__ ((num_compute_units(KNOB_COMPUTE_UNITS_1)))
#endif
__kernel void kernel1(__global const Node* restrict  g_graph_nodes,
		__global int*    restrict  g_graph_edges,
		__global mask_t* restrict  g_graph_frontier_mask,
		__global mask_t* restrict  g_graph_updating_mask,
		__global mask_t* restrict  g_graph_visited_mask,
		__global int*    restrict  g_cost,
		unsigned int no_of_nodes) 
//--------------------------------------------------------
{
	
	__private size_t tid = get_global_id(0);

	// Process node only if binary mask is true.
	if(tid < no_of_nodes && g_graph_frontier_mask[tid] != 0)
	{
		// Reset binary mask
		g_graph_frontier_mask[tid] = 0;

		// Find indices of edges for the current node
		__private const unsigned int start = g_graph_nodes[tid].starting;
		__private const unsigned int end   = g_graph_nodes[tid].no_of_edges + g_graph_nodes[tid].starting;

		// Storage for intermediate variables
		__private unsigned int id  [KNOB_UNROLL_FACTOR];
		__private mask_t       Mask[KNOB_UNROLL_FACTOR];
		
		__private int       id_;
		__private mask_t    Mask_;
		
		__private const int cost = g_cost[tid]+1;
		
        // For all edges of the current node
		for(int i = start; i < end; i = i+KNOB_UNROLL_FACTOR)
		{
			// General case
			#if KNOB_UNROLL_FACTOR > 1
			if(i + KNOB_UNROLL_FACTOR - 1 < end)
			#endif
			{
				// For each edge (unrolled)
				#pragma unroll KNOB_UNROLL_FACTOR
				for(int ii = 0; ii < KNOB_UNROLL_FACTOR; ii++) 
				{
					// Store the ID of the edge
					id[ii]         = g_graph_edges[i+ii];

					// Version 1 - use bitwise operators to perform the same operation as Version 2
					#if KNOB_BRANCH == 0
               		Mask[ii]       = -( !g_graph_visited_mask[id[ii]] ); // (!0)=1  -0=0 -1=all ones
               		g_cost[id[ii]] =  ( cost & Mask[ii] ) | ( g_cost[id[ii]] & ~Mask[ii] );

					g_graph_updating_mask[id[ii]] = 1 & !g_graph_visited_mask[id[ii]];
				
					// Version 2
					#else
					// If the graph has not been visited
					if(!g_graph_visited_mask[id[ii]])
					{
						// Update the cost to current level + 1
						// Note that multiple threads can update the same data, but it doesn't matter since the value is the same
						g_cost[id[ii]] = g_cost[tid] + 1;

						// Mark this node to be processed in the next iteration
						// (note: same as above)
						g_graph_updating_mask[id[ii]] = 1;
					}
					#endif // KNOB_BRANCH
				
				}	
		 	}

			// Special case if number of remaining edges is less than the unroll factor
			#if KNOB_UNROLL_FACTOR > 1
		 	else
		 	{
		 		for(int ii = i; ii < end; ii++) 
				{
					id_         = g_graph_edges[ii];

					#if KNOB_BRANCH == 0
					Mask_       = -( !g_graph_visited_mask[id_] );
				 	g_cost[id_] =  ( cost & Mask_) | ( g_cost[id_] & ~Mask_ );

				 	g_graph_updating_mask[id_] = 1 & !g_graph_visited_mask[id_];

					#else
					if(!g_graph_visited_mask[id_])
					{
						g_cost[id_] = g_cost[tid] + 1;
						g_graph_updating_mask[id_] = 1;
					}
					#endif // KNOB_BRANCH
				}
		 	}
			#endif
	    }
	}
}




//--------------------------------------------------------
/**
 * Update the binary masks to check for completion and prepare for the next iteration.
 * @param[out] g_graph_frontier_mask           A binary mask to indicate which nodes must be processed at the next iteration.
 * @param[in]  g_graph_updating_mask  A binary mask indicating how to update the other binary masks.
 * @param[out] g_graph_visited_mask        A binary mask to indicate which nodes have already been visited in the past.
 * @param[out] g_over                 If set to 1, the algorithm is *NOT* over.
 * @param[in]  no_of_nodes            Number of nodes in the graph.
 **/
//--------------------------------------------------------
#ifdef ALTERA_CL
__attribute__ ((reqd_work_group_size(KNOB_NUM_WORK_ITEMS_2,1,1)))
__attribute__ ((num_simd_work_items(KNOB_SIMD_2)))
__attribute__ ((num_compute_units(KNOB_COMPUTE_UNITS_2)))
#endif
__kernel void kernel2(
		__global mask_t* restrict  g_graph_frontier_mask,
		__global mask_t* restrict  g_graph_updating_mask,
		__global mask_t* restrict  g_graph_visited_mask,
		__global int*    restrict  g_over,
		unsigned int no_of_nodes)
{
//--------------------------------------------------------
	unsigned int tid = get_global_id(0);

	if(tid < no_of_nodes && g_graph_updating_mask[tid] == 1)
	{
		g_graph_frontier_mask[tid]    = 1;
		g_graph_visited_mask[tid] = 1;

		*g_over = 1;

		g_graph_updating_mask[tid] = 0;
	}	
}
