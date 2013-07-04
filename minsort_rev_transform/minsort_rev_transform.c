#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <Judy.h>
#include "map_file.h"

//clock_t last_clock;
long primary_index;

void decode(unsigned char *UST, long len, FILE *outfile);

//void print_time(void)
//{
	//clock_t t = clock();
	//fprintf(stderr, "Elapsed time %.2f secs\n", ((double)t - (double)last_clock) / (double)CLOCKS_PER_SEC);
//}

void usage(void)
{
	fprintf(stderr, "Usage: minsort_rev_transform <infile.ust> <primary_index> <outfile>\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	if(argc < 4) {
		usage();
	}

	unsigned char *UST;
	long len;
	map_in(UST, len, argv[1]);

	FILE *outfile = stdout;
	outfile = fopen(argv[3], "w");
	primary_index = strtol(argv[2], NULL, 10);

	decode(UST, len, outfile);

	return 0;
}

Word_t transition_code(int node, int symbol)
{
	return ((Word_t)symbol << 32) | (Word_t)node;
}

static int *symbol_map;
static int symbol_counts[256];
static int len_;

int count_range_linear(unsigned char *UST, int start, int end, int symbol)
{
	int count=0;
	for(; start < end; start++) {
		if(UST[start] == symbol) {
			count++;
		}
	}
	return count;
}

int binary_search_symbols(int pos, int symbol)
{
	int *A = symbol_map;
	int imin = symbol_counts[symbol];
	int imax, sym_max;
	int key = pos;
	imax = sym_max = symbol < 255 ? symbol_counts[symbol + 1] : len_;

	while (imax > imin)
	{
		int imid = (imin + imax) / 2;

		if (A[imid] < key)
			imin = imid + 1;
		else if (A[imid] > key)
			imax = imid - 1;
		else
			return imid;
	}
	return (imin<sym_max && A[imin] < pos) ? imin+1 : imin;
}

int count_range(unsigned char *UST, int start, int end, int symbol)
{
	if(end - start < 32) {
		return count_range_linear(UST, start, end, symbol);
	}
	else {
		int min = binary_search_symbols(start, symbol);
		int max = binary_search_symbols(end, symbol);

		return max - min;
	}
}

void init_symbol_counts(unsigned char *UST, int len)
{
	int i;
	memset(symbol_counts, 0, sizeof(symbol_counts));
	for(i=0; i<len; i++) {
		symbol_counts[UST[i]]++;
	}
	int sum=0;
	for(i=0; i<256; i++) {
		int tmp = symbol_counts[i];
		symbol_counts[i] = sum;
		sum += tmp;
	}
}

void init_symbol_map(unsigned char *UST, int len)
{
	int i;
	int symbol_map_positions[256];
	memcpy(symbol_map_positions, symbol_counts, sizeof(symbol_map_positions));
	symbol_map = (int *)malloc(sizeof(int) * len);
	for(i=0; i<len; i++) {
		symbol_map[symbol_map_positions[UST[i]]++] = i;
	}
	len_ = len;
}

void decode(unsigned char *UST, long len, FILE *outfile)
{
	int num_nodes = 0;
	int *node_parent = (int *)malloc(sizeof(int) * len);
	int *node_start_pos = (int *)malloc(sizeof(int) * len);
	int *node_end_pos = (int *)malloc(sizeof(int) * len);
	int *node_next_pos = (int *)malloc(sizeof(int) * len);
	int *primary_node = (int *)calloc(len, sizeof(int));

	init_symbol_counts(UST, len);
	init_symbol_map(UST, len);

	Word_t Index;
	Word_t   *PValue;                   // Pointer to array element value.
	int Rc_int;

	Pvoid_t   transitions = NULL;       // Judy array.

	node_start_pos[0] = 0; // make the root node
	node_end_pos[0] = len; // make the root node
	node_next_pos[0] = 1; // make the root node
	node_parent[0] = -1; // make the root node
	num_nodes++;

	//last_clock = clock();
	//int ticks = 0;

	int last_symbol = UST[primary_index];
	//fprintf(stderr, "last_symbol = %d\n", last_symbol);

	long p;
	long i = 0;
	for(p=0; p < len; p++) {
		int c = UST[i];
		int node, dest_node=0;
		assert(num_nodes <= len);

		putc(c, outfile);

		//fprintf(stderr, "At rank %d, position %d\n", (int)i, (int)p);

		// Try to find a destination node, trying each node and then its parent, all the way up to the root
		PValue = NULL;
		for(node=primary_node[i]; node >= 0; node=node_parent[node]) {
			Index = transition_code(node, c);
			JLG(PValue, transitions, Index);
			if(PValue == PJERR) {
				fprintf(stderr, "Judy malloc error. Aborting.\n");
				exit(1);
			}
			if(PValue != NULL) {
				dest_node = *PValue;

				break;
			}
		}

		// The ancestor that had a destination has already been started for this symbol, and the node that it points to was already created.
		// Descendants of this ancestor that are ancestors of the current node will have to be started if they have corresponding destination nodes.
		int started_node = node;

		int pos = node_next_pos[dest_node];
		int orig_pos = pos;
		int last_node_made = 0;
		int last_node_count = 0;
		int contexts_lost = 0;
		for(node=primary_node[i]; node != started_node; node=node_parent[node]) {
			int count = count_range(UST, node_start_pos[node], node_end_pos[node], c);
			if((c == last_symbol) && (node_start_pos[node] <= primary_index) && (node_end_pos[node] > primary_index)) {
				//fprintf(stderr, "Reducing count d/t primary index. next rank = %d, node_start_pos = %d, node_end_pos = %d\n", orig_pos, node_start_pos[node], node_end_pos[node]);
				count--;
			}
			//else if((node_start_pos[node] <= primary_index) && (node_end_pos[node] > primary_index)) {
				//fprintf(stderr, "Not reducing count. last_symbol = %d, c = %d node_start_pos[node] = %d node_end_pos[node] = %d\n", last_symbol, c, node_start_pos[node], node_end_pos[node]);
			//}

			if((last_node_made==0 && count > 1) || (last_node_made>0 && count-last_node_count>0)) {
				int next_node = num_nodes++;
				int j;
				for(j=last_node_count; j<count; j++) {
					primary_node[orig_pos + j] = next_node;
				}
				node_start_pos[next_node] = orig_pos;
				node_end_pos[next_node] = orig_pos + count;
				node_next_pos[next_node] = (last_node_made>0) ? orig_pos + last_node_count : orig_pos + 1;

				Index = transition_code(node, c);
				JLI(PValue, transitions, Index);
				if(PValue == PJERR) {
					fprintf(stderr, "Judy malloc error. Aborting.\n");
					exit(1);
				}
				*PValue = next_node;
				if(last_node_made==0) {
					//fprintf(stderr, "Inserted a transition %d => %d\n", node, dest_node);
				}

				if(last_node_made > 0) {
					node_parent[last_node_made] = next_node;
				}
				last_node_made = next_node;

				pos = orig_pos + count;
				last_node_count = count;

				//fprintf(stderr, "Node %d has %d suffixes\n", next_node, count);
			}
			else {
				contexts_lost++;
			}
		}

		//if(contexts_lost > 0) {
			//fprintf(stderr, "Lost %d nodes. i = %d\n", contexts_lost, (int)i);
		//}

		i = node_next_pos[dest_node];

		if(last_node_made > 0) {
			node_parent[last_node_made] = dest_node;

			node_next_pos[dest_node] += last_node_count;
		}
		else {
			node_next_pos[dest_node]++;
		}

		//if((ticks++ % 10000) == 0) {
			//print_time();
		//}
	}

	JLFA(Rc_int, transitions);
}
