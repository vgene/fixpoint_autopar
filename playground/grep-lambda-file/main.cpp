// main.cpp
#include <aws/lambda-runtime/runtime.h>
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/utils/Array.h>
#include <aws/core/platform/Environment.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/lambda-runtime/runtime.h>
#include <iostream>
#include <memory>

using namespace aws::lambda_runtime;

invocation_response my_handler(invocation_request const& request)
{
  using namespace Aws::Utils::Json;
    JsonValue json(request.payload);

  auto v = json.View();

  // get pattern, bucket, and file_name from the request
  // if not exist, quite
  if (!v.ValueExists("pattern") || !v.ValueExists("bucket") || !v.ValueExists("file_name")) {
    return invocation_response::failure("Pattern or bucket or file_name is not specified", "InvalidJSON");
  }
  // create logging
  AWS_LOGSTREAM_INFO("dbg", "args found");
  auto pattern = v.GetString("pattern");
  auto bucket = v.GetString("bucket");
  auto file_name = v.GetString("file_name");

  // create logging
  AWS_LOGSTREAM_INFO("dbg", "args extracted");
  
  Aws::SDKOptions options;
  Aws::InitAPI(options);

  Aws::Client::ClientConfiguration config;
  config.caFile = "/etc/pki/tls/certs/ca-bundle.crt"; 
  Aws::S3::S3Client s3_client(config);

  Aws::S3::Model::GetObjectRequest req;
  req.WithBucket(bucket).WithKey(file_name);

  auto outcome = s3_client.GetObject(req);
  AWS_LOGSTREAM_INFO("dbg", "object downloaded");

  // a list of lines
  std::vector<std::string> lines;
  int total_lines = 0;
  if (outcome.IsSuccess()) {
    auto &stream = outcome.GetResult().GetBody();

    // process line by line
    std::string line;
    // second degree of parallelism
    while (std::getline(stream, line)) {
      total_lines++;
      if (line.find(pattern) != std::string::npos) {
        // push file_name ":" line to the lines
        lines.push_back(file_name + ": " + line);
      }
    }
  }
  else {
    // format the outcome error
    auto error = outcome.GetError();

    return invocation_response::failure(error.GetMessage(), "InvalidJSON");
  }

  Aws::Utils::Array<Aws::String> aws_lines(lines.size());
  for (int i = 0; i < lines.size(); i++) {
    aws_lines[i] = lines[i];
  }
  JsonValue json_lines;
  json_lines.WithInteger("total_lines", total_lines);
  json_lines.WithArray("lines", aws_lines);
  json_lines.WithInteger("lines_count", lines.size());
  json_lines.WithString("pattern", pattern);
  Aws::ShutdownAPI(options);
  return invocation_response::success(json_lines.View().WriteReadable(), "application/json");
}

int main()
{
   run_handler(my_handler);
   return 0;
}
