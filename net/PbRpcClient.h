//
// Created by chijinxin on 19-4-28.
//

#ifndef RPC_PROTOBUF_CPP_PBRPCCLIENT_H
#define RPC_PROTOBUF_CPP_PBRPCCLIENT_H

#include <iostream>

#include <folly/init/Init.h>
#include <folly/hash/FarmHash.h>
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
    RpcMsgClientSerializePipeline::Ptr newPipeline(std::shared_ptr<AsyncTransportWrapper> socket) override;
};



/*
 * RPC Client
 */
class PbRpcClient {
public:
    PbRpcClient(std::string host, int port, int ioThreadNum = 0);
    PbRpcClient(std::string host, int port, std::shared_ptr<folly::IOThreadPoolExecutor> ioThreadPool);
    PbRpcClient(std::string host, int port, std::shared_ptr<folly::IOThreadPoolExecutor> ioThreadPool, uint64_t reqLimit, uint32_t t);
    ~PbRpcClient();

public:
    //远程rpc服务器地址
    SocketAddress remoteAddress;

    //本地tcp客户端
    ClientBootstrap<RpcMsgClientSerializePipeline> tcpClient;

    //客户端消息分发器
    std::shared_ptr<MultiplexClientDispatcher> dispatcher;

    //连接pipeline
    RpcMsgClientSerializePipeline* pipeline;

    //是否成功与远程rpc服务器建立连接
    std::atomic_bool connected;

private:
    /*异步连接远程rpc服务器*/
    void connect();
};

/*
 * RPC Channel
 */
class RpcChannel : public google::protobuf::RpcChannel {
public:
    /*构造函数*/
    RpcChannel();
    RpcChannel(PbRpcClient* client);
    RpcChannel(std::shared_ptr<PbRpcClient> client);
    ~RpcChannel();


    /*实现RpcChannel中的CallMethod方法*/
    void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request, google::protobuf::Message *response,
                    google::protobuf::Closure *done) override ;

private:
    //rpc客户端
    std::shared_ptr<PbRpcClient> rpcClient;

    //请求id
    std::atomic_long request_id;

    //互斥锁
    std::mutex connect_mu;
};



#endif //RPC_PROTOBUF_CPP_PBRPCCLIENT_H
