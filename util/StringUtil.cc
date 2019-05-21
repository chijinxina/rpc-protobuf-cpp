//
// Created by chijinxin on 19-5-10.
//

#include "StringUtil.h"

std::vector<std::string> StringUtil::split(std::string str, std::string pattern)
{
    std::string::size_type pos;

    std::vector<std::string> result;

    str += pattern;

    int size = str.size();

    for(int i=0; i<size; i++)
    {

        pos = str.find(pattern, i);

        if(pos < size)
        {
            std::string s = str.substr(i, pos - i);
            result.push_back(s);
            i = pos + pattern.size() - 1;
        }
    }

    return result;
}

void StringUtil::split(std::string src, std::vector<std::string> &result, std::string pattern) 
{
    std::string::size_type pos;
    
    src += pattern;

    int size = src.size();

    for(int i=0; i<size; i++)
    {

        pos = src.find(pattern, i);

        if(pos < size)
        {
            std::string s = src.substr(i, pos - i);
            result.push_back(s);
            i = pos + pattern.size() - 1;
        }
    }
}
