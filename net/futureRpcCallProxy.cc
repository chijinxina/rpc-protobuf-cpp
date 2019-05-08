//
// Created by chijinxin on 19-5-7.
//
#include "futureRpcCallProxy.h"

//构造函数
futureRpcCallProxy::futureRpcCallProxy(google::protobuf::Service* s, int ioThreadNum)
        :request_id(0L),
        service(s)
{
    //默认为CPU核数
    if(ioThreadNum == 0)
    {
        int cpuNum = std::thread::hardware_concurrency();
        ioThreadPool = std::make_shared<folly::IOThreadPoolExecutor>(cpuNum);
    }
    else
    {
        ioThreadPool = std::make_shared<folly::IOThreadPoolExecutor>(ioThreadNum);
    }
}

void futureRpcCallProxy::setLBStrategy() {

}

//添加RPC远程主机
void futureRpcCallProxy::addRemoteHost(std::string host, int port)
{
    std::unique_lock<std::mutex> lock(update_mu);

    vRpcClient.push_back(std::make_shared<PbRpcClient>(host, port, ioThreadPool));
}



void futureRpcCallProxy::RoundRobinRun() {

}
