//
// Created by chijinxin on 19-5-7.
//

#ifndef RPC_PROTOBUF_CPP_FUTURERPCCALLPROXY_H
#define RPC_PROTOBUF_CPP_FUTURERPCCALLPROXY_H

#include <string>
#include <memory>
#include "net/PbRpcClient.h"

class futureRpcCallProxy {
public:
    typedef std::shared_ptr<google::protobuf::Message> MessagePtr;

    enum LBStrategy {
        RAND,        //负载均衡随机策略
        HASH,        //负载均衡Hash策略
        RoundRobin   //负载均衡轮询策略
    };

    /*构造函数*/
    futureRpcCallProxy(google::protobuf::Service* s, int ioThreadNum = 0);

    /*设置负载均衡策略*/
    void setLBStrategy();

    /*添加RPC远程主机*/
    void addRemoteHost(std::string host, int port);

    /*异步服务方法调用*/
    template < typename  T>
    folly::Future<T> CallMethod(std::string method_name, MessagePtr request)
    {
        auto promise = std::make_shared< folly::Promise<T> >();

        const google::protobuf::ServiceDescriptor* pSvcDes = service->GetDescriptor();
        const google::protobuf::MethodDescriptor*  pMetDes = pSvcDes->FindMethodByName(method_name);
        //找不到对应方法
        if(!pMetDes)
        {
            std::cerr << "Cannot find Method: "<< method_name << " in Service: "<<pSvcDes->full_name()<<std::endl;
            promise->setException(folly::make_exception_wrapper<std::logic_error>(" Cannot find Method"));
            return promise->getFuture();
        }

        if(vRpcClient.size() <= 0)
        {
            std::cerr << "Available hosts lists is empty!" << std::endl;
            promise->setException(folly::make_exception_wrapper<std::logic_error>("Available hosts lists is empty!"));
            return promise->getFuture();
        }

        std::string serviceName = pSvcDes->full_name();

        uint32_t serviceId = folly::hash::farmhash::Hash32(serviceName.c_str(), serviceName.length());
        uint32_t methodId = pMetDes->index();
        std::cout << "service id = "<< serviceId << " , ";
        std::cout << "method id = "<< methodId << std::endl;

        rpc::codec::RpcMessage Req;

        Req.set_type( rpc::codec::REQUEST );
        Req.set_id( request_id++ );
        Req.set_serviceid( serviceId );
        Req.set_methodid( methodId );

        std::string request_str;
        request->SerializeToString(&request_str);
        Req.set_request( std::move(request_str) );

        (*(vRpcClient[0]->dispatcher))(Req)
                    .thenValue(
                        [promise](rpc::codec::RpcMessage Res)
                        {
                            T response;
                            response.ParseFromString( Res.response() );
                            promise->setValue(std::move(response));
                        });

        return promise->getFuture();
    }

private:
    /*负载均衡轮询策略*/
    void RoundRobinRun();

private:
    google::protobuf::Service* service;  //服务

    std::shared_ptr<folly::IOThreadPoolExecutor> ioThreadPool;

    std::vector< std::shared_ptr<PbRpcClient> > vRpcClient;
    std::atomic_long request_id; //请求ID
    std::mutex update_mu;
    std::mutex connect_mu;
};


#endif //RPC_PROTOBUF_CPP_FUTURERPCCALLPROXY_H
