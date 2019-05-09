//
// Created by chijinxin on 19-5-7.
//
#include <iostream>
#include <folly/futures/Future.h>
#include <google/protobuf/message.h>
#include "net/PbRpcClient.h"
#include "example/MyService.pb.h"
#include "net/futureRpcCallProxy.h"

using namespace folly;
using namespace std;



int main()
{
    std::shared_ptr<example::rpcProto::OperaReq> req = std::make_shared<example::rpcProto::OperaReq>();
    example::rpcProto::OperaRes a;

    RpcChannel c;
    example::rpcProto::MyService_Stub myservice(&c);

    futureRpcCallProxy myServiceCallProxy(&myservice);

    myServiceCallProxy.addRemoteHost("127.0.0.1", 8888);

    req->set_a(100);
    req->set_b(200);
    a = myServiceCallProxy.CallMethod<example::rpcProto::OperaRes>("123", req).get();

    cout<< a.c() <<endl;

    this_thread::sleep_for(std::chrono::seconds(10));

    return 0;
}


