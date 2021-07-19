#define BOOST_TEST_MODULE test_async

#include "async.h"
#include <set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/test/unit_test.hpp>

namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(test_async)

class CheckResponseHandler : public ResponseHandler {
public:
    void HandleResponse(const Response& response) override {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this] { return !expected_responses_.empty(); });
        const auto expected = std::move(expected_responses_.front());
        expected_responses_.pop();
        assert(expected == response);
        cv_.notify_all();
    }

    void AddExpectedResponse(const Response& response) {
        std::lock_guard lock{mutex_};
        expected_responses_.push(response);
        cv_.notify_all();
    }

    bool IsResponseChecked() {
        std::unique_lock lock{mutex_};
        cv_.wait_for(lock, std::chrono::milliseconds(500), [this] { return expected_responses_.empty(); });
        return expected_responses_.empty();
    }

private:
    std::queue<Response> expected_responses_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

std::pair<async::ContextId, std::shared_ptr<CheckResponseHandler>> ConnectToContext(size_t block_size) {
    std::shared_ptr<CheckResponseHandler> response_handler = std::make_shared<CheckResponseHandler>();
    async::AddResponseHandler(response_handler);
    auto context_id = async::Connect(block_size);
    return {context_id, response_handler};
}

void TestCommand(async::ContextId context_id, const std::shared_ptr<CheckResponseHandler>& response_handler,
                 const std::string& command, const Response& expected_response) {
    response_handler->AddExpectedResponse(expected_response);
    async::Receive(command, context_id);
    assert(response_handler->IsResponseChecked());
}

void TestStopCommand(async::ContextId context_id, const std::shared_ptr<CheckResponseHandler>& response_handler,
                     const std::vector<std::string>& expected_response) {
    response_handler->AddExpectedResponse(expected_response);
    async::Disconnect(context_id);
    assert(response_handler->IsResponseChecked());
}

BOOST_AUTO_TEST_CASE(test_simple) {
    static constexpr size_t kTestCount = 100;
    for (size_t i = 0; i < kTestCount; ++i) {
        async::ResetResponseHandlers();
        const auto[context_id, check_response_handler] = ConnectToContext(3);
        TestCommand(context_id, check_response_handler, "cmd1", {});
        TestCommand(context_id, check_response_handler, "cmd2", {});
        TestCommand(context_id, check_response_handler, "{", {"cmd1", "cmd2"});
        TestCommand(context_id, check_response_handler, "cmd3", {});
        TestCommand(context_id, check_response_handler, "cmd4", {});
        TestCommand(context_id, check_response_handler, "}", {"cmd3", "cmd4"});
        TestCommand(context_id, check_response_handler, "{", {});
        TestCommand(context_id, check_response_handler, "cmd5", {});
        TestCommand(context_id, check_response_handler, "cmd6", {});
        TestCommand(context_id, check_response_handler, "{", {});
        TestCommand(context_id, check_response_handler, "cmd7", {});
        TestCommand(context_id, check_response_handler, "cmd8", {});
        TestCommand(context_id, check_response_handler, "}", {});
        TestCommand(context_id, check_response_handler, "cmd9", {});
        TestCommand(context_id, check_response_handler, "}", {"cmd5", "cmd6", "cmd7", "cmd8", "cmd9"});
        TestStopCommand(context_id, check_response_handler, {});
    }
}

}

BOOST_AUTO_TEST_SUITE(stress_test_async)

static constexpr size_t kMainThreadCount = 3;
static constexpr size_t kContextCount = 3;
static constexpr size_t kResponseHandlerCount = 3;
static constexpr size_t kCommandCount = 5000;
static constexpr size_t kBlockSize = 10;

std::string MakeCommand(size_t thread_index, async::ContextId context_id, size_t command_index) {
    return "cmd_" + std::to_string(thread_index) + "_" + std::to_string(context_id) + "_" +
           std::to_string(command_index);
}

class StressCheckResponseHandler : public ResponseHandler {
public:
    StressCheckResponseHandler(std::array<async::ContextId, kContextCount> context_ids, size_t thread_count) {
        for (size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
            for (const auto context_id : context_ids) {
                command_count_by_thread_and_context_id_.emplace(std::make_pair(thread_index, context_id), 0);
            }
        }
    }

    void HandleResponse(const Response& response) override {
        static const boost::regex xRegEx("cmd_(\\d+)_(\\d+)_(\\d+)");
        for (const auto& response_part : response) {
            boost::smatch match_result;
            assert(boost::regex_match(response_part, match_result, xRegEx));
            size_t thread_index = std::stoll(match_result[1].str());
            async::ContextId context_id = std::stoll(match_result[2].str());
            size_t command_index = std::stoll(match_result[3].str());
            std::pair<size_t, async::ContextId> key{thread_index, context_id};
            assert(command_count_by_thread_and_context_id_.at(key) == command_index);
            ++command_count_by_thread_and_context_id_.at(key);
        }
    }

    void CheckExpectedCommandCount(size_t expected_command_count) const {
        for (const auto [_, command_count] : command_count_by_thread_and_context_id_) {
            std::ignore = _;
            assert(command_count == expected_command_count);
        }
    }

private:
    std::map<std::pair<size_t, async::ContextId>, size_t> command_count_by_thread_and_context_id_;
};

BOOST_AUTO_TEST_CASE(stress_test) {

    async::ResetResponseHandlers();

    std::array<async::ContextId, kContextCount> context_ids{};
    for (auto& context_id : context_ids) {
        context_id = async::Connect(kBlockSize);
    }

    std::array<std::shared_ptr<StressCheckResponseHandler>, kResponseHandlerCount> response_handlers;
    for (size_t i = 0; i < kResponseHandlerCount; ++i) {
        response_handlers[i] = std::make_shared<StressCheckResponseHandler>(context_ids, kMainThreadCount);
        async::AddResponseHandler(response_handlers[i]);
    }

    std::array<std::thread, kMainThreadCount> main_threads;
    for (size_t thread_index = 0; thread_index < main_threads.size(); ++thread_index) {
        main_threads[thread_index] = std::thread([thread_index, &context_ids] {
            for (size_t cmd_index = 0; cmd_index < kCommandCount; ++cmd_index) {
                for (const auto context_id : context_ids) {
                    async::Receive(MakeCommand(thread_index, context_id, cmd_index), context_id);
                }
            }
        });
    }
    for (auto& thread : main_threads) {
        thread.join();
    }

    for (const auto context_id : context_ids) {
        async::Disconnect(context_id);
    }

    for (const auto& response_handler : response_handlers) {
        response_handler->CheckExpectedCommandCount(kCommandCount);
    }
}

}
