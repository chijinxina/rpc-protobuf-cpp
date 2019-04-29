//
// Created by chijinxin on 19-4-28.
//

#ifndef RPC_PROTOBUF_CPP_PBRPCSERVER_H
#define RPC_PROTOBUF_CPP_PBRPCSERVER_H

#include <iostream>

#include <folly/init/Init.h>
#include <folly/hash/FarmHash.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>

#include <wangle/service/Service.h>
#include <wangle/service/ExecutorFilter.h>
#include <wangle/channel/AsyncSocketHandler.h>
#include <wangle/channel/EventBaseHandler.h>
#include <wangle/bootstrap/ServerBootstrap.h>

#include <google/protobuf/service.h>


#include "codec/LengthFieldPrepender.h"
#include "codec/LengthFieldBasedFrameDecoder.h"
#include "protobufCoder/RpcMessage.pb.h"
#include "protobufCoder/RpcMsgSerializeHandler.h"
#include "net/ServiceDispatcher/ServerDispatcher.h"

using namespace folly;
using namespace wangle;

//RpcMsg Server端序列化工作流
using RpcMsgServerSerializePipeline = wangle::Pipeline<IOBufQueue&, rpc::codec::RpcMessage>;

typedef std::function<void (rpc::codec::RpcMessage, rpc::codec::RpcMessage&)> HandleRpcCallFunc;

/*
 *  RpcMsgService
 *  定义protobuf消息Service
 * 实现()方法进行第一级RPC调用
 * (第一级RPC调用进行服务的消息分发 第二级RPC调用进行方法的消息分发)
 */
class RpcMsgService : public Service<rpc::codec::RpcMessage, rpc::codec::RpcMessage> {
public:
    /*RpcMsgService构造函数*/
    RpcMsgService(HandleRpcCallFunc arg) : _handleRpcCall(arg)
    {}

    /*RpcMsgService调用流程*/
    Future<rpc::codec::RpcMessage> operator()(rpc::codec::RpcMessage request) override
    {
        //printf("RpcService Request: \n");
        //printf("------- request.request = %s \n",request.request().c_str());
        //printf("------- request.response = %s \n",request.response().c_str());

        rpc::codec::RpcMessage response;

        _handleRpcCall(request, response);

        return response;
    }

private:
    HandleRpcCallFunc _handleRpcCall;   //RPC call function
};


/*
 * RPC Message pipeline工厂
 * pipeline数据流:
 *     上行： 异步Socket IO          IO事件循环           消息分帧(头部2byte的消息长度)      第一级RPC服务消息序列化             服务端服务消息分发器
 *           AsyncSocketHandler -> EventBaseHandler -> LengthFieldBasedFrameDecoder -> RpcMsgServerSerializeHandler -> MultiplexServerDispatcher
 *
 *     下行： 服务调用返回数据    第一级RPC服务消息序列化            消息头部添加长度           IO事件循环           异步Socket IO
 *           RpcMsgService -> RpcMsgServerSerializeHandler -> LengthFieldPrepender -> EventBaseHandler -> AsyncSocketHandler
 */
class RpcMsgPipelineFactory : public PipelineFactory<RpcMsgServerSerializePipeline> {
public:
    /*RpcMsgPipelineFactory构造函数 默认线程池线程数为4*/
    RpcMsgPipelineFactory(HandleRpcCallFunc arg)
            :service_(std::make_shared<CPUThreadPoolExecutor>(4),
                      std::make_shared<RpcMsgService>(arg))
    {
        //std::cout<<"RpcMsgPipelineFactory Constructed!!! --- [1]"<<std::endl;
    }
    /*exeThreadNum设置服务执行线程池线程数量*/
    RpcMsgPipelineFactory(HandleRpcCallFunc arg, int exeThreadNum)
            :service_(std::make_shared<CPUThreadPoolExecutor>(exeThreadNum),
                      std::make_shared<RpcMsgService>(arg))
    {
        //std::cout<<"RpcMsgPipelineFactory Constructed!!! --- [2]"<<std::endl;
    }

    /*RPC消息处理流工厂创建消息处理pipeline*/
    RpcMsgServerSerializePipeline::Ptr newPipeline(std::shared_ptr<AsyncTransportWrapper> sock) override
    {
        auto pipeline = RpcMsgServerSerializePipeline::create();

        pipeline->addBack(AsyncSocketHandler(sock));

        // enable write message into socket in any thread
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

        /*RPC Message Protobuf序列化Handle 第一级序列化*/
        pipeline->addBack(RpcMsgSerializeHandler());

        /*RPC服务端消息分发器*/
        pipeline->addBack(MultiplexServerDispatcher<rpc::codec::RpcMessage, rpc::codec::RpcMessage>(&service_));

        pipeline->finalize();

        return pipeline;
    }

private:
    //Service 执行过滤器（设置服务执行线程池）
    ExecutorFilter<rpc::codec::RpcMessage, rpc::codec::RpcMessage> service_;
};

/*
 * protobuf RPC Server
 */
class pbRPCServer {
public:
    typedef int32_t serviceID;
    typedef int32_t methodID;

public:
    /*方法元数据*/
    struct MethodData {
        const google::protobuf::MethodDescriptor* m_descriptor;  //protobuf方法描述
        const google::protobuf::Message* request_proto;          //protobuf请求数据类型
        const google::protobuf::Message* response_proto;         //protobuf响应数据类型
    };
    /*服务元数据*/
    struct ServiceData {
        google::protobuf::Service* rpcService;                   //protobuf rpc service
        std::unordered_map<methodID, MethodData> methodMap;      //存储methodId与方法对应关系（通过方法ID查询相应的方法）
    };

    /*注册服务*/
    bool RegisterService(::google::protobuf::Service* service)
    {
        const google::protobuf::ServiceDescriptor* pSerDes = service->GetDescriptor();

        int methodCnt = pSerDes->method_count();

        //获取service id 进行farmhash运算 将service name 转为 uint32
        std::string serviceName = pSerDes->full_name();
        uint32_t serviceId = folly::hash::farmhash::Hash32(serviceName.c_str(), serviceName.length());
        //std::cout<<"[RegisterService] -- "<<pSerDes->full_name()<<std::endl;
        //std::cout<<serviceID<<std::endl;

        ServiceData servicetmp;

        for(int i=0; i<methodCnt; i++)
        {
            const google::protobuf::MethodDescriptor* pMethodDes = pSerDes->method(i);
            const google::protobuf::Message* request_proto = &service->GetRequestPrototype(pMethodDes);
            const google::protobuf::Message* response_proto = &service->GetResponsePrototype(pMethodDes);

            MethodData methodtmp;
            methodtmp.m_descriptor  = pMethodDes;
            methodtmp.request_proto = request_proto;
            methodtmp.response_proto= response_proto;

            //获取method id
            uint32_t methodId = (uint32_t)i;

            servicetmp.rpcService = service;
            servicetmp.methodMap[methodId] = methodtmp;
        }

        _rpcCallMap[serviceId] = servicetmp;

        return true;
    }
    
    /*启动RPC服务器*/
    void Start(int bindPort)
    {
        tcpServer.childPipeline(
                std::make_shared<RpcMsgPipelineFactory>
                        (std::bind( &pbRPCServer::handleRpcCall,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2 )
                        ));

        tcpServer.bind(bindPort);

        std::cout<<"RPC Server Start!"<<std::endl;

        tcpServer.waitForStop();
    }


private:
    /*根据serviceId获取service*/
    google::protobuf::Service* GetService(uint32_t serviceId)
    {
        return _rpcCallMap[serviceId].rpcService;
    }

    /*根据methodId获取method*/
    MethodData* GetMethod(uint32_t serviceId, uint32_t methodId)
    {
        return &_rpcCallMap[serviceId].methodMap[methodId];
    }

    /*根据RPC请求的serviceId和methodId取出相应的方法去执行并获取执行结果写回响应*/
    void handleRpcCall(rpc::codec::RpcMessage req, rpc::codec::RpcMessage &res)
    {
        google::protobuf::Service* rpc_service = GetService(req.serviceid());
        MethodData* this_method = GetMethod(req.serviceid(), req.methodid());

        google::protobuf::Message* request  = this_method->request_proto->New();
        google::protobuf::Message* response = this_method->response_proto->New();
        request->ParseFromString(req.request());

        rpc_service->CallMethod(this_method->m_descriptor, NULL, request, response, NULL);

        std::string response_str;
        response->SerializeToString(&response_str);

        res.set_type( rpc::codec::RESPONSE );
        res.set_id( req.id() );
        res.set_response( response_str );
        res.set_serviceid( req.serviceid() );
        res.set_methodid( req.methodid() );
        res.set_error(rpc::codec::NO_ERROR);
    }

private:
    /*RPC服务方法调用map*/
    std::unordered_map<serviceID, ServiceData> _rpcCallMap;

    /*TCP服务器*/
    ServerBootstrap<RpcMsgServerSerializePipeline> tcpServer;
};


#endif //RPC_PROTOBUF_CPP_PBRPCSERVER_H
