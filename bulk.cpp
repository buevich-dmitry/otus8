#include "bulk.h"

class CommandHandlerImpl {
public:
    explicit CommandHandlerImpl(size_t max_block_size) : max_block_size_(max_block_size) {
    }

    void HandleCommand(const std::string& command) {
        std::vector<std::string> response;
        if (command == "{") {
            ++dynamic_block_necting_;
            if (dynamic_block_necting_ == 1) {
                response = FlushCommandBlock();
            }
        } else if (command == "}") {
            assert(dynamic_block_necting_ != 0);
            --dynamic_block_necting_;
            if (dynamic_block_necting_ == 0) {
                response = FlushCommandBlock();
            }
        } else {
            command_block_.push_back(command);
            if (dynamic_block_necting_ == 0) {
                if (command_block_.size() == max_block_size_) {
                    response = FlushCommandBlock();
                }
            }
        }
        HandleResponse(response);
    }

    void Stop() {
        std::vector<std::string> response;
        if (dynamic_block_necting_ == 0) {
            response = FlushCommandBlock();
        }
        HandleResponse(response);
    }

    void AddResponseHandler(std::shared_ptr<ResponseHandler> handler) {
        response_handlers_.push_back(std::move(handler));
    }

    void ResetResponseHandlers() {
        response_handlers_.clear();
    }

private:
    std::vector<std::string> FlushCommandBlock() {
        const auto result = std::move(command_block_);
        command_block_.clear();
        return result;
    }

    void HandleResponse(const std::vector<std::string>& response) const {
        for (auto& response_handler : response_handlers_) {
            response_handler->HandleResponse(response);
        }
    }

    std::vector<std::string> command_block_;
    int dynamic_block_necting_= 0;
    size_t max_block_size_;
    std::vector<std::shared_ptr<ResponseHandler>> response_handlers_;
};

CommandHandler::CommandHandler(size_t max_block_size) : impl_(std::make_shared<CommandHandlerImpl>(max_block_size)) {
}

CommandHandler::~CommandHandler() = default;

void CommandHandler::HandleCommand(const std::string& command) {
    impl_->HandleCommand(command);
}

void CommandHandler::Stop() {
    impl_->Stop();
}

void CommandHandler::AddResponseHandler(std::shared_ptr<ResponseHandler> handler) {
    impl_->AddResponseHandler(std::move(handler));
}

void CommandHandler::ResetResponseHandlers() {
    impl_->ResetResponseHandlers();
}
