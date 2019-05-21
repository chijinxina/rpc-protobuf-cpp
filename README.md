# rpc-protobuf-cpp

### 基于[wangle](https://github.com/facebook/wangle)框架和[google protobuf](https://github.com/protocolbuffers/protobuf)序列化框架实现的C++版高性能RPC框架


### 特性
**1. 采用google protobuf消息序列化和反序列化机制，并采用protobuf描述RPC服务**

**2. TCP长连接纯异步RPC调用**

**3. 支持在同一个端口绑定多个服务**

**4. 对服务名称进行hash计算服务hashcode，减小网络消息传输压力**

**5. 提供3种风格的RPC客户端**
1. google protobuf风格的同步调用RPC客户端
2. google protobuf风格的异步调用RPC客户端
3. Future/Promise风格的异步调用RPC客户端

### 源码编译及相关库依赖
![rpc](https://github.com/chijinxina/rpc-protobuf-cpp/blob/master/doc/rpc.png)
1. 安装folly组件库 [folly编译安装](https://github.com/facebook/folly)
2. 安装wangle网络库 [wangle编译安装](https://github.com/facebook/wangle)
3. 编译rpc-protobuf-cpp
```
git clone --recursive https://github.com/chijinxina/rpc-protobuf-cpp.git
cd rpc-protobuf-cpp
mkdir build
cd build
cmake ..
make
```

### 相关文件
**1. RPC底层网络 net：**
1. RPC客户端 - PbRpcClient.cc
2. RPC服务器 - PbRpcServer.cc
3. Future/Promise风格的RPC客户端调用代理 - futureRpcCallProxy.cc
4. wangle service服务器/客户端消息分发器 - ServiceDispatcher 

**2. wangle相关的网络消息编码器 codec：**
1. 上游定长消息解码器（消息头部包含消息长度）- codec/LengthFieldBasedFrameDecoder.cpp
2. 下游定长消息编码器（计算消息长度并在头部添加长度域）- codec/LengthFieldPrepender.cpp

**3. Protobuf消息编码器 protobufCoder：**
1. rpc请求/响应消息的序列化编码文件  -  protobufCoder/RpcMessage.proto
```
enum MsgType
{
    REQUEST = 1;
    RESPONSE = 2;
    ERROR = 3; // not used
}

enum ErrorCode
{
    NO_ERROR = 0;        //正确响应
    WRONG_PROTO = 1;     //协议错误
    NO_SERVICE = 2;      //找不到服务
    NO_METHOD = 3;       //找不到方法
    INVALID_REQUEST = 4; //错误请求
    INVALID_RESPONSE = 5;//错误响应
    TIMEOUT = 6;         //请求超时
}

message RpcMessage
{
    required MsgType type     = 1;     //消息类型
    required int64 id         = 2;     //请求id requiest_id 类http2.0的无序请求-响应
    required uint32 serviceId = 3;     //服务ID
    required uint32 methodId  = 4;     //方法ID

    optional bytes request    = 5;     //请求 protocol二进制编码
    optional bytes response   = 6;     //响应 protocol二进制编码
    
    optional ErrorCode error  = 7;     //错误码
}
```
2. wangle中RPC消息序列化与反序列化处理Handler - protobufCoder/RpcMsgSerializeHandler.cc

**4. 相关工具 util:**
1. string字符串分割 - StringUtil.cc
2. 获取网卡IP地址 - LinuxNetworkUtil.cc

**5. 示例程序 example:**
1. protobuf RPC定义文件  -  MyService.proto
2. RPC服务器测试 - RpcServerTest.cpp
3. protobuf风格的同步RPC客户端  -  SyncRpcClientTest.cpp
4. protobuf异步回调风格的RPC客户端  -  AsyncRpcClientTest.cpp
5. Future/Promise风格的异步RPC客户端  -  FutureRpcClientTest.cc

