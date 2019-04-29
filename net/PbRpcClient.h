//
// Created by chijinxin on 19-4-28.
//

#ifndef RPC_PROTOBUF_CPP_PBRPCCLIENT_H
#define RPC_PROTOBUF_CPP_PBRPCCLIENT_H

#include <iostream>

#include <folly/init/Init.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>

#include <wangle/service/Service.h>
#include <wangle/service/ClientDispatcher.h>
#include <wangle/service/ExecutorFilter.h>
#include <wangle/channel/AsyncSocketHandler.h>
#include <wangle/channel/EventBaseHandler.h>
#include <wangle/bootstrap/ClientBootstrap.h>

#include <google/protobuf/service.h>

#include "codec/LengthFieldPrepender.h"
#include "codec/LengthFieldBasedFrameDecoder.h"
#include "protobufCoder/RpcMessage.pb.h"
#include "protobufCoder/RpcMsgSerializeHandler.h"
#include "net/ServiceDispatcher/ClientDispatcher.h"

using namespace folly;
using namespace wangle;

using RpcMsgClientSerializePipeline = wangle::Pipeline<IOBufQueue&, rpc::codec::RpcMessage>;

/*
 * RPC Message pipeline工厂
 * pipeline数据流:
 *     上行： 异步Socket IO          IO事件循环           消息分帧(头部2byte的消息长度)      第一级RPC服务消息序列化             服务端服务消息分发器
 *           AsyncSocketHandler -> EventBaseHandler -> LengthFieldBasedFrameDecoder -> RpcMsgServerSerializeHandler -> MultiplexServerDispatcher
 *
 *     下行： 服务调用返回数据    第一级RPC服务消息序列化            消息头部添加长度           IO事件循环           异步Socket IO
 *           RpcMsgService -> RpcMsgServerSerializeHandler -> LengthFieldPrepender -> EventBaseHandler -> AsyncSocketHandler
 */
class RpcMsgPipelineFactory : public PipelineFactory<RpcMsgClientSerializePipeline> {
public:
    RpcMsgClientSerializePipeline::Ptr newPipeline(std::shared_ptr<AsyncTransportWrapper> sock) override
    {
        auto pipeline = RpcMsgClientSerializePipeline::create();
        pipeline->addBack(AsyncSocketHandler(sock));
        // ensure we can write from any thread
        pipeline->addBack(EventBaseHandler());
        /*
         * LengthFieldBasedFrameDecoder()数据上行编码：
         *       * lengthFieldOffset   = 0
         *       * lengthFieldLength   = 2
         *       * lengthAdjustment    = 0
         *       * initialBytesToStrip = 2 (= the length of the Length field)
         *
         *       * BEFORE DECODE (14 bytes)         AFTER DECODE (12 bytes)
         *       +--------+----------------+      +----------------+
         *       | Length | Actual Content |----->| Actual Content |
         *       | 0x000C | "HELLO, WORLD" |      | "HELLO, WORLD" |
         *       +--------+----------------+      +----------------+
         *
         *
         * LengthFieldPrepender()数据下行编码：
         *       * int lengthFieldLength = 4,
         *       * int lengthAdjustment = 0
         *
         *       * BEFORE DECODE (12 bytes)      AFTER DECODE (12 bytes)
         *         +----------------+            +--------+----------------+
         *         | Actual Content |    ----->  | Length | Actual Content |
         *         | "HELLO, WORLD" |            | 0x000C | "HELLO, WORLD" |
         *         +----------------+            +--------+----------------+
         */
        pipeline->addBack(LengthFieldBasedFrameDecoder(2, 65536, 0, 0, 2, true));
        pipeline->addBack(LengthFieldPrepender(2, 0, false, true));

        pipeline->addBack(RpcMsgSerializeHandler());

        pipeline->finalize();
        return pipeline;
    }
};


/*
 * RPC Client
 */
class PbRpcClient {
public:

    PbRpcClient(std::string host, int port, int ioThreadNum = 0)
        :remoteAddress(host, port),
         pipeline(nullptr),
         connected(false)
    {

        if(ioThreadNum != 0)
        {
            tcpClient.group(std::make_shared<folly::IOThreadPoolExecutor>(ioThreadNum));
        }
        //默认IOThreadPoolExecutor线程数为CPU核数


        tcpClient.pipelineFactory(std::make_shared<RpcMsgPipelineFactory>());

        dispatcher = std::make_shared<MultiplexClientDispatcher>();

        connect();
    }

public:
    std::atomic_long request_id;   //rpc请求id

    SocketAddress remoteAddress;   //远程rpc服务器地址

    ClientBootstrap<RpcMsgClientSerializePipeline> tcpClient; //本地tcp客户端

    std::shared_ptr<MultiplexClientDispatcher> dispatcher;    //客户端消息分发器

    RpcMsgClientSerializePipeline* pipeline;    //pipeline

    std::atomic_bool connected;        //是否成功与远程rpc服务器建立连接

    std::shared_ptr<folly::CPUThreadPoolExecutor> threadPool;

private:
    void connect()
    {
        std::cout<<remoteAddress<<std::endl;
        pipeline = tcpClient.connect(remoteAddress).get();
        std::cout<<"connected to the rpc server!"<<std::endl;
        connected = true;
        dispatcher->setPipeline(pipeline);
//        tcpClient.connect(remoteAddress)
//                .thenValue(
//                        [this](RpcMsgClientSerializePipeline* p)
//                        {
//                            this->pipeline = p;
//                            connected = true;
//                            this->dispatcher->setPipeline(pipeline);
//                            std::cout<<"connected to the rpc server!"<<std::endl;
//                        })
//                .thenError(folly::tag_t<std::exception>{},
//                           [this](const std::exception& e)
//                           {
//                               std::cerr<<"[RpcClient] Connect error: "<< exceptionStr(e);
//                               this->pipeline = nullptr;
//                           });
    }

};

/*
 * RPC Channel
 */
class RpcChannel : public google::protobuf::RpcChannel {
public:
    /*构造函数*/
    RpcChannel(PbRpcClient* client) : rpcClient(client), request_id(0)
    {}
    RpcChannel(std::shared_ptr<PbRpcClient> client) : rpcClient(client), request_id(0)
    {}


    /*实现RpcChannel中的CallMethod方法*/
    void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request, google::protobuf::Message *response,
                    google::protobuf::Closure *done) override {

        if(!rpcClient->connected)
        {
            std::unique_lock<std::mutex>(connect_mutex);
            rpcClient->pipeline = rpcClient->tcpClient.connect(rpcClient->remoteAddress).get();
            rpcClient->dispatcher->setPipeline(rpcClient->pipeline);
            rpcClient->connected = false;
        }

        uint32_t serviceId = 1;

        rpc::codec::RpcMessage Req;
        Req.set_type( rpc::codec::REQUEST );
        Req.set_id( request_id++ );
        Req.set_serviceid( serviceId );
        Req.set_methodid( method->index() );

        std::string request_str;
        request->SerializeToString(&request_str);
        Req.set_request( std::move(request_str) );

        /*同步调用获取结果*/
        if(!done)
        {
            rpc::codec::RpcMessage Res = (*(rpcClient->dispatcher))(Req).get();
            response->ParseFromString( Res.response() );
        }
            /*设置异步回调*/
        else
        {
            //cout<<"Req id = "<<Req.id()<<endl;
            (*(rpcClient->dispatcher))(Req)
                    .thenValue(
                        [Req, response, done](rpc::codec::RpcMessage Res)
                        {
                            //cout<<"Res id = "<<Res.id()<<endl;
                            response->ParseFromString( Res.response() );
                            done->Run();
                            return Res;
                        });
//                    .thenError(folly::tag_t<std::exception>{},
//                        [](const std::exception& e)
//                        {
//                            std::cerr<<"[RpcCLient Error]"<<exceptionStr(e)<<std::endl;
//                        });
        }
    }


private:
    std::shared_ptr<PbRpcClient> rpcClient;
    std::mutex connect_mutex;
    std::atomic_long request_id;
};



#endif //RPC_PROTOBUF_CPP_PBRPCCLIENT_H
