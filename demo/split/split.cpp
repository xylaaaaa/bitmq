#include <iostream>
#include <string>
#include <vector>

size_t split(const std::string &str, const std::string &sep, std::vector<std::string> &result)
{
    size_t pos, idx = 0;
    while (idx < str.size())
    {
        pos = str.find(sep, idx);
        if (pos == std::string::npos)
        {
            result.push_back(str.substr(idx));
            return result.size();
        }
        if (pos == idx)
        {
            idx = pos + sep.size();
            continue;
        }
        result.push_back(str.substr(idx, pos - idx));
        idx = pos + sep.size();
    }
    return result.size();
}

int main()
{
    std::string str = "...news....music.#.pop...";
    std::vector<std::string> array;
    int n = split(str, ".", array);
    for (auto& e : array)
    {
        std::cout << e << std::endl; 
    }
    return 0;
}
