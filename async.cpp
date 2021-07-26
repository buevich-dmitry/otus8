#include "async.h"
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>

namespace async {

namespace {

ContextId GetUniqueContextDescriptor() {
    static ContextId context_id = 0;
    return ++context_id;
}

}  //anonymous namespace

struct Context {
    std::shared_ptr<CommandHandler> command_handler;
    std::mutex mutex;
};

class AsyncResponseHandler : public ResponseHandler {
public:
    explicit AsyncResponseHandler(std::shared_ptr<ResponseHandler> inner_response_handler)
            : inner_response_handler_(std::move(inner_response_handler)) {
        thread_ = std::thread{std::bind(&AsyncResponseHandler::Run, this)};
    }

    void HandleResponse(const Response& response) override {
        std::lock_guard lock{mutex_};
        response_queue_.push(response);
        cv_.notify_all();
    }

    void Stop() {
        std::unique_lock lock{mutex_};
        stop_ = true;
        cv_.notify_all();
        lock.unlock();
        thread_.join();
    }

    bool IsStopped() {
        return stop_;
    }

private:
    void Run() {
        std::unique_lock lock{mutex_};
        while (!stop_ || !response_queue_.empty()) {
            cv_.wait(lock, [this] { return !response_queue_.empty() || stop_; });
            if (!response_queue_.empty()) {
                auto response = response_queue_.front();
                response_queue_.pop();
                lock.unlock();
                inner_response_handler_->HandleResponse(response);
                lock.lock();
            }
        }
    }

    std::shared_ptr<ResponseHandler> inner_response_handler_;
    std::thread thread_;
    std::queue<Response> response_queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_ = false;
};

std::shared_ptr<AsyncResponseHandler>
MakeAsyncResponseHandler(std::shared_ptr<ResponseHandler> inner_response_handler) {
    return std::make_shared<AsyncResponseHandler>(std::move(inner_response_handler));
}

class GlobalContext {
public:
    static GlobalContext& GetInstance() {
        static GlobalContext global_context;
        return global_context;
    }

    void AddResponseHandler(std::shared_ptr<ResponseHandler> handler) {
        const auto async_response_handler = MakeAsyncResponseHandler(std::move(handler));
        response_handlers_.push_back(async_response_handler);
        for (const auto& [_, command_handler] : command_handlers_) {
            std::ignore = _;
            command_handler->AddResponseHandler(async_response_handler);
        }
    }

    ContextId Connect(size_t block_size) {
        std::lock_guard lock{mutex_};
        ContextId context_id = GetUniqueContextDescriptor();
        assert(!command_handlers_.count(context_id));
        command_handlers_[context_id] = MakeCommandHandler(block_size);
        return context_id;
    }

    void Receive(const std::string& command, ContextId context_id) {
        std::lock_guard lock{mutex_};
        command_handlers_.at(context_id)->HandleCommand(command);
    }

    void Disconnect(ContextId context_id) {
        std::lock_guard lock{mutex_};
        command_handlers_.at(context_id)->Stop();
        command_handlers_.erase(context_id);
        if (command_handlers_.empty()) {
            for (const auto& response_handler : response_handlers_) {
                response_handler->Stop();
            }
        }
    }

    void ResetResponseHandlers() {
        for (const auto& response_handler : response_handlers_) {
            assert(response_handler->IsStopped());
        }
        response_handlers_.clear();
        for (const auto& [_, command_handler] : command_handlers_) {
            std::ignore = _;
            command_handler->ResetResponseHandlers();
        }
    }

private:
    std::shared_ptr<CommandHandler> MakeCommandHandler(size_t block_size) {
        auto handler = std::make_shared<CommandHandler>(block_size);
        for (const auto& response_handler : response_handlers_) {
            handler->AddResponseHandler(response_handler);
        }
        return handler;
    }

    std::unordered_map<ContextId, std::shared_ptr<CommandHandler>> command_handlers_;
    std::vector<std::shared_ptr<AsyncResponseHandler>> response_handlers_;
    std::mutex mutex_;
};


void AddResponseHandler(std::shared_ptr<ResponseHandler> handler) {
    GlobalContext::GetInstance().AddResponseHandler(std::move(handler));
}

ContextId Connect(size_t block_size) {
    return GlobalContext::GetInstance().Connect(block_size);
}

void Receive(const std::string& command, ContextId context_id) {
    GlobalContext::GetInstance().Receive(command, context_id);
}

void Disconnect(ContextId context_id) {
    GlobalContext::GetInstance().Disconnect(context_id);
}

void ResetResponseHandlers() {
    GlobalContext::GetInstance().ResetResponseHandlers();
}

} // namespace async
