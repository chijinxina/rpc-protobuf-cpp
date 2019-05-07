cmake_minimum_required(VERSION 3.0.2)

######### RPC客户端测试 promise/future风格异步RPC调用 #######
add_executable(SyncRpcClientTest
        MyService.pb.cc
        SyncRpcClientTest.cpp
        )
target_link_libraries(SyncRpcClientTest
        pbrpc
        )

######### RPC客户端测试 google::protobuf风格异步回调 ########
add_executable(AsyncRpcClientTest
        MyService.pb.cc
        AsyncRpcClientTest.cpp
        )
target_link_libraries(AsyncRpcClientTest
        pbrpc
        )

########## RPC客户端测试 promise/future风格异步RPC调用 #######
#add_executable(SyncRpcClientTest
#        MyService.pb.cc
#        AsyncRpcClientTest.cpp
#        )
#target_link_libraries(AsyncRpcClientTest
#        pbrpc
#        )


################ RPC 服务器测试 ###########################
add_executable(RpcServerTest
        MyService.pb.cc
        RpcServerTest.cpp
        )
target_link_libraries(RpcServerTest
        pbrpc
        )

