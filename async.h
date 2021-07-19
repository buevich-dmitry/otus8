#pragma once
#include <string>
#include "bulk.h"

namespace async {

using ContextId = size_t;

void AddResponseHandler(std::shared_ptr<ResponseHandler> handler);

ContextId Connect(size_t block_size);

void Receive(const std::string& command, ContextId context_id);

void Disconnect(ContextId context_id);

void ResetResponseHandlers();

}  // namespace async
