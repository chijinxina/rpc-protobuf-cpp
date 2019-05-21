//
// Created by chijinxin on 19-5-7.
//
#include <sstream>
#include "futureRpcCallProxy.h"

//构造函数
futureRpcCallProxy::futureRpcCallProxy(google::protobuf::Service* s, int ioThreadNum)
        :request_id(0L),
        service(s),
         lbs(RoundRobin),
         getLocalIP_flag(false)
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


/*
 * 添加RPC远程服务器
 * param:
 *      host: RPC服务器主机IP地址
 *      port: RPC服务器端口
 *      reqLimit: 最大pending请求的数量限制 默认值为INT64_MAX
 *      t: 远程RPC请求超时时间 单位ms
 */
void futureRpcCallProxy::addRemoteHost(std::string host, int port, uint32_t t, uint64_t reqLimit)
{
    //在更新提供服务的RPC主机列表时需要加锁
    std::unique_lock<std::mutex> lock(update_mu);

    std::shared_ptr<PbRpcClient> clientPtr = std::make_shared<PbRpcClient>(host, port, ioThreadPool, reqLimit, t);

    SocketAddress addr;
    //std::cout<<clientPtr->pipeline->getTransportInfo()->localAddr->describe()<<std::endl;
    //LocalIPstr = addr.getIPAddress().str();
    //std::cout<<LocalIPstr<<std::endl;

    vRpcClient.push_back(clientPtr);


//    //获取本地IP地址
//    if(!getLocalIP_flag)
//    {
//        SocketAddress addr;
//        clientPtr->pipeline->getTransport()->getLocalAddress(&addr);
//        LocalIPstr = addr.getIPAddress().str();
//        getLocalIP_flag = true;
//    }
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
//根据本地主机地址、服务名字、方法名字 进行hash运算 选取对应的远程RPC主机
//相同的请求会被映射到相同的RPC服务器，适合维护长连接和提高命中率
std::shared_ptr<PbRpcClient> futureRpcCallProxy::Hash_Select(std::string host,
        std::string service_name, std::string method_name)
{
    std::unique_lock<std::mutex> lock(update_mu);

    std::ostringstream ostr;
    ostr<<host<<service_name<<method_name;

    uint32_t hashCode = folly::hash::farmhash::Hash32(ostr.str().c_str(), ostr.str().length());

    return vRpcClient[ hashCode % vRpcClient.size() ];
}
