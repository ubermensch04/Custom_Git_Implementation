#ifndef WRITETREE_H
#define WRITETREE_H

#include <string>

// Function to write the working directory as a Git tree object
std::string write_tree(const std::string& directory = ".");

#endif // WRITETREE_H
