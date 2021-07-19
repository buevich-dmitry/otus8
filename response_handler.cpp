#include "response_handler.h"
#include <iostream>
#include <fstream>

class AbstractOstreamResponseHandler : public ResponseHandler {
public:
    void HandleResponse(const std::vector<std::string>& response) override {
        auto& out = GetOstream();
        assert(out.good());
        if (response.empty()) {
            return;
        }
        out << "bulk: ";
        bool first = true;
        for (const auto& command : response) {
            if (!first) {
                out << ", ";
            }
            first = false;
            out << command;
        }
        out << std::endl;
    }

protected:
    virtual std::ostream& GetOstream() = 0;
};

class OstreamResponseHandler : public AbstractOstreamResponseHandler {
public:
    explicit OstreamResponseHandler(std::ostream& out) : out_(out) {
    }

protected:
    std::ostream& GetOstream() override {
        return out_;
    }

private:
    std::ostream& out_;
};

class FileResponseHandler : public AbstractOstreamResponseHandler {
public:
    explicit FileResponseHandler(std::string file_name) : file_(std::move(file_name)) {
    }

protected:
    std::ostream& GetOstream() override {
        return file_;
    }

private:
    std::ofstream file_;
};

std::shared_ptr<ResponseHandler> MakeOstreamResponseHandler(std::ostream& out) {
    return std::make_shared<OstreamResponseHandler>(out);
}

std::shared_ptr<ResponseHandler> MakeFileResponseHandler(const std::string& file_name) {
    return std::make_shared<FileResponseHandler>(file_name);
}