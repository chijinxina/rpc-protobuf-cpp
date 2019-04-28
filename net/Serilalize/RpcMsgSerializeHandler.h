//
// Created by chijinxin on 19-4-22.
//

#ifndef WANGLE_RPCMSGSERIALIZEHANDLER_H
#define WANGLE_RPCMSGSERIALIZEHANDLER_H

/*
 * RPC Message SerializeHandler
 *    message RpcMessage
 *    {
 *       required MsgType type    = 1;   //消息类型
 *       required int64 id        = 2;   //请求id requiest_id 类http2.0标准
 *       optional bytes request   = 3;   //请求 protocol二进制编码
 *       optional bytes response  = 4;   //响应 protocol二进制编码
 *       optional ErrorCode error = 5;   //错误码
 *    }
 */
class RpcMsgSerializeHandler
           : public wangle::Handler< std::unique_ptr<folly::IOBuf>,
                                     rpc::codec::RpcMessage,
                                     rpc::codec::RpcMessage,
                                     std::unique_ptr<folly::IOBuf> >
{
public:
    //从IObuf中读取数据 到  RpcMessage中 利用protobuf进行反序列化 得到 rpc::codec::RpcMessage
    void read(Context* ctx, std::unique_ptr<folly::IOBuf> msg) override
    {
        rpc::codec::RpcMessage in;

        in.ParseFromArray((void*)msg->data(), msg->length());

//        printf("[ServerSerializeHandler] READ: \n");
//        printf("------- in id       = %d \n",in.id());
//        printf("------- in request  = %s \n",in.request().c_str());
//        printf("------- in response = %s \n",in.response().c_str());
        ctx->fireRead(std::move(in));
    }

    //从RpcMessage中写数据 到 IOBuf中 利用protobuf进行序列化 得到二进制的RpcMessage
    folly::Future<folly::Unit> write(Context* ctx, rpc::codec::RpcMessage out) override
    {
        std::string outstr;

        //将RpcMesage序列化写入到string中
        out.SerializePartialToString(&outstr);

        //      std::cout<<"[ServerSerializeHandler WRITE]:"<<std::endl;
        //      std::cout<<outstr<<std::endl;

        return ctx->fireWrite(folly::IOBuf::copyBuffer(outstr));
    }



    //读取到EOF 说明对端关闭了连接
    void readEOF(Context *ctx) override
    {
        std::cout<<"[ServerSerializeHandler] -- read EOF"<<std::endl;
        Handler::readEOF(ctx);
    }

    //读取数据出现异常
    void readException(Context *ctx, folly::exception_wrapper e) override
    {
        std::cout<<"[ServerSerializeHandler] -- read Exception:"<<exceptionStr(e)<<std::endl;
        Handler::readException(ctx, e);
    }

    //写入数据出现异常
    folly::Future<folly::Unit> writeException(Context *ctx, folly::exception_wrapper e) override
    {
        std::cout<<"[ServerSerializeHandler] -- write Exception:"<<exceptionStr(e)<<std::endl;
        return Handler::writeException(ctx, e);
    }
};


#endif //WANGLE_RPCMSGSERIALIZEHANDLER_H
