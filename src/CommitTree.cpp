#include "CommitTree.h"
#include "HashObject.h"
#include "sha1.hpp"
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <zlib.h>
#include <algorithm>

std::string commit_tree(const std::string& tree_sha, const std::string& parent_sha, const std::string& message)
{
    // Step 1: Preparing Commit Object Content
    std::ostringstream commit_stream;

    commit_stream << "tree " << tree_sha << "\n";

    if (!parent_sha.empty()) {
        commit_stream << "parent " << parent_sha << "\n";
    }

    commit_stream << "author John Doe <john@example.com> 1708545600 +0000\n";
    commit_stream << "committer John Doe <john@example.com> 1708545600 +0000\n";

    // Required empty line before message
    commit_stream << "\n" << message<< "\n";

    std::string commit_content = commit_stream.str();

    // Step 2: Construct Commit Header
    std::ostringstream header_stream;
    header_stream << "commit " << commit_content.size() << '\0';
    std::string header = header_stream.str();

    // Combine header and commit content
    std::vector<char> full_commit(header.begin(), header.end());
    full_commit.insert(full_commit.end(), commit_content.begin(), commit_content.end());

    // Step 3: Compute SHA-1
    std::string commit_hash = compute_sha1(full_commit);

    // Step 4: Write Commit to .git/objects/
    std::string dir = ".git/objects/" + commit_hash.substr(0, 2);
    std::string path = dir + "/" + commit_hash.substr(2);
    
    if (std::filesystem::exists(path)) {
        return commit_hash;
    }

    std::filesystem::create_directories(dir);

    // Compress the commit using Zlib
    uLongf compressed_size = compressBound(full_commit.size());
    std::vector<char> compressed_data(compressed_size);

    if (compress(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size,
                 reinterpret_cast<const Bytef*>(full_commit.data()), full_commit.size()) != Z_OK) {
        std::cerr << "Error: Compression failed.\n";
        return "";
    }

    compressed_data.resize(compressed_size);

    std::ofstream outFile(path, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error: Could not write to file.\n";
        return "";
    }
    outFile.write(compressed_data.data(), compressed_data.size());

    return commit_hash;
}
