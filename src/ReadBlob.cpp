#include"ReadBlob.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <zlib.h>
#include <cstring>
#include<algorithm>

std::vector<unsigned char> decompress_zlib(const std::vector<unsigned char>& compressed_data) {
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

void read_blob(const std::string& blob_hash) {
    // Constructing the object file path
    std::string object_path = ".git/objects/" + blob_hash.substr(0, 2) + "/" + blob_hash.substr(2);

    // Open the object file
    std::ifstream file(object_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Blob Object not found.\n";
        return;
    }

    // Read compressed data
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> compressed_data(fileSize);
    if (!file.read(reinterpret_cast<char*>(compressed_data.data()), fileSize)) {
        std::cerr << "Error: Failed to read the blob file.\n";
        return;
    }

    // Decompress the file content
    std::vector<unsigned char> decompressed_data;
    try {
        decompressed_data = decompress_zlib(compressed_data);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return;
    }

    // Find the null separator between header and content
    auto null_pos = std::find(decompressed_data.begin(), decompressed_data.end(), '\0');
    if (null_pos == decompressed_data.end()) {
        std::cerr << "Error: No null character found in decompressed data.\n";
        return;
    }

    // Extract and verify header
    std::string header(decompressed_data.begin(), null_pos);
    std::string content(null_pos + 1, decompressed_data.end());

    size_t space_pos = header.find(' ');
    if (space_pos == std::string::npos) {
        std::cerr << "Error: Malformed blob header.\n";
        return;
    }

    std::string type = header.substr(0, space_pos);
    if (type != "blob") {
        std::cerr << "Error: Object is not a blob.\n";
        return;
    }

    // Print the blob content
    std::cout << content;
}
