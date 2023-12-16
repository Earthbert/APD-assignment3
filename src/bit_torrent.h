#ifndef BIT_TORRENT_H
#define BIT_TORRENT_H

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define NO_CHUNKS -1
#define SEND_ALL_FILES_INFO 0

typedef struct {
	char filename[MAX_FILENAME]; // file name
	char chuck_present[MAX_CHUNKS]; // which chunks are present
	char hashes[MAX_CHUNKS][HASH_SIZE]; // file hashes
	int chunks_count; // number of chunks
} file_info_t; // file info

typedef struct {
	file_info_t file; // file info
	int *peers; // which peers have the file
	int *peers_per_chunk[MAX_CHUNKS]; // which peers have each chunk
} swarm_t; // swarm info

typedef struct {
	int num_files;
	file_info_t files[MAX_FILES];
} mpi_files_info_t;

typedef struct {
	int rank; // peer rank
	int num_peers; // number of peers
	int num_files; // number of files
	int num_files_to_download; // number of files to download
	file_info_t *files; // files info
	file_info_t *files_to_download; // files to download
	mpi_datatypes_t *mpi_datatypes; // mpi datatypes
} args_t; // arguments for peer threads

typedef struct {
	MPI_Datatype mpi_file_info;
	MPI_Datatype mpi_files_info;
} mpi_datatypes_t;

void peer(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes);

void tracker(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes);

#endif // BIT_TORRENT_H