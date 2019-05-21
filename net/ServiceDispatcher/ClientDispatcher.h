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
    /*构造函数*/
    MultiplexClientDispatcher(uint64_t reqLimit = INT64_MAX, uint32_t t = 5000)
            : reqlimit(reqLimit), //默认最大请求等待队列长度为无穷大
            timeout(t)            //默认请求超时时间为5s
    {}

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

        //pending requests numbers limit process
        if( requests_.size() >= reqlimit )
        {
            rpc::codec::RpcMessage Res;
            Res.set_type(rpc::codec::RESPONSE);
            Res.set_id( arg.id() );
            Res.set_error(rpc::codec::PENDDING_LIMIT);
            return std::move(Res);
        }

        //将请求凭证保存至
        auto ret = requests_.insert((int64_t)arg.id(), p);
        //请求凭证无法成功保存
        if(!ret.second)
        {
            rpc::codec::RpcMessage Res;
            Res.set_type(rpc::codec::RESPONSE);
            Res.set_id( arg.id() );
            Res.set_error(rpc::codec::INVALID_REQUEST);
            return std::move(Res);
        }

        /*响应异常 删除hashmap中相应的请求*/
//        p->setInterruptHandler(
//                [arg, this](const folly::exception_wrapper&)
//                {
//                    std::cerr << "[Rpc Called Error Interrupt]" <<std::endl;
//                    this->requests_.erase( (int64_t)arg.id() );
//                });

        this->pipeline_->write(arg);

        /*
         * 设置RPC请求超时时间 超时删除当前请求凭证并返回超时异常
         * 此处设置超时很有必要
         * 因为在异步RPC调用过程中, 需要存储请求凭证与响应之间的对应关系
         * 我们利用folly::ConcurrentHashMap存储了等待响应返回的RPC请求
         * 如果发生某一请求对应的响应由于某种异常情况没有返回,
         * 那么ConcurrentHashMap存储的请求凭证将无法删除,
         * 如果这种情况一直发生, 那么ConcurrentHashMap中存储的
         * 无效数据将一直增加, 占用内存, 发生内存泄露
         * 所以此处将发出的请求设置一个超时时间, 在规定时间内
         * 请求对应的响应没有返回, 则在ConcurrentHashMap中删除该请求
         */
        return p->getFuture().onTimeout(std::chrono::milliseconds(timeout),
                           [arg, this]()
                           {
                                std::cout<<"rpc call timeout"<<std::endl;
                                //删除异步请求凭证
                                this->requests_.erase( arg.id() );
                                //设置响应包体
                                rpc::codec::RpcMessage Res;
                                Res.set_type(rpc::codec::RESPONSE);
                                Res.set_id( arg.id() );
                                Res.set_error(rpc::codec::TIMEOUT);
                                //返回超时响应
                                return Res;
                           });
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

    /*RPC请求超时时间 单位ms*/
    uint32_t timeout;

    /*RPC调用等待队列最大长度*/
    uint64_t reqlimit;
};

#endif //RPC_PROTOBUF_CPP_CLIENTDISPATCHER_H
