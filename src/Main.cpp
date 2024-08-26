#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>

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
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + file_path);
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
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
