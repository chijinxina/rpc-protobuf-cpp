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
    MyServiceImpl():callCount(0L){}


    /* 实现 MyService::Echo() 方法 */
    void Echo(::google::protobuf::RpcController *controller, const ::example::rpcProto::EchoReq *request,
              ::example::rpcProto::EchoRes *response, ::google::protobuf::Closure *done) override {

        response->set_response( request->request() );

        cout<<"RPC Server Called MyService::Echo() "<< callCount++ <<endl;

        if(done)
            done->Run();
    }


    /* 实现 MyService::Add() 方法 */
    void Add(::google::protobuf::RpcController *controller, const ::example::rpcProto::OperaReq *request,
             ::example::rpcProto::OperaRes *response, ::google::protobuf::Closure *done) override {

        response->set_c( request->a() + request->b() );

        cout<<"RPC Server Called MyService::Add()  "<< callCount++ <<endl;

        if(done)
            done->Run();
    }


    /* 实现 MyService::Sub() 方法 */
    void Sub(::google::protobuf::RpcController *controller, const ::example::rpcProto::OperaReq *request,
             ::example::rpcProto::OperaRes *response, ::google::protobuf::Closure *done) override {

        response->set_c( request->a() - request->b() );

        cout<<"RPC Server Called MyService::Sub()  "<< callCount++ <<endl;

        if(done)
            done->Run();
    }

private:
    //服务调用次数计数
    std::atomic_long callCount;
};


/*
 * 实现Service2中的方法
 */
class Service2Impl : public example::rpcProto::Service2 {
public:

    /* 实现 Service2::Mul() 方法 */
    void Mul(::google::protobuf::RpcController *controller, const ::example::rpcProto::OperaReqF *request,
             ::example::rpcProto::OperaResF *response, ::google::protobuf::Closure *done) override {

        response->set_c( request->a() * request->b() );

        cout<<"RPC Server Called Service2::Mul()   "<< callCount++ <<endl;

        if(done)
            done->Run();
    }

    /* 实现 Service2::Div() 方法 */
    void Div(::google::protobuf::RpcController *controller, const ::example::rpcProto::OperaReqF *request,
             ::example::rpcProto::OperaResF *response, ::google::protobuf::Closure *done) override {

        response->set_c( request->a() / request->b() );

        cout<<"RPC Server Called Service2::Div()   "<< callCount++ <<endl;

        if(done)
            done->Run();
    }

private:
    //服务调用次数计数
    std::atomic_long callCount;
};

int main()
{
    MyServiceImpl* myServiceImpl = new MyServiceImpl();
    Service2Impl*  service2Impl  = new Service2Impl();

    pbRPCServer rpc_server;

    //RPC绑定服务
    rpc_server.RegisterService(myServiceImpl);
    rpc_server.RegisterService(service2Impl);

    //启动RPC服务器 监听8888端口
    rpc_server.Start(8888);

    std::cout<<"RPC Server Stop!"<<std::endl;
}