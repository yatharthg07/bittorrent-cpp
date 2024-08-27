#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <openssl/sha.h>

#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

json decode_bencoded_value(const std::string& encoded_value, size_t& pos);

json decode_bencoded_integer(const std::string& encoded_value, size_t& pos) {
    pos++; // Skip the 'i'
    size_t end = encoded_value.find('e', pos);
    if (end == std::string::npos) {
        throw std::runtime_error("Invalid bencoded integer: " + encoded_value);
    }
    int64_t number = std::atoll(encoded_value.substr(pos, end - pos).c_str());
    pos = end + 1; // Move past 'e'
    return json(number);
}

json decode_bencoded_string(const std::string& encoded_value, size_t& pos) {
    size_t colon_index = encoded_value.find(':', pos);
    if (colon_index == std::string::npos) {
        throw std::runtime_error("Invalid bencoded string: " + encoded_value);
    }
    size_t length = std::atoll(encoded_value.substr(pos, colon_index - pos).c_str());
    pos = colon_index + 1 + length;
    return json(encoded_value.substr(colon_index + 1, length));
}

json decode_bencoded_list(const std::string& encoded_value, size_t& pos) {
    pos++; // Skip the 'l'
    json list = json::array();
    while (encoded_value[pos] != 'e') {
        list.push_back(decode_bencoded_value(encoded_value, pos));
    }
    pos++; // Move past 'e'
    return list;
}

json decode_bencoded_dict(const std::string& encoded_value, size_t& pos) {
    pos++; // Skip the 'd'
    json dict = json::object();
    while (encoded_value[pos] != 'e') {
        json key = decode_bencoded_string(encoded_value, pos);
        json value = decode_bencoded_value(encoded_value, pos);
        dict[key.get<std::string>()] = value;
    }
    pos++; // Move past 'e'
    return dict;
}

json decode_bencoded_value(const std::string& encoded_value, size_t& pos) {
    if (std::isdigit(encoded_value[pos])) {
        return decode_bencoded_string(encoded_value, pos);
    } else if (encoded_value[pos] == 'i') {
        return decode_bencoded_integer(encoded_value, pos);
    } else if (encoded_value[pos] == 'l') {
        return decode_bencoded_list(encoded_value, pos);
    } else if (encoded_value[pos] == 'd') {
        return decode_bencoded_dict(encoded_value, pos);
    } else {
        throw std::runtime_error("Unhandled encoded value: " + encoded_value);
    }
}

std::string read_file_as_string(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    std::stringstream buffer;
    if (file) {
        buffer << file.rdbuf();
        file.close();
        return buffer.str();
    } else {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
}

std::string json_to_bencode(const json& j) {
    std::ostringstream os;
    if (j.is_object()) {
        os << 'd';
        for (auto& el : j.items()) {
            os << el.key().size() << ':' << el.key() << json_to_bencode(el.value());
        }
        os << 'e';
    } else if (j.is_array()) {
        os << 'l';
        for (const json& item : j) {
            os << json_to_bencode(item);
        }
        os << 'e';
    } else if (j.is_number_integer()) {
        os << 'i' << j.get<int64_t>() << 'e';
    } else if (j.is_string()) {
        const std::string& value = j.get<std::string>();
        os << value.size() << ':' << value;
    }
    return os.str();
}

std::string calculate_sha1_hash(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

void extract_torrent_info(const json& torrent_data) {
    if (torrent_data.contains("announce")) {
        std::cout << "Tracker URL: " << torrent_data["announce"].get<std::string>() << std::endl;
    } else {
        std::cerr << "Error: 'announce' field is missing in the torrent file." << std::endl;
    }

    if (torrent_data.contains("info")) {
        const json& info = torrent_data["info"];
        if (info.contains("length")) {
            std::cout << "Length: " << info["length"].get<int64_t>() << std::endl;
        } else {
            std::cerr << "Error: 'length' field is missing in the 'info' dictionary." << std::endl;
        }

        // Re-bencode the info dictionary
        std::string bencoded_info = json_to_bencode(info);

        // Calculate the SHA-1 hash of the bencoded info dictionary
        std::string info_hash = calculate_sha1_hash(bencoded_info);

        std::cout << "Info Hash: " << info_hash << std::endl;

        // Extract and print piece length and piece hashes
        if (info.contains("piece length")) {
            int64_t piece_length = info["piece length"].get<int64_t>();
            std::cout << "Piece Length: " << piece_length << std::endl;
        } else {
            std::cerr << "Error: 'piece length' field is missing in the 'info' dictionary." << std::endl;
        }

        if (info.contains("pieces")) {
            std::string pieces = info["pieces"].get<std::string>();
            std::cout << "Piece Hashes:" << std::endl;

            for (size_t i = 0; i < pieces.size(); i += 20) {
                std::string piece_hash = pieces.substr(i, 20);
                std::stringstream ss;
                for (char c : piece_hash) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << (static_cast<int>(c) & 0xff);
                }
                std::cout << ss.str() << std::endl;
            }
        } else {
            std::cerr << "Error: 'pieces' field is missing in the 'info' dictionary." << std::endl;
        }

    } else {
        std::cerr << "Error: 'info' dictionary is missing in the torrent file." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " info <torrent_file>" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    std::string torrent_file_path = argv[2];

    if (command == "info") {
        try {
            std::string torrent_content = read_file_as_string(torrent_file_path);
            size_t pos = 0;
            json torrent_data = decode_bencoded_value(torrent_content, pos);
            extract_torrent_info(torrent_data);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
  
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
