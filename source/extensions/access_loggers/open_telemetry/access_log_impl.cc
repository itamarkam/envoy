#include "extensions/access_loggers/open_telemetry/access_log_impl.h"

#include "envoy/config/core/v3/base.pb.h"
#include "envoy/data/accesslog/v3/accesslog.pb.h"
#include "envoy/extensions/access_loggers/grpc/v3/als.pb.h"
#include "envoy/extensions/access_loggers/open_telemetry/v3/open_telemetry.pb.h"

#include "common/common/assert.h"
#include "common/formatter/substitution_formatter.h"
#include "common/http/headers.h"
#include "common/network/utility.h"
#include "common/protobuf/message_validator_impl.h"
#include "common/protobuf/utility.h"
#include "common/stream_info/utility.h"

// #include "extensions/access_loggers/grpc/grpc_access_log_utils.h"

// #include "external/opentelemetry_proto/opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"

// const char BODY_CONFIG[] = R"EOF(
// kvlist_value:
//   values:
//     - key: "downstream_service_id"
//       value:
//         string_value: "%REQ(x-envoy-downstream-service-cluster)%"
//     - key: "upstream_service_id"
//       value:
//         string_value: "DYNAMIC_METADATA(com.google.trafficdirector:backend_service_name)"
//     - key: "upstream_backend_service"
//       value:
//         string_value: "DYNAMIC_METADATA(com.google.trafficdirector:backend_service_name)"
//     - key: "protocol"
//       value:
//         string_value: "%PROTOCOL%"
//     - key: "status_code"
//       value:
//         string_value: "%RESPONSE_CODE%"
//     - key: "duration"
//       value:
//         string_value: "%REQUEST_DURATION%"
//     - key: "request_bytes"
//       value:
//         string_value: "%BYTES_RECEIVED%"
//     - key: "response_bytes"
//       value:
//         string_value: "%BYTES_SENT%"
// )EOF";

namespace Envoy {
namespace Extensions {
namespace AccessLoggers {
namespace OpenTelemetry {

Http::RegisterCustomInlineHeader<Http::CustomInlineHeaderRegistry::Type::RequestHeaders>
    referer_handle(Http::CustomHeaders::get().Referer);

AccessLog::ThreadLocalLogger::ThreadLocalLogger(GrpcAccessLoggerSharedPtr logger)
    : logger_(std::move(logger)) {}

AccessLog::AccessLog(
    ::Envoy::AccessLog::FilterPtr&& filter,
    envoy::extensions::access_loggers::open_telemetry::v3::OpenTelemetryAccessLogConfig config,
    ThreadLocal::SlotAllocator& tls, GrpcAccessLoggerCacheSharedPtr access_logger_cache,
    Stats::Scope& scope)
    : Common::ImplBase(std::move(filter)), scope_(scope), tls_slot_(tls.allocateSlot()),
      access_logger_cache_(std::move(access_logger_cache)) {

  tls_slot_->set([this, config](Event::Dispatcher&) {
    return std::make_shared<ThreadLocalLogger>(access_logger_cache_->getOrCreateLogger(
        config.common_config(), Common::GrpcAccessLoggerType::HTTP, scope_));
  });

  ProtobufWkt::Struct body_format;
  MessageUtil::jsonConvert(config.body(), body_format);
  body_formatter_ = std::make_unique<Formatter::StructFormatter>(body_format, false, false);
  ProtobufWkt::Struct attributes_format;
  MessageUtil::jsonConvert(config.attributes(), attributes_format);
  attributes_formatter_ =
      std::make_unique<Formatter::StructFormatter>(attributes_format, false, false);
}

void AccessLog::emitLog(const Http::RequestHeaderMap& request_headers,
                        const Http::ResponseHeaderMap& response_headers,
                        const Http::ResponseTrailerMap& response_trailers,
                        const StreamInfo::StreamInfo& stream_info) {

  opentelemetry::proto::logs::v1::LogRecord log_entry;
  const auto formatted_body = body_formatter_->format(
      request_headers, response_headers, response_trailers, stream_info, absl::string_view());
  MessageUtil::jsonConvert(formatted_body, ProtobufMessage::getNullValidationVisitor(),
                           *log_entry.mutable_body());
  const auto formatted_attributes = attributes_formatter_->format(
      request_headers, response_headers, response_trailers, stream_info, absl::string_view());
  opentelemetry::proto::common::v1::KeyValueList attributes;
  MessageUtil::jsonConvert(formatted_attributes, ProtobufMessage::getNullValidationVisitor(),
                           attributes);
  *log_entry.mutable_attributes() = attributes.values();

  tls_slot_->getTyped<ThreadLocalLogger>().logger_->log(std::move(log_entry));
}

} // namespace OpenTelemetry
} // namespace AccessLoggers
} // namespace Extensions
} // namespace Envoy