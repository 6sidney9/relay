#pragma once

#include <string>
#include <string_view>

namespace relay::http {

std::string guess_content_type(std::string_view path);

}  // namespace relay::http
