//
// Created by chijinxin on 19-5-10.
//

#ifndef RPC_PROTOBUF_CPP_STRINGUTIL_H
#define RPC_PROTOBUF_CPP_STRINGUTIL_H

#include <string>
#include <vector>

class StringUtil {
public:
    static std::vector<std::string> split(std::string str, std::string pattern);
    static void split(std::string src, std::vector<std::string> &result, std::string pattern);
};


#endif //RPC_PROTOBUF_CPP_STRINGUTIL_H
