//
// Created by chijinxin on 19-4-29.
//
#include <iostream>

#include "net/PbRpcServer.h"
#include "protobufCoder/RpcMessage.pb.h"
#include "example/MyService.pb.h"

using namespace std;


/*
 * 实现MyService中的方法
 */
class MyServiceImpl : public example::rpcProto::MyService {
public:
    /*
     * 实现 MyService::Echo() 方法
     */
    void Echo(::google::protobuf::RpcController *controller, const ::example::rpcProto::EchoReq *request,
              ::example::rpcProto::EchoRes *response, ::google::protobuf::Closure *done) override {

        response->set_response( request->request() );

        cout<<"RPC Server Called MyService::Echo()"<<endl;

        if(done)
            done->Run();
    }

    /*
     * 实现 MyService::Add() 方法
     */
    void Add(::google::protobuf::RpcController *controller, const ::example::rpcProto::OperaReq *request,
             ::example::rpcProto::OperaRes *response, ::google::protobuf::Closure *done) override {

        response->set_c( request->a() + request->b() );

        cout<<"RPC Server Called MyService::Add()"<<endl;

        if(done)
            done->Run();
    }

    /*
     * 实现 MyService::Sub() 方法
     */
    void Sub(::google::protobuf::RpcController *controller, const ::example::rpcProto::OperaReq *request,
             ::example::rpcProto::OperaRes *response, ::google::protobuf::Closure *done) override {

        response->set_c( request->a() - request->b() );

        cout<<"RPC Server Called MyService::Sub()"<<endl;

        if(done)
            done->Run();
    }
};


int main()
{
    MyServiceImpl* myServiceImpl = new MyServiceImpl();

    pbRPCServer rpc_server;
    
    rpc_server.RegisterService(myServiceImpl);
    
    rpc_server.Start(8888);
}