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
        RANDOM,        //负载均衡随机策略
        HASH,        //负载均衡Hash策略
        RoundRobin   //负载均衡轮询策略
    };

    /*构造函数*/
    futureRpcCallProxy(google::protobuf::Service* s, int ioThreadNum = 0);

    /*设置负载均衡策略*/
    void setLBStrategy(LBStrategy l);

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
        //std::cout << "service id = "<< serviceId << " , ";
        //std::cout << "method id = "<< methodId << std::endl;

        rpc::codec::RpcMessage Req;

        long curReqId = request_id++;

        Req.set_type( rpc::codec::REQUEST );
        Req.set_id( curReqId );
        Req.set_serviceid( serviceId );
        Req.set_methodid( methodId );

        std::string request_str;
        request->SerializeToString(&request_str);
        Req.set_request( std::move(request_str) );

        std::shared_ptr<PbRpcClient> rpcClient;

        //负载均衡轮询策略
        if(lbs == RoundRobin)
        {
            rpcClient = RoundRobin_Select(curReqId);
        }
        //负载均衡随机策略
        else if(lbs == RANDOM)
        {
            rpcClient = Random_Select(curReqId);
        }
        //负载均衡Hash策略
        else if(lbs == HASH)
        {
            rpcClient = Hash_Select();
        }

        //判断客户端是否与远程RPC服务器建立了连接
        //如果没有连接 则阻塞地与远程服务器进行连接
        if( !rpcClient->connected.load() )
        {
            std::unique_lock<std::mutex> lock(connect_mu);

            auto p = rpcClient->tcpClient.connect(rpcClient->remoteAddress).get();
            rpcClient->pipeline = p;
            rpcClient->connected = true;
            rpcClient->dispatcher->setPipeline(rpcClient->pipeline);
        }

        //向远程RPC服务器发起异步RPC调用
        (*(rpcClient->dispatcher))(Req)
                    .thenValue(
                     //成功调用 返回响应结果
                        [promise](rpc::codec::RpcMessage Res)
                        {
                            T response;
                            response.ParseFromString( Res.response() );
                            promise->setValue(std::move(response));
                        })
                    //远程调用发生异常
                    .thenError(folly::tag_t<std::exception>{},
                         [promise](const std::exception& e)
                         {
                             promise->setException(e);
                         });

        //返回Future
        return promise->getFuture();
    }

private:
    /*负载均衡轮询策略*/
    std::shared_ptr<PbRpcClient> RoundRobin_Select(long curReqId);  //轮询策略
    std::shared_ptr<PbRpcClient> Random_Select(long curReqId);
    std::shared_ptr<PbRpcClient> Hash_Select();

private:
    /*服务*/
    google::protobuf::Service* service;

    /*IO线程池*/
    std::shared_ptr<folly::IOThreadPoolExecutor> ioThreadPool;

    /*可用RPC服务器列表*/
    std::vector< std::shared_ptr<PbRpcClient> > vRpcClient;

    /*请求ID*/
    std::atomic_long request_id;

    /*更新可用RPC服务器 互斥锁*/
    std::mutex update_mu;

    /*连接远程RPC服务器 互斥锁*/
    std::mutex connect_mu;

    /*负载均衡策略*/
    LBStrategy lbs;
};


#endif //RPC_PROTOBUF_CPP_FUTURERPCCALLPROXY_H
