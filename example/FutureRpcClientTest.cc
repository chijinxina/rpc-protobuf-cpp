//
// Created by chijinxin on 19-5-8.
//

#include <iostream>
#include <google/protobuf/message.h>
#include "net/PbRpcClient.h"
#include "net/futureRpcCallProxy.h"

#include "example/MyService.pb.h"

using namespace std;
using namespace folly;
using namespace example::rpcProto;


int main()
{
    RpcChannel c;   //无用  google::protobuf::service接口要求

    /*
     * service MyService
     * {
     *      rpc Echo(EchoReq) returns(EchoRes);
     *      rpc Add(OperaReq) returns(OperaRes);
     *      rpc Sub(OperaReq) returns(OperaRes);
     * }
     */
    MyService_Stub rpc_myservice(&c);

    /*
     * service Service2
     * {
     *      rpc Mul(OperaReqF) returns(OperaResF);
     *      rpc Div(OperaReqF) returns(OperaResF);
     * }
     */
    Service2_Stub rpc_service2(&c);

    futureRpcCallProxy myServiceCallProxy(&rpc_myservice);
    futureRpcCallProxy service2CallProxy(&rpc_service2);

    //添加可用RPC服务器
    myServiceCallProxy.addRemoteHost("127.0.0.1", 8888);

    service2CallProxy.addRemoteHost("127.0.0.1", 8888);

    myServiceCallProxy.addRemoteHost("127.0.0.1", 9999);

    service2CallProxy.addRemoteHost("127.0.0.1", 9999);

    int i = 0;
    while(i < 10000000)
    {
        //1. Test MyService::Echo()
        if(i%5 == 0)
        {
            std::shared_ptr<EchoReq> req = std::make_shared<EchoReq>();

            ostringstream ss;
            ss<<" --- "<<i<<" --- ";
            req->set_request(ss.str());

            myServiceCallProxy.CallMethod<EchoRes>("Echo", req)
                    .thenValue(
                            [req](EchoRes res)
                            {
                                cout<<"AsyncFuture call MyService::Echo() req: "<<req->request()<<"| res: "<<res.response()<<endl;
                            });
        }
        //2. Test MyService::Add()
        else if(i%5 == 1)
        {
            std::shared_ptr<OperaReq> req = std::make_shared<OperaReq>();

            req->set_a(i);
            req->set_b(i);

            myServiceCallProxy.CallMethod<OperaRes>("Add", req)
                    .thenValue(
                            [req](OperaRes res)
                            {
                                cout<<"AsyncFuture call MyService::Add()  req: ";
                                cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a+b="<<res.c()<<endl;
                            });
        }
        //3. Test MyService::Sub()
        else if(i%5 == 2)
        {
            std::shared_ptr<OperaReq> req = std::make_shared<OperaReq>();

            req->set_a(i+i);
            req->set_b(i);

            myServiceCallProxy.CallMethod<OperaRes>("Sub", req)
                    .thenValue(
                            [req](OperaRes res)
                            {
                                cout<<"AsyncFuture call MyService::Sub()  req: ";
                                cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a-b="<<res.c()<<endl;
                            });
        }
        //4. Test Service2::Mul()
        else if(i%5 == 3)
        {
            std::shared_ptr<OperaReqF> req = std::make_shared<OperaReqF>();

            req->set_a(i);
            req->set_b(i);

            service2CallProxy.CallMethod<OperaResF>("Mul", req)
                    .thenValue(
                            [req](OperaResF res)
                            {
                                cout<<"AsyncFuture call Service2::Mul()   req: ";
                                cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a*b="<<res.c()<<endl;
                            });
        }
        //5. Test Service2::Div()
        else
        {
            std::shared_ptr<OperaReqF> req = std::make_shared<OperaReqF>();

            req->set_a(i+i/10);
            req->set_b(i);

            service2CallProxy.CallMethod<OperaResF>("Div", req)
                    .thenValue(
                            [req](OperaResF res)
                            {
                                cout<<"AsyncFuture call Service2::Div()   req: ";
                                cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a/b="<<res.c()<<endl;
                            });
        }

        i++;
    }

    this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}