#include <cthun-agent/agent.hpp>
#include <cthun-agent/errors.hpp>
#include <cthun-agent/rpc_schemas.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.agent"
#include <leatherman/logging/logging.hpp>

#include <vector>

namespace CthunAgent {

static const std::string AGENT_CLIENT_TYPE { "agent" };

Agent::Agent(const std::string& modules_dir,
             const std::string& server_url,
             const std::string& ca,
             const std::string& crt,
             const std::string& key)
        try
            : connector_ptr_ { new CthunConnector(server_url, AGENT_CLIENT_TYPE,
                                                  ca, crt, key) },
              request_processor_ { connector_ptr_, modules_dir } {
} catch (CthunClient::connection_config_error& e) {
    throw fatal_error { std::string { "failed to configure: " } + e.what() };
}

void Agent::start() {
    // TODO(ale): add associate response callback

    connector_ptr_->registerMessageCallback(
        RPCSchemas::BlockingRequestSchema(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            blockingRequestCallback(parsed_chunks);
        });

    connector_ptr_->registerMessageCallback(
        RPCSchemas::NonBlockingRequestSchema(),
        [this](const CthunClient::ParsedChunks& parsed_chunks) {
            nonBlockingRequestCallback(parsed_chunks);
        });

    try {
        connector_ptr_->connect();
    } catch (CthunClient::connection_config_error& e) {
        LOG_ERROR("Failed to configure the underlying communications layer: %1%",
                  e.what());
        throw fatal_error { "failed to configure the underlying communications"
                            "layer" };
    } catch (CthunClient::connection_fatal_error& e) {
        LOG_ERROR("Failed to connect: %1%", e.what());
        throw fatal_error { "failed to connect" };
    }

    // The agent is now connected and the request handlers are set;
    // we can now call the monitoring method that will block this
    // thread of execution.
    // Note that, in case the underlying connection drops, the
    // connector will keep trying to re-establish it indefinitely
    // (the max_connect_attempts is 0 by default).
    try {
        connector_ptr_->monitorConnection();
    } catch (CthunClient::connection_fatal_error) {
        throw fatal_error { "failed to reconnect" };
    }
}

void Agent::blockingRequestCallback(
                const CthunClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::Blocking, parsed_chunks);
}

void Agent::nonBlockingRequestCallback(
                const CthunClient::ParsedChunks& parsed_chunks) {
    request_processor_.processRequest(RequestType::NonBlocking, parsed_chunks);
}

}  // namespace CthunAgent