//
// Created by chijinxin on 19-4-29.
//
#include <iostream>

#include "net/PbRpcClient.h"
#include "protobufCoder/RpcMessage.pb.h"
#include "example/MyService.pb.h"

using namespace std;
using namespace example::rpcProto;



int main()
{
    PbRpcClient rpcClient("127.0.0.1", 8888, 4);

    RpcChannel channel(&rpcClient);

    MyService_Stub rpc_ctl(&channel);

    EchoReq req;
    req.set_request("chijinxin");

    EchoRes res;

    rpc_ctl.Echo(NULL, &req, &res, NULL);
    cout<<res.response()<<endl;

    while(1){}

    return 0;
}