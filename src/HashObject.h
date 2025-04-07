#ifndef HASH_OBJECT_H
#define HASH_OBJECT_H

#include <string>
#include <vector>

std::string compute_sha1(const std::vector<char>& data);

// Function to read a blob object from the Git repository
std::string hash_object(const std::string& filename,bool write);

#endif