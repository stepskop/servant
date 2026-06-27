#include "Status.hpp"
#include <string>

std::string get_status_string(size_t status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        default: return "Unknown";
    }
}
