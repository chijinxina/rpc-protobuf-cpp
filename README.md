#rpc-protobuf-cpp
####基于[wangle](https://github.com/facebook/wangle)框架和[google protobuf](https://github.com/protocolbuffers/protobuf)序列化框架实现的CPP版高性能RPC框架

**提供3种风格的RPC客户端**
1. google protobuf风格的同步调用RPC客户端
2. google protobuf风格的异步调用RPC客户端
3. Future/Promise风格的异步调用RPC客户端


**rpc请求/响应的序列化编码：**
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
