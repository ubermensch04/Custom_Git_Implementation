#include "WriteTree.h"
#include "HashObject.h"
#include "sha1.hpp"
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include <algorithm>
#include <tuple>

namespace fs = std::filesystem;

std::string write_tree(const std::string& directory) {
    // We'll store each entry as a tuple: (mode, name, sha)
    std::vector<std::tuple<std::string, std::string, std::string>> entries;

    // Iterate over the contents of the directory.
    for (const auto& entry : fs::directory_iterator(directory)) {
        std::string name = entry.path().filename().string();
        if (name == ".git") continue; // Ignore .git directory

        std::string sha;
        std::string mode;
        
        if (fs::is_regular_file(entry.path())) {
            // For files, use hash_object to store as a blob.
            sha = hash_object(entry.path().string(), true);
            mode = "100644";
        } 
        else if (fs::is_directory(entry.path())) {
            // For directories, recursively create a tree.
            sha = write_tree(entry.path().string());
            mode = "40000";
        }

        entries.push_back({mode, name, sha});
    }

    // Sort entries solely by name (alphabetically).
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return std::get<1>(a) < std::get<1>(b);
    });

    // Construct the tree data (each entry: "<mode> <name>\0<20-byte SHA>")
    std::vector<char> tree_data;
    for (const auto& entry : entries) {
        std::ostringstream entry_stream;
        entry_stream << std::get<0>(entry) << " " << std::get<1>(entry) << '\0';
        std::string entry_header = entry_stream.str();

        // Convert SHA-1 from hex (40 characters) to binary (20 bytes)
        std::vector<char> sha_binary(20);
        for (size_t i = 0; i < 40; i += 2) {
            sha_binary[i / 2] = static_cast<char>(std::stoi(std::get<2>(entry).substr(i, 2), nullptr, 16));
        }

        tree_data.insert(tree_data.end(), entry_header.begin(), entry_header.end());
        tree_data.insert(tree_data.end(), sha_binary.begin(), sha_binary.end());
    }

    // Construct the tree header: "tree <size>\0"
    std::ostringstream tree_stream;
    tree_stream << "tree " << tree_data.size() << '\0';
    std::string tree_header = tree_stream.str();

    // Combine header and tree data into one buffer.
    std::vector<char> full_tree(tree_header.begin(), tree_header.end());
    full_tree.insert(full_tree.end(), tree_data.begin(), tree_data.end());

    // Compute SHA-1 hash of the full tree object.
    std::string tree_hash = compute_sha1(full_tree);

    // Determine the object path in .git/objects/
    std::string obj_dir = ".git/objects/" + tree_hash.substr(0, 2);
    std::string path = obj_dir + "/" + tree_hash.substr(2);
    fs::create_directories(obj_dir);

    // Compress the tree object with zlib.
    uLongf compressed_size = compressBound(full_tree.size());
    std::vector<char> compressed_data(compressed_size);
    if (compress(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size,
                 reinterpret_cast<const Bytef*>(full_tree.data()), full_tree.size()) != Z_OK) {
        std::cerr << "Error: Compression failed.\n";
        return "";
    }
    compressed_data.resize(compressed_size);

    // Write the compressed tree object to the file.
    std::ofstream outFile(path, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error: Could not write to file.\n";
        return "";
    }
    outFile.write(compressed_data.data(), compressed_data.size());

    return tree_hash;
}
