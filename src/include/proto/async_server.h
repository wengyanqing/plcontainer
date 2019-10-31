#ifndef __ASYNC_SERVER_H__
#define __ASYNC_SERVER_H__

#include <memory>
#include <string>

#include <unistd.h>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include "plcontainer.grpc.pb.h"

#include "interface.h"

extern "C"
{
#include "postgres.h"
#include "mb/pg_wchar.h"
#include "common/comm_dummy.h"
#include "plc/containers.h"
}

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::CompletionQueue;
using grpc::Status;

using namespace plcontainer;

class AsyncServer final {
public:
    AsyncServer(const std::string& uds);
    ~AsyncServer();

    void Init();    
    void Start();
    void ProcessRequest(int timeout_seconds);

private:
    std::string uds_;
    PLCoordinator::AsyncService service_;
    std::unique_ptr<Server> server_;
    std::unique_ptr<ServerCompletionQueue> cq_;
    void* call_ = nullptr;
};

#endif
