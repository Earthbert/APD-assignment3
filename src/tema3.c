#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define REQUEST_CHUNK 1
#define SEND_CHUNK 2
#define REQUEST_FILE_INFO 3
#define SEND_FILE_INFO 4

typedef struct {
	char filename[MAX_FILENAME]; // file name
	char hashes[MAX_CHUNKS][HASH_SIZE]; // file hashes
	int chunks_count; // number of chunks
	int full; // 1 if we have the whole file, 0 otherwise
	int last_requested_peer; // last peer that we requested a chunk from
} file_info; // file info used for peers

typedef struct {
	int rank; // peer rank
	int num_peers; // number of peers
	int num_files; // number of files
	int num_files_to_download; // number of files to download
	file_info *files; // files info
	file_info *files_to_download; // files to download
} args; // arguments for peer threads

args *prepare_thread_args(int rank, int numtasks) {
	char filename[MAX_FILENAME] = "in";
	sprintf(filename, "in%d.txt", rank);

	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		printf("Eroare la deschiderea fisierului %s\n", filename);
		exit(-1);
	}
	args *thread_args = malloc(sizeof(args));
	thread_args->rank = rank;
	thread_args->num_peers = numtasks - 1;

	fscanf(f, "%d", &thread_args->num_files);
	file_info *files = malloc(thread_args->num_files * sizeof(file_info));
	for (int i = 0; i < thread_args->num_files; i++) {
		fscanf(f, "%s", files[i].filename);
		fscanf(f, "%d", &files[i].chunks_count);
		for (int j = 0; j < files[i].chunks_count; j++) {
			fscanf(f, "%s", files[i].hashes[j]);
		}
		files[i].full = 1;
		files[i].last_requested_peer = -1;
	}
	thread_args->files = files;

	fscanf(f, "%d", &thread_args->num_files_to_download);
	file_info *files_to_download = malloc(thread_args->num_files_to_download * sizeof(file_info));
	for (int i = 0; i < thread_args->num_files_to_download; i++) {
		fscanf(f, "%s", files_to_download[i].filename);
		files_to_download[i].chunks_count = 0;
		files_to_download[i].full = 0;
		files_to_download[i].last_requested_peer = -1;
	}
	thread_args->files_to_download = files_to_download;

	fclose(f);
	return thread_args;
}

void *download_thread_func(void *arg) {
	int rank = *(int *)arg;

	return NULL;
}

void *upload_thread_func(void *arg) {
	int rank = *(int *)arg;

	return NULL;
}

void tracker(int numtasks, int rank) {

}

void peer(int numtasks, int rank) {
	pthread_t download_thread;
	pthread_t upload_thread;
	void *status;
	int r;

	args *thread_args = prepare_thread_args(rank, numtasks);

	r = pthread_create(thread_args, NULL, download_thread_func, (void *)&rank);
	if (r) {
		printf("Eroare la crearea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_create(thread_args, NULL, upload_thread_func, (void *)&rank);
	if (r) {
		printf("Eroare la crearea thread-ului de upload\n");
		exit(-1);
	}

	r = pthread_join(download_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de download\n");
		exit(-1);
	}

	r = pthread_join(upload_thread, &status);
	if (r) {
		printf("Eroare la asteptarea thread-ului de upload\n");
		exit(-1);
	}
}

int main(int argc, char *argv[]) {
	int numtasks, rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (rank == TRACKER_RANK) {
		tracker(numtasks, rank);
	} else {
		peer(numtasks, rank);
	}

	MPI_Finalize();
}
