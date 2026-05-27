#pragma once
#include <string>
#include <sys/types.h>
#include <vector>

class StringUtils
{
public:
    static std::vector<std::string> split(const std::string& string, char separator)
    {
        std::vector<std::string> result;
        std::string current = "";
        ssize_t ptr = 0;
        while (ptr < string.size())
        {
            if (string[ptr] == separator)
            {
                if (current.size() > 0)
                {
                    result.push_back(current);
                    current = "";
                }
            } else {
                current += string[ptr];
            }
            ptr++;
        }
        if (current.size() > 0) {
            result.push_back(current);
        }
        return result;
    }
};