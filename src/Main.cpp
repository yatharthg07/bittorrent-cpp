#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <map>

#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

json decode_bencoded_value(const std::string& encoded_value, size_t& pos);

json decode_bencoded_integer(const std::string& encoded_value, size_t& pos) {
    pos++; 
    size_t end = encoded_value.find('e', pos);
    if (end == std::string::npos) {
        throw std::runtime_error("Invalid bencoded integer: " + encoded_value);
    }
    int64_t number = std::atoll(encoded_value.substr(pos, end - pos).c_str());
    pos = end + 1;
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
    pos++; 
    json list = json::array();
    while (encoded_value[pos] != 'e') {
        list.push_back(decode_bencoded_value(encoded_value, pos));
    }
    pos++; 
    return list;
}

json decode_bencoded_dict(const std::string& encoded_value, size_t& pos) {
    pos++; 
    json dict = json::object();
    while (encoded_value[pos] != 'e') {
        json key = decode_bencoded_string(encoded_value, pos);
        json value = decode_bencoded_value(encoded_value, pos);
        dict[key.get<std::string>()] = value;
    }
    pos++; 
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

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
        return 1;
    }

    std::string command = argv[1];

    if (command == "decode") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
            return 1;
        }
        std::string encoded_value = argv[2];
        size_t pos = 0;
        json decoded_value = decode_bencoded_value(encoded_value, pos);
        std::cout << decoded_value.dump() << std::endl;
    } else {
        std::cerr << "unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
