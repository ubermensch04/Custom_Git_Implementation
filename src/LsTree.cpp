#include"LsTree.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include <cstring>
#include<algorithm>

std::vector<unsigned char> decompressed_zlib(const std::vector<unsigned char>& compressed_data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("inflateInit failed");
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<unsigned char*>(compressed_data.data()));
    zs.avail_in = compressed_data.size();

    std::vector<unsigned char> decompressed_data;
    unsigned char outbuffer[32768];

    int ret;
    do {
        zs.next_out = outbuffer;
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);
        if (ret == Z_STREAM_ERROR) {
            throw std::runtime_error("Zlib decompression failed");
        }

        size_t decompressed_size = zs.total_out - decompressed_data.size();
        decompressed_data.insert(decompressed_data.end(), outbuffer, outbuffer + decompressed_size);

    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Exception during zlib decompression: (" + std::to_string(ret) + ")");
    }

    return decompressed_data;
}

std::vector<std::string> ls_tree(const std::string& tree_hash)
{
    // Constructing the object file path
    std::string tree_object_path = ".git/objects/" + tree_hash.substr(0, 2) + "/" + tree_hash.substr(2);

    // Open the object file
    std::ifstream file(tree_object_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Tree Object not found.\n";
        return {};
    }

    // Read compressed tree object data
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> compressed_data(fileSize);
    if (!file.read(reinterpret_cast<char*>(compressed_data.data()), fileSize)) {
        std::cerr << "Error: Failed to read the blob file.\n";
        return {};
    }

    // Decompress the tree object data
    std::vector<unsigned char> decompressed_data;
    try {
        decompressed_data = decompressed_zlib(compressed_data);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return {};
    }

    //Parsing the tree object
    std::vector<std::string> filenames;
    size_t i = 0;

    // Skip the "tree <size>\0" header
    while (i < decompressed_data.size() && decompressed_data[i] != '\0') {
        i++;
    }
    i++; // Move past the null byte

    // Process each entry
    while (i < decompressed_data.size()) {
        // Extract mode (until space)
        std::string mode;
        while (i < decompressed_data.size() && decompressed_data[i] != ' ') {
            mode += decompressed_data[i++];
        }
        i++; // Skip space

        // Extract filename (until null byte)
        std::string filename;
        while (i < decompressed_data.size() && decompressed_data[i] != '\0') {
            filename += decompressed_data[i++];
        }
        i++; // Skip null byte

        // Skip the 20-byte SHA hash
        i += 20;

        // Store filename
        filenames.push_back(filename);
    }

    return filenames;

}


