#pragma once

#include <memory>

#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"

#include "envoy/event/dispatcher.h"
#include "envoy/extensions/access_loggers/grpc/v3/als.pb.h"
#include "envoy/grpc/async_client_manager.h"
#include "envoy/local_info/local_info.h"
#include "envoy/thread_local/thread_local.h"

#include "extensions/access_loggers/common/grpc_access_logger.h"

namespace Envoy {
namespace Extensions {
namespace AccessLoggers {
namespace GrpcCommon {

// Note: OpenTelemetry protos are extra flexible and used also in the OT collector for batching and
// so forth. As a result, some fields are repeated, but for our use case we assume the following
// structure:
// ExportLogsServiceRequest -> (single) ResourceLogs -> (single) InstrumentationLibrary ->
// (repeated) LogRecord.
class GrpcOpenTelemetryAccessLoggerImpl
    : public Common::GrpcAccessLogger<
          opentelemetry::proto::logs::v1::LogRecord,
          opentelemetry::proto::logs::v1::ResourceLogs /*TCP*/,
          opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest,
          opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse> {
public:
  GrpcOpenTelemetryAccessLoggerImpl(Grpc::RawAsyncClientPtr&& client, std::string log_name,
                                    std::chrono::milliseconds buffer_flush_interval_msec,
                                    uint64_t max_buffer_size_bytes, Event::Dispatcher& dispatcher,
                                    const LocalInfo::LocalInfo& local_info, Stats::Scope& scope,
                                    envoy::config::core::v3::ApiVersion transport_api_version);

private:
  void initMessageRoot(const std::string& log_name, const LocalInfo::LocalInfo& local_info);
  // Extensions::AccessLoggers::GrpcCommon::GrpcAccessLogger
  void addEntry(opentelemetry::proto::logs::v1::LogRecord&& entry) override;
  void addEntry(opentelemetry::proto::logs::v1::ResourceLogs&& entry) override;
  bool isEmpty() override;
  void initMessage() override;
  void clearMessage() override;

  opentelemetry::proto::logs::v1::InstrumentationLibraryLogs* root_;
};

class GrpcOpenTelemetryAccessLoggerCacheImpl
    : public Common::GrpcAccessLoggerCache<
          GrpcOpenTelemetryAccessLoggerImpl,
          envoy::extensions::access_loggers::grpc::v3::CommonGrpcAccessLogConfig> {
public:
  GrpcOpenTelemetryAccessLoggerCacheImpl(Grpc::AsyncClientManager& async_client_manager,
                                         Stats::Scope& scope, ThreadLocal::SlotAllocator& tls,
                                         const LocalInfo::LocalInfo& local_info);

private:
  // Common::GrpcAccessLoggerCache
  GrpcOpenTelemetryAccessLoggerImpl::SharedPtr
  createLogger(const envoy::extensions::access_loggers::grpc::v3::CommonGrpcAccessLogConfig& config,
               Grpc::RawAsyncClientPtr&& client,
               std::chrono::milliseconds buffer_flush_interval_msec, uint64_t max_buffer_size_bytes,
               Event::Dispatcher& dispatcher, Stats::Scope& scope) override;

  const LocalInfo::LocalInfo& local_info_;
};

/**
 * Aliases for class interfaces for mock definitions.
 */
using GrpcOpenTelemetryAccessLogger = GrpcOpenTelemetryAccessLoggerImpl::Interface;
using GrpcOpenTelemetryAccessLoggerSharedPtr = GrpcOpenTelemetryAccessLogger::SharedPtr;

using GrpcOpenTelemetryAccessLoggerCache = GrpcOpenTelemetryAccessLoggerCacheImpl::Interface;
using GrpcOpenTelemetryAccessLoggerCacheSharedPtr = GrpcOpenTelemetryAccessLoggerCache::SharedPtr;

} // namespace GrpcCommon
} // namespace AccessLoggers
} // namespace Extensions
} // namespace Envoy
