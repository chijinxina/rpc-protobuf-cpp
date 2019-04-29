//
// Created by chijinxin on 19-4-28.
//

#ifndef RPC_PROTOBUF_CPP_CLIENTDISPATCHER_H
#define RPC_PROTOBUF_CPP_CLIENTDISPATCHER_H

#include <folly/concurrency/ConcurrentHashMap.h>

#include <wangle/service/Service.h>
#include <wangle/channel/Handler.h>
#include <wangle/service/ClientDispatcher.h>

#include "protobufCoder/RpcMessage.pb.h"

using namespace folly;
using namespace wangle;

using RpcMsgClientSerializePipeline = wangle::Pipeline<IOBufQueue&, rpc::codec::RpcMessage>;

/*
 * 客户端服务消息分发器
 * 类HTTP2.0
 * 利用hashmap保存request_id和响应凭证之间的对应关系从而允许响应乱序到达
 */
class MultiplexClientDispatcher
        : public ClientDispatcherBase< RpcMsgClientSerializePipeline,   //pipeline
                rpc::codec::RpcMessage,                                 //Request
                rpc::codec::RpcMessage >                                //Response
{
public:
    typedef folly::Promise<rpc::codec::RpcMessage> ReqPromise;          //异步请求凭证
    typedef std::shared_ptr< folly::Promise<rpc::codec::RpcMessage> > ReqPromisePtr;


public:
    /*从下游读取数据*/
    void read(Context*, rpc::codec::RpcMessage in) override
    {
        //std::cout<<"Recv Response Id = "<<in.id()<<std::endl;
        auto search = requests_.find( (int64_t)in.id() );

        //CHECK(search != requests_.end());
        //response找不到对应的request_id,丢弃response
        if(search == requests_.end())
        {
            return;
        }
        ReqPromisePtr p = search->second;
        requests_.erase(search);
        p->setValue(in);
    }

    /*远程主机关闭了连接*/
    void readEOF(Context *ctx) override
    {
        std::cout << "Connection closed by remote host" << std::endl;
        pipeline_->close();
    }

    /*读取数据发生异常*/
    void readException(Context *ctx, folly::exception_wrapper e) override
    {
        std::cout<<"Remote error: " << exceptionStr(e) << std::endl;
        Handler::readException(ctx, e);
    }

    /*客户端RPC服务调用*/
    Future<rpc::codec::RpcMessage> operator()(rpc::codec::RpcMessage arg) override
    {
        ReqPromisePtr p = std::make_shared<ReqPromise>();
        auto f = p->getFuture();     //获取异步请求凭证对应的Future
        requests_.insert((int64_t)arg.id(), p);

        /*响应异常 删除hashmap中相应的请求*/
        p->setInterruptHandler(
                [arg, this](const folly::exception_wrapper&)
                {
                    std::cerr << "[Rpc Called Error Interrupt]" <<std::endl;
                    this->requests_.erase((int64_t)arg.id());
                });

        this->pipeline_->write(arg);
        return f;
    }

    //pipeline关闭时打印信息
    Future<Unit> close() override
    {
        printf("Channel closed [1]\n");
        return ClientDispatcherBase::close();
    }
    Future<Unit> close(Context* ctx) override
    {
        printf("Channel closed [2]\n");
        return ClientDispatcherBase::close(ctx);
    }

private:
    /*对于每个RPC请求 利用hashmap记录request_id与响应凭证之间的对应关系 从而实现请求-响应的乱序到达 类似HTTP2.0*/
    //std::unordered_map< int64_t, folly::Promise<rpc::codec::RpcMessage> > requests_;
    folly::ConcurrentHashMap< int64_t, std::shared_ptr<ReqPromise> > requests_;
};

#endif //RPC_PROTOBUF_CPP_CLIENTDISPATCHER_H
