//
// Created by chijinxin on 19-5-10.
//

#ifndef RPC_PROTOBUF_CPP_LINUXNETWORKUTIL_H
#define RPC_PROTOBUF_CPP_LINUXNETWORKUTIL_H

#include <string>

class LinuxNetworkUtil {
public:
    static bool GetIP(std::string net_name, std::string &strIP);
    static bool GetMAC(std::string net_name, std::string &strMAC);
};

#endif //RPC_PROTOBUF_CPP_LINUXNETWORKUTIL_H
