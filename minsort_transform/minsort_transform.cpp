#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "esa.hxx"

#define alloc_int32s(count) (int32_t *)malloc(sizeof(int32_t) * (count))

struct sa_node_t {
	int32_t parent;
	int32_t width;
	int32_t next;
};

void read_text(FILE *tfile, unsigned char **T, int *n)
{
	unsigned char *textbuf;
	textbuf = NULL;
	int len = 0;
	int c;
	while((c = getc(tfile)) != EOF) {
		if(len % BUFSIZ == 0) {
			textbuf = (unsigned char*)realloc(textbuf, len + BUFSIZ);
		}

		textbuf[len++] = c;
	}

	// reverse text
	for(int i=0; i<len/2; i++) {
		unsigned char tmp = textbuf[i];
		textbuf[i] = textbuf[len - i - 1];
		textbuf[len - i - 1] = tmp;
	}


	textbuf = (unsigned char*)realloc(textbuf, len);

	*T = textbuf;
	*n = len; 
}

void dump_array(int32_t *A, int len, const char *fname)
{
	FILE *outfile = fopen(fname, "wb");
	fwrite(A, sizeof(int32_t), len, outfile);
	fclose(outfile);
}

// Associate each suffix with a node, and connect nodes to parents
// Nodes are in post-order
void find_nodes_and_parents(int32_t *SA, int32_t *L, int32_t *R, int nodeNum, int32_t *primary_nodes, sa_node_t *nodes)
{
	int i, needs_parent = -1;
	for(i=0; i<nodeNum; i++) {
		int suffix = R[i] - 1;

		// see if this node has children
		while(needs_parent != -1 && (L[i] <= L[needs_parent] && R[i] >= R[needs_parent])) {
			// add suffixes not enclosed by the child to the current node
			while(suffix >= R[needs_parent]) {
				primary_nodes[SA[suffix--]] = i;
			}
			suffix = L[needs_parent] - 1;

			int tmp = nodes[needs_parent].parent;
			nodes[needs_parent].parent = i;
			needs_parent = tmp;
		}

		// add remaining suffixes to the current node
		while(suffix >= L[i]) {
			primary_nodes[SA[suffix--]] = i;
		}

		// add current node to the needs_parent list
		nodes[i].parent = needs_parent;
		needs_parent = i;

		// width
		nodes[i].width = R[i] - L[i];
		nodes[i].next = 0;
	}
	// the last node is the root
	nodes[nodeNum-1].parent = -1;
}

void compute_transform(int32_t *primary_nodes, int len, sa_node_t *nodes, int nodeNum, int32_t *transform)
{
	int i;
	for(i = len; i-- > 0; ) {
		int node, ancestor, index, node_suffixes;

		// look for an ancestor that's been started
		for(ancestor = primary_nodes[i]; ancestor != -1 && nodes[ancestor].next == 0; ancestor = nodes[ancestor].parent) {
		}

		// next open position in the ancestor, or 0 if this is the first suffix
		index = (ancestor == -1) ? 0 : nodes[ancestor].next;

		// start unstarted ancestors
		node_suffixes = 1;
		for(node = primary_nodes[i]; node != ancestor; node = nodes[node].parent) {
			nodes[node].next = index + node_suffixes;
			node_suffixes = nodes[node].width;
		}

		// increment started ancestor
		if(ancestor != -1) {
			nodes[ancestor].next += node_suffixes;
		}

		// map rank to text position, as in suffix array
		transform[index] = i;
	}
}

void usage()
{
	fprintf(stderr, "Usage: minsort_transform [-a|-t|-n] <infile> <outfile>\n");
	fprintf(stderr, "   -t: output the transform (default)\n");
	fprintf(stderr, "   -a: output the position array\n");
	fprintf(stderr, "   -n: output an array with the suffix tree node for each position\n");
	fprintf(stderr, "   To use stdin/stdout, either filename may be specified as -.\n");
	exit(1);
}

extern int optind;
extern char *optarg;

enum {
	MODE_TRANSFORM,
	MODE_ARRAY,
	MODE_ST_NODES,
	MODE_FIRSTCOL,
};
int output_mode = MODE_TRANSFORM;

int main(int argc, char **argv)
{
	unsigned char *T;
	int len;

	int opt;
	while ((opt = getopt(argc, argv, "atnf")) != -1) {
		switch (opt) {
		case 'a':
			output_mode = MODE_ARRAY;
			break;
		case 't':
			output_mode = MODE_TRANSFORM;
			break;
		case 'n':
			output_mode = MODE_ST_NODES;
			break;
		case 'f':
			output_mode = MODE_FIRSTCOL;
			break;
		default: /* '?' */
			usage();
		}
	}

	if(argc - optind < 2) {
		usage();
	}

	const char *infilename = argv[optind++];
	const char *outfilename = argv[optind++];

	if(!strcmp(infilename, "-")) {
		read_text(stdin, &T, &len);
	}
	else {
		FILE *infile = fopen(infilename, "rb");
		read_text(infile, &T, &len);
		fclose(infile);
	}


	int32_t *SA = alloc_int32s(len);
	int32_t *L  = alloc_int32s(len);
	int32_t *R  = alloc_int32s(len);
	int32_t *D  = alloc_int32s(len);

	int nodeNum = 0;
	if (esaxx(T, SA, L, R, D, len, 256, nodeNum) == -1) {
		fprintf(stderr, "esaxx returned -1\n");
		return 1;
	}
	free(D); // don't need depth


	sa_node_t *nodes = (sa_node_t *)malloc(sizeof(sa_node_t) * nodeNum);
	int32_t *primary_nodes = alloc_int32s(len);

	find_nodes_and_parents(SA, L, R, nodeNum, primary_nodes, nodes);

	free(R);
	free(L);

	int i;
	for(i=0; i<len; i++) {
		if(nodes[primary_nodes[i]].width < 3) {
			//printf("Got rid of a node, %d, width = %d\n", primary_nodes[i], nodes[primary_nodes[i]].width);
			primary_nodes[i] = nodes[primary_nodes[i]].parent;
		}
	}

	// parents[0..nodeNum-1] -> maps each node to its parent
	// primary_nodes[0..len-1] -> maps each suffix (by position) to the deepest (non-leaf) node that includes it

	int32_t *transform = alloc_int32s(len);
	compute_transform(primary_nodes, len, nodes, nodeNum, transform);

	FILE *outfile = (strcmp(outfilename, "-")==0) ? stdout : fopen(outfilename, "wb");
	if(output_mode == MODE_ARRAY) {
		fwrite(transform, sizeof(int32_t), len, outfile);
	}
	else if(output_mode == MODE_TRANSFORM) {
		int primary_index;
		int syms_written = 0;
		putc(T[len-1], outfile);
		syms_written++;
		for(int i=0; i<len; i++) {
			if(transform[i] > 0) {
				putc(T[transform[i] - 1], outfile);
				if(transform[i] == 1) {
					primary_index = syms_written;
				}
				syms_written++;
			}
		}
		fprintf(stderr, "Primary index: %d\n", primary_index);
	}
	else if(output_mode == MODE_FIRSTCOL) {
		for(int i=0; i<len; i++) {
			putc(T[transform[i]], outfile);
		}
		if(outfile == stdout) {
			fflush(stdout);
			fputs("\n", stderr);
		}
	}
	else if(output_mode == MODE_ST_NODES) {
		int32_t *node_array = alloc_int32s(len);
		int32_t *node_order = alloc_int32s(nodeNum);
		memset(node_order, -1, sizeof(int32_t) * nodeNum);
		int32_t node_order_num = 0;
		for(int i=len; i-->0; ) {
			if(node_order[primary_nodes[i]] < 0) {
				node_order[primary_nodes[i]] = node_order_num++;
			}
		}
		for(int i=0; i<len; i++) {
			node_array[i] = node_order[primary_nodes[transform[i]]];
		}
		fwrite(node_array, sizeof(int32_t), len, outfile);
	}

	return 0;
}
