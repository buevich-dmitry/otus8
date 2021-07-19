#define BOOST_TEST_MODULE test_bulk

#include "bulk.h"
#include <set>
#include <boost/filesystem.hpp>

#include <boost/test/unit_test.hpp>

namespace fs = boost::filesystem;

BOOST_AUTO_TEST_SUITE(test_response_handler)

void TestResponseHandler(ResponseHandler* handler, std::function<std::string()> get_handler_output) {
    std::string expected_output;

    BOOST_CHECK_EQUAL(expected_output, get_handler_output());

    handler->HandleResponse({});
    BOOST_CHECK_EQUAL(expected_output, get_handler_output());

    handler->HandleResponse({"cmd1"});
    expected_output += "bulk: cmd1\n";
    BOOST_CHECK_EQUAL(expected_output, get_handler_output());

    handler->HandleResponse({"cmd2", "cmd3", "cmd4"});
    expected_output += "bulk: cmd2, cmd3, cmd4\n";
    BOOST_CHECK_EQUAL(expected_output, get_handler_output());

    handler->HandleResponse({});
    BOOST_CHECK_EQUAL(expected_output, get_handler_output());

    handler->HandleResponse({"cmd5", "cmd6"});
    expected_output += "bulk: cmd5, cmd6\n";
    BOOST_CHECK_EQUAL(expected_output, get_handler_output());
}

BOOST_AUTO_TEST_CASE(test_OstreamResponseHandler) {
    std::stringstream ss;
    std::shared_ptr<ResponseHandler> handler = MakeOstreamResponseHandler(ss);

    TestResponseHandler(handler.get(), [&ss]() { return ss.str(); });
}

BOOST_AUTO_TEST_CASE(test_FileResponseHandler) {
    std::string file_name = "test_file.log";
    if (fs::exists(file_name)) {
        fs::remove(file_name);
    }
    std::shared_ptr<ResponseHandler> handler = MakeFileResponseHandler(file_name);

    TestResponseHandler(handler.get(), [&file_name]() {
        BOOST_CHECK(fs::exists(file_name));
        std::ifstream file{file_name};
        BOOST_CHECK(file.good());
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    });
}

}


BOOST_AUTO_TEST_SUITE(test_response_handler)

class CheckResponseHandler : public ResponseHandler {
public:
    void HandleResponse(const std::vector<std::string>& response) override {
        BOOST_CHECK(expected_response_ == response);
        is_response_checked_ = true;
    }

    void SetExpectedResponse(const std::vector<std::string>& response) {
        expected_response_ = response;
        is_response_checked_ = false;
    }

    bool IsResponseChecked() {
        return is_response_checked_;
    }

private:
    std::vector<std::string> expected_response_;
    bool is_response_checked_ = false;
};

std::pair<CommandHandler, std::shared_ptr<CheckResponseHandler>> MakeCommandHandler(size_t block_size) {
    CommandHandler handler{block_size};
    std::shared_ptr<CheckResponseHandler> response_handler = std::make_shared<CheckResponseHandler>();
    handler.AddResponseHandler(response_handler);
    return {std::move(handler), response_handler};
}

void TestCommand(CommandHandler& handler, const std::shared_ptr<CheckResponseHandler>& response_handler,
                 const std::string& command, const std::vector<std::string>& expected_response) {
    response_handler->SetExpectedResponse(expected_response);
    handler.HandleCommand(command);
    BOOST_CHECK(response_handler->IsResponseChecked());
}

void TestStopCommand(CommandHandler& handler, const std::shared_ptr<CheckResponseHandler>& response_handler,
                     const std::vector<std::string>& expected_response) {
    response_handler->SetExpectedResponse(expected_response);
    handler.Stop();
    BOOST_CHECK(response_handler->IsResponseChecked());
}

BOOST_AUTO_TEST_CASE(test_CommandHandler_1) {
    auto [handler, check_response_handler] = MakeCommandHandler(3);
    TestCommand(handler, check_response_handler, "cmd1", {});
    TestCommand(handler, check_response_handler, "cmd2", {});
    TestCommand(handler, check_response_handler, "cmd3", {"cmd1", "cmd2", "cmd3"});
    TestCommand(handler, check_response_handler, "cmd4", {});
    TestCommand(handler, check_response_handler, "cmd5", {});
    TestStopCommand(handler, check_response_handler, {"cmd4", "cmd5"});
}

BOOST_AUTO_TEST_CASE(test_CommandHandler_2) {
    auto [handler, check_response_handler] = MakeCommandHandler(3);
    TestCommand(handler, check_response_handler, "cmd1", {});
    TestCommand(handler, check_response_handler, "cmd2", {});
    TestCommand(handler, check_response_handler, "{", {"cmd1", "cmd2"});
    TestCommand(handler, check_response_handler, "cmd3", {});
    TestCommand(handler, check_response_handler, "cmd4", {});
    TestCommand(handler, check_response_handler, "}", {"cmd3", "cmd4"});
    TestCommand(handler, check_response_handler, "{", {});
    TestCommand(handler, check_response_handler, "cmd5", {});
    TestCommand(handler, check_response_handler, "cmd6", {});
    TestCommand(handler, check_response_handler, "{", {});
    TestCommand(handler, check_response_handler, "cmd7", {});
    TestCommand(handler, check_response_handler, "cmd8", {});
    TestCommand(handler, check_response_handler, "}", {});
    TestCommand(handler, check_response_handler, "cmd9", {});
    TestCommand(handler, check_response_handler, "}", {"cmd5", "cmd6", "cmd7", "cmd8", "cmd9"});
    TestStopCommand(handler, check_response_handler, {});
}

}