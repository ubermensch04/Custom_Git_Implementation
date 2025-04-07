#include "CloneRepo.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <curl/curl.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <unordered_map>
#include <cstdint>
#include <zlib.h>
#include <stdexcept>
#include"sha1.hpp"
// --- cURL Helpers ---

size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* user_data) {
    size_t total_size = size * nmemb;
    user_data->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string fetch_refs(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error initializing cURL\n";
        return "";
    }
    std::string fetch_response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fetch_response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "cURL error: " << curl_easy_strerror(res) << "\n";
        fetch_response.clear();
    }
    curl_easy_cleanup(curl);
    return fetch_response;
}

std::string send_post_request(const std::string& url, const std::string& post_data) {
    CURL* curl;
    CURLcode res;
    std::string fetch_response;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/x-git-upload-pack-request");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.length());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fetch_response);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return fetch_response;
}

// --- Parsing Fetch Response ---

std::unordered_map<std::string, std::string> parse_fetch_response(std::string& fetch_response) {
    std::unordered_map<std::string, std::string> references;
    size_t pos = 0;
    while (pos < fetch_response.size()) {
        if (pos + 4 > fetch_response.size()) break;
        std::string pkt_hex_len = fetch_response.substr(pos, 4);
        if (pkt_hex_len == "0000") { pos += 4; continue; }
        unsigned int pkt_dec_len;
        std::stringstream ss;
        ss << std::hex << pkt_hex_len;
        ss >> pkt_dec_len;
        if (pos + pkt_dec_len > fetch_response.size()) break;
        std::string pkt_content = fetch_response.substr(pos + 4, pkt_dec_len - 4);
        pos += pkt_dec_len;
        if (pkt_content.size() < 40) continue;
        std::string sha1 = pkt_content.substr(0, 40);
        size_t split_pos = pkt_content.find_first_of(" \0", 40);
        if (split_pos == std::string::npos) continue;
        std::string branch = pkt_content.substr(split_pos + 1);
        size_t null_pos = branch.find('\0');
        if (null_pos != std::string::npos) {
            branch = branch.substr(0, null_pos);
        }
        references[branch] = sha1;
    }
    return references;
}

std::vector<uint8_t> extract_packfile_data(std::string& post_request_response) {
    std::vector<uint8_t> packfile_data;
    size_t pos = 0;
    while (pos < post_request_response.length()) {
        if (pos + 4 > post_request_response.length()) break;
        std::string len_str = post_request_response.substr(pos, 4);
        pos += 4;
        if (len_str == "0000") continue;
        unsigned int pkt_len;
        std::istringstream iss(len_str);
        iss >> std::hex >> pkt_len;
        if (pos + (pkt_len - 4) > post_request_response.length()) break;
        std::string pkt_data = post_request_response.substr(pos, pkt_len - 4);
        pos += pkt_len - 4;
        uint8_t channel = static_cast<uint8_t>(pkt_data[0]);
        if (channel == 1) {
            packfile_data.insert(packfile_data.end(), pkt_data.begin() + 1, pkt_data.end());
        }
    }
    return packfile_data;
}

// --- Improved Decompression Loop ---
std::vector<uint8_t> decompress_object(const std::vector<uint8_t>& pack_data,
                                         size_t start_pos,
                                         size_t uncompressed_size,
                                         size_t &compressed_consumed) {
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = const_cast<Bytef*>(pack_data.data() + start_pos);
    strm.avail_in = pack_data.size() - start_pos;
    
    std::vector<uint8_t> uncompressed_data(uncompressed_size);
    strm.next_out = uncompressed_data.data();
    strm.avail_out = uncompressed_size;
    
    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        throw std::runtime_error("inflateInit failed");
    }
    
    // Improved loop: repeatedly call inflate until finished.
    while (true) {
        ret = inflate(&strm, Z_SYNC_FLUSH);
        if (ret == Z_STREAM_END) break;
        if (ret != Z_OK && ret != Z_BUF_ERROR) {
            inflateEnd(&strm);
            throw std::runtime_error("inflate error: " + std::to_string(ret));
        }
        // If no progress is made, break (to avoid infinite loop).
        if (strm.avail_in == 0) break;
    }
    
    if (ret != Z_STREAM_END) {
         inflateEnd(&strm);
         throw std::runtime_error("inflate did not finish properly: " + std::to_string(ret));
    }
    
    compressed_consumed = strm.total_in;
    inflateEnd(&strm);
    return uncompressed_data;
}

// --- Global Object Storage ---
std::unordered_map<std::string, std::vector<uint8_t>> parsed_objects;

std::string compute_object_sha1(const std::vector<char>& data) {
    SHA1 checksum;
    std::string data_s(data.begin(), data.end());
    checksum.update(data_s);
    return checksum.final();
}

// --- Base Object Lookup ---
std::vector<uint8_t> get_base_object(const std::string& base_sha) {
    auto it = parsed_objects.find(base_sha);
    if (it != parsed_objects.end()) {
        return it->second;
    }
    throw std::runtime_error("Base object lookup failed for SHA: " + base_sha);
}

// --- Store Object to Disk ---
std::string store_object(const std::string& type, const std::vector<uint8_t>& uncompressed_data) {
    std::vector<char> uncompressed_char(uncompressed_data.begin(), uncompressed_data.end());
    std::ostringstream header_stream;
    header_stream << type << " " << uncompressed_data.size() << '\0';
    std::string header = header_stream.str();
    
    std::vector<char> full_object;
    full_object.insert(full_object.end(), header.begin(), header.end());
    full_object.insert(full_object.end(), uncompressed_char.begin(), uncompressed_char.end());
    
    std::string object_hash = compute_object_sha1(full_object);
    
    std::string subdir = object_hash.substr(0, 2);
    std::string filename = object_hash.substr(2);
    std::string object_path = ".git/objects/" + subdir + "/" + filename;
    
    if (std::filesystem::exists(object_path))
        return object_hash;
    
    std::filesystem::create_directories(".git/objects/" + subdir);
    
    uLongf compressed_size = compressBound(full_object.size());
    std::vector<uint8_t> compressed_data(compressed_size);
    int ret = compress(reinterpret_cast<Bytef*>(compressed_data.data()), &compressed_size,
                       reinterpret_cast<const Bytef*>(full_object.data()), full_object.size());
    if (ret != Z_OK)
        throw std::runtime_error("Compression failed in store_object.");
    compressed_data.resize(compressed_size);
    
    std::ofstream out(object_path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Could not open file to write object.");
    out.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_data.size());
    out.close();
    
    parsed_objects[object_hash] = uncompressed_data;
    
    return object_hash;
}

// --- Delta Application ---
std::vector<uint8_t> apply_delta(const std::vector<uint8_t>& base, const std::vector<uint8_t>& delta) {
    size_t pos = 0;
    auto read_varlen = [&](const std::vector<uint8_t>& stream, size_t &p) -> size_t {
        size_t result = 0;
        int shift = 0;
        while (true) {
            if (p >= stream.size())
                throw std::runtime_error("Delta stream ended unexpectedly.");
            uint8_t byte = stream[p++];
            result |= (byte & 0x7F) << shift;
            if (!(byte & 0x80))
                break;
            shift += 7;
        }
        return result;
    };
    
    size_t expected_base_size = read_varlen(delta, pos);
    if (expected_base_size != base.size())
        throw std::runtime_error("Delta base size does not match actual base object size.");
    
    size_t result_size = read_varlen(delta, pos);
    std::vector<uint8_t> result(result_size);
    size_t result_pos = 0;
    
    while (pos < delta.size() && result_pos < result_size) {
        uint8_t opcode = delta[pos++];
        if (opcode & 0x80) { 
            size_t copy_offset = 0;
            size_t copy_size = 0;
            if (opcode & 0x01) { copy_offset |= delta[pos++]; }
            if (opcode & 0x02) { copy_offset |= delta[pos++] << 8; }
            if (opcode & 0x04) { copy_offset |= delta[pos++] << 16; }
            if (opcode & 0x08) { copy_offset |= delta[pos++] << 24; }
            if (opcode & 0x10) { copy_size |= delta[pos++]; }
            if (opcode & 0x20) { copy_size |= delta[pos++] << 8; }
            if (opcode & 0x40) { copy_size |= delta[pos++] << 16; }
            if (copy_size == 0) { copy_size = 0x10000; }
            
            if (copy_offset + copy_size > base.size() || result_pos + copy_size > result_size)
                throw std::runtime_error("Delta copy instruction out of bounds.");
            std::copy(base.begin() + copy_offset, base.begin() + copy_offset + copy_size, result.begin() + result_pos);
            result_pos += copy_size;
        } else if (opcode) {
            size_t insert_size = opcode;
            if (pos + insert_size > delta.size() || result_pos + insert_size > result_size)
                throw std::runtime_error("Delta insert instruction out of bounds.");
            std::copy(delta.begin() + pos, delta.begin() + pos + insert_size, result.begin() + result_pos);
            pos += insert_size;
            result_pos += insert_size;
        } else {
            throw std::runtime_error("Delta opcode of 0 is not allowed.");
        }
    }
    
    if (result_pos != result_size)
        throw std::runtime_error("Delta result size mismatch.");
    
    return result;
}

// --- Packfile Parsing ---
void parse_packfile(const std::vector<uint8_t>& pack_data) {
    if (pack_data.size() < 12) {
        std::cerr << "Packfile too small!" << std::endl;
        return;
    }
    
    if (std::memcmp(pack_data.data(), "PACK", 4) != 0) {
        std::cerr << "Invalid packfile (missing PACK magic)!" << std::endl;
        return;
    }
    uint32_t version = (pack_data[4] << 24) | (pack_data[5] << 16) | (pack_data[6] << 8) | (pack_data[7]);
    uint32_t object_count = (pack_data[8] << 24) | (pack_data[9] << 16) | (pack_data[10] << 8) | (pack_data[11]);
    std::cout << "Packfile Version: " << version << ", Object Count: " << object_count << std::endl;
    
    size_t pos = 12;
    for (uint32_t i = 0; i < object_count; i++) {
        if (pos >= pack_data.size()) {
            std::cerr << "Unexpected end of packfile while reading object " << i+1 << std::endl;
            break;
        }
        
        // Debug: capture raw header bytes.
        size_t header_start = pos;
        
        uint8_t byte = pack_data[pos++];
        int type = (byte >> 4) & 0x7; 
        uint64_t size = byte & 0x0F;  
        int shift = 4;
        while (byte & 0x80) {
            if (pos >= pack_data.size()) {
                std::cerr << "Unexpected end of packfile while decoding header for object " << i+1 << std::endl;
                return;
            }
            byte = pack_data[pos++];
            size |= (uint64_t)(byte & 0x7F) << shift;
            shift += 7;
        }
        
        // Debug print of raw header bytes.
        std::cout << "Object " << i+1 << " header raw bytes: ";
        for (size_t j = header_start; j < pos; j++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)pack_data[j] << " ";
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "Object " << i+1 << ": Parsed header - Type " << type << ", Uncompressed Size " << size << std::endl;
        
        size_t compressed_consumed = 0;
        std::vector<uint8_t> uncompressed_data;
        try {
            uncompressed_data = decompress_object(pack_data, pos, size, compressed_consumed);
        } catch (const std::exception &e) {
            std::cerr << "Object " << i+1 << ": Decompression failed: " << e.what() << std::endl;
            // Skip this object by attempting to move forward one byte (or implement more robust skipping).
            pos++;
            continue;
        }
        std::cout << "Object " << i+1 << ": Decompressed size: " << uncompressed_data.size()
                  << " bytes, Compressed bytes consumed: " << compressed_consumed << std::endl;
        pos += compressed_consumed;
        
        if (type == 6 || type == 7) {
            if (type == 7) {
                if (pos + 20 > pack_data.size()) {
                    std::cerr << "Packfile truncated: missing base SHA for REF_DELTA in object " << i+1 << std::endl;
                    continue;
                }
                std::string base_sha;
                for (int j = 0; j < 20; j++) {
                    char buf[3];
                    std::snprintf(buf, sizeof(buf), "%02x", pack_data[pos + j]);
                    base_sha.append(buf);
                }
                pos += 20;
                std::cout << "Object " << i+1 << ": REF_DELTA with base SHA " << base_sha << std::endl;
                
                std::vector<uint8_t> base_object;
                try {
                    base_object = get_base_object(base_sha);
                } catch (const std::exception &e) {
                    std::cerr << "Object " << i+1 << ": " << e.what() << std::endl;
                    continue;
                }
                
                try {
                    uncompressed_data = apply_delta(base_object, uncompressed_data);
                    std::cout << "Object " << i+1 << ": Delta applied, final size: " << uncompressed_data.size() << std::endl;
                } catch (const std::exception &e) {
                    std::cerr << "Object " << i+1 << ": Delta application failed: " << e.what() << std::endl;
                    continue;
                }
            } else {
                std::cerr << "Object " << i+1 << ": OFS_DELTA not implemented in this example." << std::endl;
                continue;
            }
            type = 3;
        }
        
        std::string final_type;
        switch (type) {
            case 1: final_type = "commit"; break;
            case 2: final_type = "tree"; break;
            case 3: final_type = "blob"; break;
            case 4: final_type = "tag"; break;
            default:
                std::cerr << "Object " << i+1 << ": Unexpected object type: " << type << std::endl;
                continue;
        }
        
        try {
            std::string stored_hash = store_object(final_type, uncompressed_data);
            std::cout << "Object " << i+1 << ": Stored to disk with hash: " << stored_hash << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "Object " << i+1 << ": Failed to store object: " << e.what() << std::endl;
        }
    }
    
    if (pack_data.size() >= 20) {
        std::cout << "Packfile checksum: ";
        for (size_t j = pack_data.size() - 20; j < pack_data.size(); j++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)pack_data[j] << " ";
        }
        std::cout << std::dec << std::endl;
    }
}

void clone_repo(const std::string& repo_url, const std::string& dest_dir) {
    std::filesystem::create_directories(dest_dir);
    const std::string comp_repo_url = repo_url + "/info/refs?service=git-upload-pack";
    std::string fetch_response = fetch_refs(comp_repo_url);
    if (fetch_response.empty()) {
        std::cerr << "Failed to fetch repository references\n";
        return;
    }
    std::cout << "fetch_response received from server:\n" << fetch_response << "\n";
    
    std::unordered_map<std::string, std::string> references = parse_fetch_response(fetch_response);
    for (const auto& i : references) {
        std::cout << "Branch Name: " << i.first << " SHA1: " << i.second << std::endl;
    }
    std::string post_request_endpoint = repo_url + "/git-upload-pack";
    std::cout << "POST Request endpoint: " << post_request_endpoint << std::endl;
    
    std::ostringstream post_request_content;
    std::string want_line = "want " + references["HEAD"] + " multi_ack_detailed side-band-64k\n";
    int want_line_length = want_line.length() + 4;
    post_request_content << std::setw(4) << std::setfill('0') << std::hex << want_line_length;
    post_request_content << std::dec << want_line;
    post_request_content << "0000";  // Flush packet
    post_request_content << "0009done\n";
    
    std::string post_data = post_request_content.str();
    std::cout << "Raw POST data (Hex): ";
    for (unsigned char c : post_data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)c << " ";
    }
    std::cout << std::dec << std::endl;
    std::cout << "Final POST body length: " << post_data.length() << std::endl;
    
    std::string post_request_response = send_post_request(post_request_endpoint, post_request_content.str());
    // For debugging, you might print the response here if it's not binary.
    // std::cout << "Post Request Response: " << post_request_response << std::endl;
    
    std::vector<uint8_t> packfile_data = extract_packfile_data(post_request_response);
    std::cout << "Extracted packfile data size: " << packfile_data.size() << " bytes" << std::endl;
    parse_packfile(packfile_data);
}
