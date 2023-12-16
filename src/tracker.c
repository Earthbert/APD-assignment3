#include <mpi.h>
#include <pthread.h>

#include "bit_torrent.h"

void tracker(int numtasks, int rank) {
	int num_peers = numtasks - 1;

	swarm swarms[MAX_FILES];
	// receive files info from peers
	for (int i = 0; i < num_peers; i++) {
		
	}
}