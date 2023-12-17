#ifndef BIT_TORRENT_H
#define BIT_TORRENT_H

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define UPDATE_INTERVAL 10

#define TAG_PEER_UPDATE_FILES_INFO 0
#define TAG_PEER_REQUEST_FILE_INFO_AND_PEERS 1
#define TAG_PEER_REQUEST_CHUNK 3
#define TAG_PEER_FILE_REQ_RESPONSE 4
#define TAG_PEER_FINISHED_FILE 5
#define TAG_PEER_FINISHED_ALL_FILES 6

#define TAG_TRACKER_FILE_INFO 20
#define TAG_TRACKER_PEERS_PER_CHUNK 21
#define TAG_TRACKER_END 22

#define REQ_TYPE_FILE_INFO 0
#define REQ_TYPE_UPDATE_FILES 1
#define REQ_TYPE_FINISHED_FILE 2
#define REQ_TYPE_FINISHED_ALL 3
#define NUM_REQUEST_TYPES 4

#define REQ_TYPE_CHUNK 0
#define REQ_TYPE_FINISH 1
#define NUM_PEER_REQUEST_TYPES 2

typedef struct {
	char str[HASH_SIZE]; // chunk hash
} hash;

typedef struct {
	char filename[MAX_FILENAME]; // file name
	char chuck_present[MAX_CHUNKS]; // which chunks are present
	hash hashes[MAX_CHUNKS]; // file hashes
	int chunks_count; // number of chunks
} file_info_t; // file info

typedef struct {
	file_info_t file; // file info
	int *peers; // which peers have the file
	int *peers_per_chunk[MAX_CHUNKS]; // which peers have each chunk
} swarm_t; // swarm info

typedef struct {
	char filename[MAX_FILENAME];
	hash hash;
} peer_request_t;

typedef struct {
	MPI_Datatype mpi_hash;
	MPI_Datatype mpi_file_info;
	MPI_Datatype mpi_peer_request;
} mpi_datatypes_t;

typedef struct {
	int num_peers; // number of peers
	int num_files; // number of files
	int num_files_to_download; // number of files to download
	file_info_t *files; // files info
	file_info_t *files_to_download; // files to download
	mpi_datatypes_t *mpi_datatypes; // mpi datatypes
} args_t; // arguments for peer threads

void peer(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes);

void tracker(int numtasks, int rank, mpi_datatypes_t *mpi_datatypes);

#endif // BIT_TORRENT_H