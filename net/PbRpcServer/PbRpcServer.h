//
// Created by chijinxin on 19-4-28.
//

#ifndef RPC_PROTOBUF_CPP_PBRPCSERVER_H
#define RPC_PROTOBUF_CPP_PBRPCSERVER_H

#include <iostream>

#include <folly/init/Init.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>

#include <wangle/service/Service.h>
#include <wangle/service/ExecutorFilter.h>
#include <wangle/channel/AsyncSocketHandler.h>
#include <wangle/channel/EventBaseHandler.h>
#include <wangle/bootstrap/ServerBootstrap.h>

#include <codec/LengthFieldPrepender.h>
#include <codec/LengthFieldBasedFrameDecoder.h>

class pbRPCServer {

};


#endif //RPC_PROTOBUF_CPP_PBRPCSERVER_H
