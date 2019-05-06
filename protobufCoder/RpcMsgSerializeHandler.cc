#include <iostream>
#include <wangle/channel/Handler.h>
#include <folly/ExceptionWrapper.h>
#include <folly/futures/Future.h>
#include "protobufCoder/RpcMessage.pb.h"
#include "protobufCoder/RpcMsgSerializeHandler.h"

//从IObuf中读取数据 到  RpcMessage中 利用protobuf进行反序列化 得到 rpc::codec::RpcMessage
void RpcMsgSerializeHandler::read(Context* ctx, std::unique_ptr <folly::IOBuf> msg)
{
    rpc::codec::RpcMessage in;
    in.ParseFromArray((void*)msg->data(), msg->length());

//        printf("[ServerSerializeHandler] READ: \n");
//        printf("------- in id       = %d \n",in.id());
//        printf("------- in request  = %s \n",in.request().c_str());
//        printf("------- in response = %s \n",in.response().c_str());

    ctx->fireRead(std::move(in));
}


//读取到EOF 说明对端关闭了连接
void RpcMsgSerializeHandler::readEOF(Context* ctx)
{
    std::cout<<"[ServerSerializeHandler] -- read EOF"<<std::endl;
    Handler::readEOF(ctx);
}


//读取数据出现异常
void RpcMsgSerializeHandler::readException(Context* ctx, folly::exception_wrapper e)
{
    std::cout<<"[ServerSerializeHandler] -- read Exception:"<<exceptionStr(e)<<std::endl;
    Handler::readException(ctx, e);
}


//从RpcMessage中写数据 到 IOBuf中 利用protobuf进行序列化 得到二进制的RpcMessage
folly::Future <folly::Unit> RpcMsgSerializeHandler::write(Context* ctx, rpc::codec::RpcMessage msg)
{
    std::string outstr;

    //将RpcMesage序列化写入到string中
    msg.SerializePartialToString(&outstr);

//      std::cout<<"[ServerSerializeHandler WRITE]:"<<std::endl;
//      std::cout<<outstr<<std::endl;

    return ctx->fireWrite(folly::IOBuf::copyBuffer(outstr));
}


//写入数据出现异常
folly::Future <folly::Unit>RpcMsgSerializeHandler::writeException(Context* ctx, folly::exception_wrapper e)
{
    std::cout<<"[ServerSerializeHandler] -- write Exception:"<<exceptionStr(e)<<std::endl;
    return Handler::writeException(ctx, e);
}


folly::Future <folly::Unit> RpcMsgSerializeHandler::close(Context* ctx)
{
    std::cout<<"[ServerSerializeHandler] -- pipeline closed"<<std::endl;
    return Handler::close(ctx);
}
