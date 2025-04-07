#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include"HashObject.h"
#include"ReadBlob.h"
#include"LsTree.h"
#include"WriteTree.h"
#include"CommitTree.h"
#include"CloneRepo.h"

int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cerr << "Logs from your program will appear here!\n";

    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    
    std::string command = argv[1];
    
    if (command == "init") {
        try {
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
    
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }
    
            std::cout << "Initialized git directory\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    else if (command == "cat-file") {
        if (argc < 4) {
            std::cerr << "Usage: ./your_git cat-file -p <blob_hash>\n";
            return EXIT_FAILURE;
        }
    
        std::string option = argv[2];
        std::string blob_hash = argv[3];
    
        if (option == "-p") {
            read_blob(blob_hash);
        } else {
            std::cerr << "Unsupported option for cat-file.\n";
            return EXIT_FAILURE;
        }
    }
    else if (command == "hash-object") {
        if (argc < 4) {
            std::cerr << "Error: Missing arguments.\n";
            return 1;
        }
    
        std::string option = argv[2];
        std::string filename = argv[3];
    
        if (option != "-w") {
            std::cerr << "Error: Only 'hash-object -w' is supported.\n";
            return 1;
        }
    
        std::string hash = hash_object(filename, true);
        std::cout << hash << std::endl;
    }
    else if(command=="ls-tree")
    {
        if (argc < 4) {
            std::cerr << "Error: Missing arguments.\n";
            return 1;
        }
    
        std::string option = argv[2];
        std::string tree_hash = argv[3];
    
        if (option != "--name-only") {
            std::cerr << "Error: Only 'ls-tree --name-only' is supported.\n";
            return 1;
        }
    
         // Call ls_tree and print filenames
         std::vector<std::string> output = ls_tree(tree_hash);
         for (const auto& name : output) {
             std::cout << name << "\n";
         }
    }
    else if (command == "write-tree") {
        if (argc < 2) { 
            std::cerr << "Error: Missing arguments.\n";
            return 1;
        }
        std::string tree_hash = write_tree(".");
        std::cout << tree_hash;   
    }
    else if (command == "commit-tree") {
        if (argc < 4) {  // Minimum required args: commit-tree <tree_sha> -m <message>
            std::cerr << "Error: Invalid arguments for commit-tree.\n";
            return 1;
        }
    
        std::string tree_sha = argv[2];  // First argument after commit-tree
        std::string parent_sha = "";
        std::string message = "";
    
        // Parse arguments
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
    
            if (arg == "-p" && i + 1 < argc) {
                parent_sha = argv[i + 1];  // Next argument is parent commit SHA
                i++; // Skip next argument since it's the SHA
            } 
            else if (arg == "-m" && i + 1 < argc) {
                message = argv[i + 1];  // Next argument is the commit message
                break;  // Message is the last argument
            } 
            else {
                std::cerr << "Error: Invalid commit-tree syntax.\n";
                return 1;
            }
        }
    
        if (message.empty()) {
            std::cerr << "Error: Commit message is required.\n";
            return 1;
        }
    
        // Call commit_tree() and output the SHA
        std::string commit_sha = commit_tree(tree_sha, parent_sha, message);
        if (commit_sha.empty()) {
            std::cerr << "Error: Failed to create commit.\n";
            return 1;
        }
    
        std::cout << commit_sha << std::endl;
    }
    else if (command == "clone") {
        if (argc < 4) 
        {  
            std::cerr << "Usage: ./your_program clone <repo_url> <dest_dir>\n";
            return 1;
        }
        std::string repo_url = argv[2];
        std::string dest_dir = argv[3];
        clone_repo(repo_url, dest_dir);
    }
    
            
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
