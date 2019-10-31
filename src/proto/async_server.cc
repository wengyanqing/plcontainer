#include "async_server.h"

const grpc::Status invalid_status(grpc::StatusCode::INVALID_ARGUMENT, "invalid argument");

// Simple POD struct used as an argument wrapper for calls
struct CallData {
    PLCoordinator::AsyncService* service;
    grpc::ServerCompletionQueue* cq;
};

// Base class used to cast the void* tags we get from the completion queue and call Proceed() on them.
class Call {
public:
    virtual void Proceed(bool ok) = 0;
};

class StartContainerCall final : public Call {
public:
    explicit StartContainerCall(CallData* data)
        : data_(data), responder_(&ctx_), status_(REQUEST) {
        data->service->RequestStartContainer(&ctx_, &request_, &responder_, data_->cq, data_->cq, this);
    }

    void Proceed(bool ok) {
        switch (status_) {
        case REQUEST:
            new StartContainerCall(data_);
            if (!ok) {
                plc_elog(WARNING, "StartContainer Request is not ok. Finishing.");
                responder_.FinishWithError(grpc::Status::CANCELLED, this);
            } else {
                // TODO start container
                plc_elog(WARNING, "start_container request:%s", request_.request().c_str());
                response_.set_result("start_container address:******");
                responder_.Finish(response_, grpc::Status::OK, this);
            }
            status_ = FINISH;
            break;

        case FINISH:
            if (!ok) {
                plc_elog(ERROR, "StartContainer RPC finished unexpectedly");
            }
            delete this;
            break;
        }
    }

private:
    CallData* data_;
    grpc::ServerContext ctx_;
    grpc::ServerAsyncResponseWriter<StartContainerResponse> responder_;
    StartContainerRequest request_;
    StartContainerResponse response_;
    enum CallStatus { REQUEST, FINISH };
    CallStatus status_;
};

class StopContainerCall final : public Call {
public:
    explicit StopContainerCall(CallData* data)
        : data_(data), responder_(&ctx_), status_(REQUEST) {
        data->service->RequestStopContainer(&ctx_, &request_, &responder_, data_->cq, data_->cq, this);
    }

    void Proceed(bool ok) {
        switch (status_) {
        case REQUEST:
            new StopContainerCall(data_);
            if (!ok) {
                plc_elog(WARNING, "StartContainer Request is not ok. Finishing.");
                responder_.FinishWithError(grpc::Status::CANCELLED, this);
            } else {
                // TODO stop container
                response_.set_result("stop_container response");
                responder_.Finish(response_, grpc::Status::OK, this);
            }
            status_ = FINISH;
            break;

        case FINISH:
            if (!ok) {
                plc_elog(ERROR, "StartContainer RPC finished unexpectedly");
            }
            delete this;
            break;
        }
    }

private:
    CallData* data_;
    grpc::ServerContext ctx_;
    grpc::ServerAsyncResponseWriter<StopContainerResponse> responder_;
    StopContainerRequest request_;
    StopContainerResponse response_;
    enum CallStatus { REQUEST, FINISH };
    CallStatus status_;
};

AsyncServer::AsyncServer(const std::string& uds) {
    uds_ = uds;
}

AsyncServer::~AsyncServer() {
    server_->Shutdown();
    cq_->Shutdown();
}

void AsyncServer::Init() {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(this->uds_, grpc::InsecureServerCredentials());
    builder.RegisterService(&this->service_);
    this->cq_ = builder.AddCompletionQueue();
    this->server_ = builder.BuildAndStart();
    plc_elog(LOG, "Asynchronous server listening on address %s.", this->uds_.c_str());
}

void AsyncServer::Start() {
    CallData data{&(this->service_), this->cq_.get()};
    new StartContainerCall(&data);
    new StopContainerCall(&data);
    plc_elog(LOG, "Asynchronous server started.");
}

void AsyncServer::ProcessRequest(int timeout_seconds) {
    void* tag;
    bool ok;
    CompletionQueue::NextStatus status; 
    std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() + std::chrono::seconds(timeout_seconds);
    for (;;) {
        status = this->cq_->AsyncNext(&tag, &ok, deadline);
        if (status == CompletionQueue::GOT_EVENT) {
            plc_elog(LOG, "aserver got request event to handle, ok:%d", ok);
            static_cast<Call*>(tag)->Proceed(ok);
            continue;
        } else if (status == CompletionQueue::TIMEOUT) {
            plc_elog(LOG, "aserver got timeout event, return to caller, ok:%d", ok);
            break;
        } else if (status == CompletionQueue::SHUTDOWN) {
            plc_elog(LOG, "server got shutdown event, return to caller, ok:%d", ok);
            break;
        } else {
            plc_elog(ERROR, "server got unknown status:%d", status);
            break;
        }
    }
}

PLCoordinatorServer *start_server(const char *address) {
    PLCoordinatorServer *server = (PLCoordinatorServer *)palloc(sizeof(PLCoordinatorServer));
    server->address = pstrdup(address);
    server->server = new AsyncServer(address);
    plc_elog(LOG, "start_server with server:%p address:%s", server->server, server->address);
    ((AsyncServer *)server->server)->Init();
    ((AsyncServer *)server->server)->Start();
    return server;
}

int process_request(PLCoordinatorServer *server, int timeout_seconds) {
    AsyncServer *s = (AsyncServer *)server->server;
    s->ProcessRequest(timeout_seconds);
    plc_elog(LOG, "server %p timeout in %d seconds", server, timeout_seconds);
    return 0;
}
