# Assignment 3

## Daraban Albert-Timotei

### Main Logic

* Create custom MPI datatype for more efficient communication.
* Start tracker or peer logic depending on rank.

#### Tracker Logic

* Initialize data structures:
  * Array of finished peers (1 at the index of the rank of a peer means that the peer has finished).
  * Swarm array (swarm structure).
* Receive information about which files each peer has.
* Pass through a barrier to signal that file sharing can begin.
* Loop until all peers finish sharing. In this loop, the tracker receives 4 types of messages:
  * Request for information about a file and its peers.
  * Update about a newly downloaded chunk of a peer.
  * Signal that a peer finished downloading a whole file.
  * Signal that a peer finished all files it wanted.

#### Peer Logic

* Send info about the files the peer has.
* Pass through a barrier to signal that it's ready to start sharing.
* Start two threads, one for download logic and the other for upload logic.

##### Upload Logic

* Loop until all peers finish sharing. In this loop, the peer receives 2 types of messages:
  * Request for a chunk from a fellow peer.
  * Message from the tracker that all files finished sharing.

##### Download Logic

* Initialize data structures:
  * Array of ints corresponding to each file to be downloaded, containing the rank of the last peer used to get a chunk from. This is used to distribute the load to other potential peers.
  * Array of ints specifying if a file was fully downloaded.
  * 3-dimensional array to hold for each chunk of a file, which peers have it.
* Loop until finish downloading everything. In this loop, do the following:
  * Get information about wanted files.
  * Download a number of chunks from peers. If after download we have all wanted files, stop the loop.
  * Send info about what chunks it has.
