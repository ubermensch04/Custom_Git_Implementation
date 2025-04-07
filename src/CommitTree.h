#ifndef COMMIT_TREE_H
#define COMMIT_TREE_H

#include <string>

std::string commit_tree(const std:: string& tree_sha, const std::string& parent_sha = "", const std::string& message = "");

#endif // COMMIT_TREE