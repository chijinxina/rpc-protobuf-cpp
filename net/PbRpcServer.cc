#include "net/PbRpcServer.h"
#include "PbRpcServer.h"


/*
 * RpcMsgService
 */
//RpcMsgService构造函数
RpcMsgService::RpcMsgService(HandleRpcCallFunc arg)
        : _handleRpcCall(arg)
{}

//RpcMsgService调用流程
Future <rpc::codec::RpcMessage> RpcMsgService::operator()(rpc::codec::RpcMessage request)
{
//    printf("RpcService Request: \n");
//    printf("------- request.request = %s \n",request.request().c_str());
//    printf("------- request.response = %s \n",request.response().c_str());

    rpc::codec::RpcMessage response;

    //执行RPC函数体
    _handleRpcCall(request, response);

    return response;
}


/*
 * RpcMsgPipelineFactory
 */

//RpcMsgPipelineFactory构造函数
RpcMsgServerPipelineFactory::RpcMsgServerPipelineFactory(HandleRpcCallFunc arg)
        :service_(std::make_shared<CPUThreadPoolExecutor>(std::thread::hardware_concurrency()),
        std::make_shared<RpcMsgService>(arg))
{
    //std::cout<<"RpcMsgPipelineFactory Constructed!!! --- [1]"<<std::endl;
}

RpcMsgServerPipelineFactory::RpcMsgServerPipelineFactory(HandleRpcCallFunc arg, int exeThreadNum)
        :service_(std::make_shared<CPUThreadPoolExecutor>(exeThreadNum),
        std::make_shared<RpcMsgService>(arg))
{
    //std::cout<<"RpcMsgPipelineFactory Constructed!!! --- [2]"<<std::endl;
}

RpcMsgServerSerializePipeline::Ptr
RpcMsgServerPipelineFactory::newPipeline(std::shared_ptr<AsyncTransportWrapper> socket)
{
    auto pipeline = RpcMsgServerSerializePipeline::create();

    pipeline->addBack(AsyncSocketHandler(socket));

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


/*
 * pbRPCServer
 */

//注册服务
bool pbRPCServer::RegisterService(::google::protobuf::Service *service)
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

//启动RPC服务器
void pbRPCServer::Start(int bindPort)
{
    tcpServer.childPipeline(
            std::make_shared<RpcMsgServerPipelineFactory>
                    (std::bind( &pbRPCServer::handleRpcCall,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2 )
                    ));

    tcpServer.bind(bindPort);

    std::cout<<"RPC Server Start!"<<std::endl;

    tcpServer.waitForStop();
}

//根据serviceId获取service
google::protobuf::Service *pbRPCServer::GetService(uint32_t serviceId)
{
    return _rpcCallMap[serviceId].rpcService;
}

//根据methodId获取method
pbRPCServer::MethodData *pbRPCServer::GetMethod(uint32_t serviceId, uint32_t methodId)
{
    return &_rpcCallMap[serviceId].methodMap[methodId];
}


//根据RPC请求的serviceId和methodId取出相应的方法去执行并获取执行结果写回响应
void pbRPCServer::handleRpcCall(rpc::codec::RpcMessage req, rpc::codec::RpcMessage &res)
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

