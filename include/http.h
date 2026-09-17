#pragma once

#include "outcome.h"
#include <string>

namespace http {

struct Response {
    bool succeeded = false;
    int status = 0;
    std::string body;
    std::string userMessage;
    std::string detail;
};

bool isRemoteUrl(const std::string& url);
Response get(const std::string& url, size_t maxBytes = 8 * 1024 * 1024);

}
