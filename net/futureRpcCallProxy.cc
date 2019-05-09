//
// Created by chijinxin on 19-5-7.
//
#include "futureRpcCallProxy.h"

//构造函数
futureRpcCallProxy::futureRpcCallProxy(google::protobuf::Service* s, int ioThreadNum)
        :request_id(0L),
        service(s),
         lbs(RoundRobin)
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

//设置负载均衡策略 默认为轮询
void futureRpcCallProxy::setLBStrategy(LBStrategy l)
{
    lbs = l;
}

//添加RPC远程主机
void futureRpcCallProxy::addRemoteHost(std::string host, int port)
{
    std::unique_lock<std::mutex> lock(update_mu);

    vRpcClient.push_back(std::make_shared<PbRpcClient>(host, port, ioThreadPool));
}

//轮询策略 选择服务器
std::shared_ptr<PbRpcClient> futureRpcCallProxy::RoundRobin_Select(long curReqId)
{
    std::unique_lock<std::mutex> lock(update_mu);
    return vRpcClient[ curReqId % vRpcClient.size() ];
}

//随机策略 选择服务器
std::shared_ptr<PbRpcClient> futureRpcCallProxy::Random_Select(long curReqId)
{

}

//Hash策略 选择服务器
std::shared_ptr<PbRpcClient> futureRpcCallProxy::Hash_Select()
{

}
