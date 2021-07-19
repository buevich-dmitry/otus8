#include "bulk.h"
#include "async.h"
#include "response_handler.h"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

std::string MakeBulkFileName(const std::string& suffix) {
    return "bulk" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + suffix + ".log";
}

int main(int ac, char** av) {
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("block-size", po::value<size_t>())
    ;
    po::positional_options_description pos_desc;
    pos_desc.add("block-size", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(ac, av).
            options(desc).positional(pos_desc).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    if (!vm.count("block-size")) {
        std::cout << "You must specify block size" << std::endl;
        std::terminate();
    }

    async::AddResponseHandler(MakeOstreamResponseHandler(std::cout));
    async::AddResponseHandler(MakeFileResponseHandler(MakeBulkFileName("_1")));
    async::AddResponseHandler(MakeFileResponseHandler(MakeBulkFileName("_2")));

    const auto context_id = async::Connect(vm["block-size"].as<size_t>());
    std::string command;
    while (std::cin >> command) {
        if (command == ":stop") {
            break;
        }
        async::Receive(command, context_id);
    }
    async::Disconnect(context_id);

    return 0;
}
