#include "net/PbRpcClient.h"
#include "PbRpcClient.h"

/*
 * RPC Message pipeline工厂
 * pipeline数据流:
 *     上行： 异步Socket IO          IO事件循环           消息分帧(头部2byte的消息长度)      第一级RPC服务消息序列化             服务端服务消息分发器
 *           AsyncSocketHandler -> EventBaseHandler -> LengthFieldBasedFrameDecoder -> RpcMsgServerSerializeHandler -> MultiplexServerDispatcher
 *
 *     下行： 服务调用返回数据    第一级RPC服务消息序列化            消息头部添加长度           IO事件循环           异步Socket IO
 *           RpcMsgService -> RpcMsgServerSerializeHandler -> LengthFieldPrepender -> EventBaseHandler -> AsyncSocketHandler
 */
RpcMsgClientSerializePipeline::Ptr RpcMsgPipelineFactory::newPipeline(std::shared_ptr<folly::AsyncTransportWrapper> socket)
{
    auto pipeline = RpcMsgClientSerializePipeline::create();
    pipeline->addBack(AsyncSocketHandler(socket));

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

    // RPC Message protobuf 序列化与反序列化
    pipeline->addBack(RpcMsgSerializeHandler());

    pipeline->finalize();
    return pipeline;
}


/*
 * PbRpcClient
 */
//PbRpcClient 构造函数 自定义IO线程池
PbRpcClient::PbRpcClient(std::string host, int port, int ioThreadNum)
        :remoteAddress(host, port),
         pipeline(nullptr),
         connected(false)
{
    if(ioThreadNum != 0)
    {
        //std::cout<<"ioThreadNum = "<<ioThreadNum<<std::endl;
        tcpClient.group(std::make_shared<folly::IOThreadPoolExecutor>(ioThreadNum));
    }
    //默认IOThreadPoolExecutor线程数为CPU核数
    else
    {
        int cpuNum = std::thread::hardware_concurrency();
        tcpClient.group(std::make_shared<folly::IOThreadPoolExecutor>(cpuNum));
        //std::cout<<"ioThreadNum = "<<cpuNum<<std::endl;
    }

    tcpClient.pipelineFactory(std::make_shared<RpcMsgPipelineFactory>());

    dispatcher = std::make_shared<MultiplexClientDispatcher>();

    //与rpc远程服务器建立连接（非阻塞）
    connect();
}

//PbRpcClient 构造函数 指定IO线程池
PbRpcClient::PbRpcClient(std::string host, int port, std::shared_ptr<folly::IOThreadPoolExecutor> ioThreadPool)
         :remoteAddress(host, port),
         pipeline(nullptr),
         connected(false)
{
    tcpClient.group(ioThreadPool);
    tcpClient.pipelineFactory(std::make_shared<RpcMsgPipelineFactory>());

    dispatcher = std::make_shared<MultiplexClientDispatcher>();

    //与rpc远程服务器建立连接（非阻塞）
    connect();
}

//析构函数
PbRpcClient::~PbRpcClient()
{
    //同步地等待连接关闭
    this->pipeline->close().get();
    delete this->pipeline;
}

//异步连接远程rpc服务器
void PbRpcClient::connect()
{
    tcpClient.connect(remoteAddress)
            .thenValue(
                    [this](RpcMsgClientSerializePipeline* p)
                    {
                        this->pipeline = p;
                        connected = true;
                        this->dispatcher->setPipeline(pipeline);
                        std::cout<<"[RpcClient] connected to the rpc server: "<<this->remoteAddress<<std::endl;
                    })
            .thenError(folly::tag_t<std::exception>{},
                       [this](const std::exception& e)
                       {
                           std::cerr<<"[RpcClient] Connect error: "<< exceptionStr(e);
                           this->pipeline = nullptr;
                       });
}



/*
 * RpcChannel
 */
//RpcChannel 构造函数
RpcChannel::RpcChannel()
{}

RpcChannel::RpcChannel(PbRpcClient *client)
      : rpcClient(client), request_id(0)
{}

RpcChannel::RpcChannel(std::shared_ptr<PbRpcClient> client)
      : rpcClient(client), request_id(0)
{}

//实现RpcChannel中的CallMethod方法
void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                       const google::protobuf::Message *request, google::protobuf::Message *response,
                       google::protobuf::Closure *done) {

    if(!rpcClient->connected.load())
    {
        std::unique_lock<std::mutex>(this->connect_mu);
        rpcClient->pipeline = rpcClient->tcpClient.connect(rpcClient->remoteAddress).get();
        rpcClient->dispatcher->setPipeline(rpcClient->pipeline);
        rpcClient->connected = false;
    }

    //获取service id 进行farmhash运算 将service name 转为 uint32
    std::string serviceName = method->service()->full_name();
    uint32_t serviceId = folly::hash::farmhash::Hash32(serviceName.c_str(), serviceName.length());
    //std::cout<<"Called service name: "<<serviceName<<std::endl;
    //std::cout<<" farmhash code = "<<serviceId<<std::endl;

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
        //std::cout<<"Req id = "<<Req.id()<<std::endl;
        rpc::codec::RpcMessage Res = (*(rpcClient->dispatcher))(Req).get();
        response->ParseFromString( Res.response() );
    }
    /*设置异步回调*/
    else
    {
        //std::cout<<"Req id = "<<Req.id()<<std::endl;
        (*(rpcClient->dispatcher))(Req)
                .thenValue(
                        [Req, response, done](rpc::codec::RpcMessage Res)
                        {
                            //std::cout<<"Res id = "<<Res.id()<<std::endl;
                            response->ParseFromString( Res.response() );
                            done->Run();
                        })
                .thenError(folly::tag_t<std::exception>{},
                           [this, Req, response](const std::exception& e)
                           {
                               std::cerr<<"[RpcClient] call error: "<< exceptionStr(e);
                           });
    }
}