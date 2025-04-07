#ifndef READ_BLOB_H
#define READ_BLOB_H

#include <string>
#include <vector>

// Function to decompress zlib-compressed data
std::vector<unsigned char> decompress_zlib(const std::vector<unsigned char>& compressed_data);

// Function to read a blob object from the Git repository
void read_blob(const std::string& blob_hash);

#endif
