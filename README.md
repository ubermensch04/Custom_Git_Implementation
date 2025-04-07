

# Custom Git (C++ Implementation)

This project is a low-level reimplementation of core Git features using C++. It closely follows the structure of Git’s internal object model and mimics its behavior through custom code.

## Features Implemented

- **Repository Setup**  
  Ability to initialize a `.git` directory and configure internal paths.

- **Object Storage**
  - **Blob Objects**: Read and write blob objects using Git’s compression format.
  - **Tree Objects**: Read and construct tree objects referencing other blobs or trees.
  - **Commit Objects**: Create commit objects pointing to tree hashes with metadata.

- **Networking and Cloning**
  - Connect to a remote Git server over TCP.
  - Perform a smart protocol handshake with `git-upload-pack`.
  - Parse and decompress the remote packfile.
  - Reconstruct and store Git objects locally.

## How It Works

- Object headers and data are parsed manually from raw bytes.
- Compressed data is handled using zlib.
- Packfiles are interpreted and decompressed, and their contents written to the `.git/objects` directory.
- Object hashes are computed using SHA-1 for consistency with Git.

## Status

Currently, this project supports reading and writing Git objects, creating commits, and cloning repositories via the smart protocol (partial packfile support). Remaining challenges include full support for delta objects (`ofs-delta`, `ref-delta`).

