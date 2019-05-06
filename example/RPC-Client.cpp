//
// Created by chijinxin on 19-4-29.
//
#include <iostream>
#include <string>
#include <sstream>

#include "net/PbRpcClient.h"
#include "MyService.pb.h"

using namespace std;
using namespace example::rpcProto;

/*MyService::Echo()响应回调函数*/
void EchoResponseCallback(EchoReq* req, EchoRes* res)
{
    cout<<"Async call MyService::Echo() req: "<<req->request()<<"| res: "<<res->response()<<endl;
    delete req;
    delete res;
}

int main()
{
    PbRpcClient rpcClient("127.0.0.1", 8888, 4);

    RpcChannel channel(&rpcClient);

    MyService_Stub rpc_ctl(&channel);

//    int i=0;
//    while(i<10000)
//    {
//
//        EchoReq* req = new EchoReq();
//        EchoRes* res = new EchoRes();
//
//        ostringstream ss;
//        ss<<" --- "<<i<<" --- ";
//        req->set_request(ss.str());
//
//        rpc_ctl.Echo(NULL, req, res, NULL);
//
//        cout<<"Sync call MyService::Echo() req: "<<req->request()<<"| res: "<<res->response()<<endl;
//
//        delete req;
//        delete res;
//
//        i++;
//    }


    int i = 0;
    while(i<10000)
    {

        EchoReq* req = new EchoReq();
        EchoRes* res = new EchoRes();

        ostringstream ss;
        ss<<" --- "<<i<<" --- ";
        req->set_request(ss.str());

        rpc_ctl.Echo(NULL, req, res, google::protobuf::NewCallback(&EchoResponseCallback, req, res));

        i++;
    }

    return 0;
}