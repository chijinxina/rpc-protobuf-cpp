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

/*MyService::Add()响应回调函数*/
void AddResponseCallback(OperaReq* req, OperaRes* res)
{
    cout<<"Async call MyService::Add()  req: ";
    cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a+b="<<res->c()<<endl;
    delete req;
    delete res;
}

/*MyService::Sub()响应回调函数*/
void SubResponseCallback(OperaReq* req, OperaRes* res)
{
    cout<<"Async call MyService::Sub()  req: ";
    cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a-b="<<res->c()<<endl;
    delete req;
    delete res;
}

/*Service2::Mul()响应回调函数*/
void MulResponseCallback(OperaReqF* req, OperaResF* res)
{
    cout<<"Async call Service2::Mul()   req: ";
    cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a*b="<<res->c()<<endl;
    delete req;
    delete res;
}

/*Service2::Div()响应回调函数*/
void DivResponseCallback(OperaReqF* req, OperaResF* res)
{
    cout<<"Async call Service2::Div()   req: ";
    cout<<"a="<<req->a()<<", b="<<req->b()<<" | res: a/b="<<res->c()<<endl;
    delete req;
    delete res;
}


/*
 * Google Protobuf 风格的RPC异步回调
 * google::protobuf::NewCallback()
 */
int main()
{
    PbRpcClient rpcClient("127.0.0.1", 8888);

    RpcChannel channel(&rpcClient);

    /*
     * service MyService
     * {
     *      rpc Echo(EchoReq) returns(EchoRes);
     *      rpc Add(OperaReq) returns(OperaRes);
     *      rpc Sub(OperaReq) returns(OperaRes);
     * }
     */
    MyService_Stub rpc_myservice(&channel);

    /*
     * service Service2
     * {
     *      rpc Mul(OperaReqF) returns(OperaResF);
     *      rpc Div(OperaReqF) returns(OperaResF);
     * }
     */
    Service2_Stub rpc_service2(&channel);

    int i = 0;
    while(i<1000000)
    {
        //1. Test MyService::Echo()
        if(i%5 == 0)
        {
            EchoReq* req = new EchoReq();
            EchoRes* res = new EchoRes();

            ostringstream ss;
            ss<<" --- "<<i<<" --- ";
            req->set_request(ss.str());

            rpc_myservice.Echo(NULL, req, res, google::protobuf::NewCallback(&EchoResponseCallback, req, res));
        }
        //2. Test MyService::Add()
        else if(i%5 == 1)
        {
            OperaReq* req = new OperaReq();
            OperaRes* res = new OperaRes();

            req->set_a(i);
            req->set_b(i);
            rpc_myservice.Add(NULL, req, res, google::protobuf::NewCallback(&AddResponseCallback, req, res));
        }
        //3. Test MyService::Sub()
        else if(i%5 == 2)
        {
            OperaReq* req = new OperaReq();
            OperaRes* res = new OperaRes();

            req->set_a(i*2);
            req->set_b(i);
            rpc_myservice.Sub(NULL, req, res, google::protobuf::NewCallback(&SubResponseCallback, req, res));
        }
        //4. Test Service2::Mul()
        else if(i%5 == 3)
        {
            OperaReqF* req = new OperaReqF();
            OperaResF* res = new OperaResF();

            req->set_a(i);
            req->set_b(i);
            rpc_service2.Mul(NULL, req, res, google::protobuf::NewCallback(&MulResponseCallback, req, res));
        }
        //5. Test Service2::Div()
        else
        {
            OperaReqF* req = new OperaReqF();
            OperaResF* res = new OperaResF();

            req->set_a(i+i/10);
            req->set_b(i);
            rpc_service2.Div(NULL, req, res, google::protobuf::NewCallback(&DivResponseCallback, req, res));
        }

        i++;
    }

    this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}