#include "HashObject.h"
#include "sha1.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include <zlib.h>

std::string compute_sha1(const std::vector<char>& data)
{
    SHA1 checksum;
    std::string data_s(data.begin(), data.end());
    checksum.update(data_s);
    return checksum.final();
}

std::string hash_object(const std::string& filename, bool write)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file.\n";
        return "";
    }

    // Read file contents into vector
    std::vector<char> content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Convert to Blob Format
    std::ostringstream blobStream;
    blobStream << "blob " << content.size() << '\0';
    std::string header = blobStream.str();

    // Combine header and content
    std::vector<char> blob_data(header.begin(), header.end());
    blob_data.insert(blob_data.end(), content.begin(), content.end());

    // Compute SHA-1 of the entire blob
    std::string hash = compute_sha1(blob_data);

    if (write) {
        // Construct file path in .git/objects/
        std::string dir = ".git/objects/" + hash.substr(0, 2);
        std::string path = dir + "/" + hash.substr(2);

        // Check if object already exists
        if (std::filesystem::exists(path)) {
            return hash;
        }

        // Ensure directory exists
        std::filesystem::create_directories(dir);

        // Compress the blob using Zlib
        uLongf compressed_size = compressBound(blob_data.size());
        std::vector<char> compressed_data(compressed_size);

        if (compress(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size,
                     reinterpret_cast<const Bytef*>(blob_data.data()), blob_data.size()) != Z_OK) {
            std::cerr << "Error: Compression failed.\n";
            return "";
        }

        // Resize vector to actual compressed size
        compressed_data.resize(compressed_size);

        // Write compressed data to file
        std::ofstream outFile(path, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error: Could not write to file.\n";
            return "";
        }
        outFile.write(compressed_data.data(), compressed_data.size());
    }

    return hash;
}
