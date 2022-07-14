/*
 * A simple program to grep for a pattern in all files in a directory.
 */

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/S3Client.h>
#include <aws/lambda/LambdaClient.h>
#include <aws/lambda/model/CreateFunctionRequest.h>
#include <aws/lambda/model/DeleteFunctionRequest.h>
#include <aws/lambda/model/InvokeRequest.h>
#include <aws/lambda/model/ListFunctionsRequest.h>

// find all files in the S3 bucket
std::vector<std::string> find_files(char *bucket_name) {

  std::vector<std::string> file_names;

  Aws::Client::ClientConfiguration config;
  Aws::S3::S3Client s3_client(config);

  Aws::S3::Model::ListObjectsRequest request;
  request.WithBucket(bucket_name);

  auto outcome = s3_client.ListObjects(request);

  if (outcome.IsSuccess()) {
    std::cout << "Objects in bucket '" << bucket_name << "':" << std::endl
              << std::endl;

    Aws::Vector<Aws::S3::Model::Object> objects =
        outcome.GetResult().GetContents();

    for (Aws::S3::Model::Object &object : objects) {
      file_names.push_back(object.GetKey());
    }
  }
  return file_names;
}

static auto all_matching_lines = std::vector<std::string>();
static int outstanding_cnt = 0;
static unsigned total_matching = 0;

// create a mutex
std::mutex lines_mutex;


void handle(const Aws::Lambda::LambdaClient*  /*client*/, const Aws::Lambda::Model::InvokeRequest& req, Aws::Lambda::Model::InvokeOutcome outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>& /*context*/) {

  if (outcome.IsSuccess())
  {
    auto &result = outcome.GetResult();

    // Lambda function result (key1 value)
    Aws::IOStream& payload = result.GetPayload();

    std::stringstream ss;
    ss << payload.rdbuf();

    using namespace Aws::Utils::Json;
    JsonValue json(ss.str());

    auto v = json.View();
    if (!v.ValueExists("lines")) {
      std::cout << ss.str() << std::endl;
    }
    auto lines = v.GetArray("lines");
    auto lines_count = v.GetInteger("lines_count");
    int line_size = lines.GetLength();

    // guard by mutex
    std::lock_guard<std::mutex> guard(lines_mutex);
    for (int i = 0; i < line_size; i++) {
      all_matching_lines.push_back(lines.GetItem(i).AsString());
    }

    outstanding_cnt--;
    total_matching += lines_count;
  }
}


int main(int argc, char *argv[])
{
    int i;
    char *pattern;
    char *bucket;
    FILE *fp;
    char line[1024];

    if (argc != 4) {
        fprintf(stderr, "Usage: %s pattern directory LAMBDA_OR_NOT (0/1)\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pattern = argv[1];
    bucket = argv[2];
    bool LAMBDA = atoi(argv[3]);


    Aws::SDKOptions options;
    Aws::InitAPI(options);

    std::vector<std::string> file_names = find_files(bucket);

    /*
     * // print all file names
     * for (i = 0; i < list_size; i++) {
     *   // print file name
     *   printf("%s\n", list[i]);
     * }
     */

    Aws::Client::ClientConfiguration clientConfig;
    auto m_client = Aws::Lambda::LambdaClient(clientConfig);

    // grep all files
    // first degree of parallelism

    if (LAMBDA)  {
      outstanding_cnt = file_names.size() - 1;
      for (auto file_name : file_names) {
        std::cout << "Processing file: " << file_name << std::endl;
        // create a AWS lambda function to execute the following
        // 1. open the file through s3
        // 2. grep the pattern
        // 3. return the list of matching lines


        // process this asynchronously

        // don't go into enwik8
        if (file_name.find("enwik") == std::string::npos) {
          // invoke grep-single aws lambda function

          Aws::Lambda::Model::InvokeRequest invokeRequest;
          invokeRequest.SetFunctionName("grep-single");
          invokeRequest.SetInvocationType(Aws::Lambda::Model::InvocationType::RequestResponse);
          invokeRequest.SetLogType(Aws::Lambda::Model::LogType::Tail);
          std::shared_ptr<Aws::IOStream> payload = Aws::MakeShared<Aws::StringStream>("FunctionTest");
          Aws::Utils::Json::JsonValue jsonPayload;
          jsonPayload.WithString("file_name", file_name);
          jsonPayload.WithString("bucket", bucket);
          jsonPayload.WithString("pattern", "hello");
          *payload << jsonPayload.View().WriteReadable();
          invokeRequest.SetBody(payload);
          invokeRequest.SetContentType("application/json");

          m_client.InvokeAsync(invokeRequest, handle);
        }
      }

      // wait for all lambda functions to finish
      while (outstanding_cnt > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
    else {
      // grep all files locally
      Aws::Client::ClientConfiguration config;
      // config.caFile = "/etc/pki/tls/certs/ca-bundle.crt"; 
      Aws::S3::S3Client s3_client(config);

      for (auto file_name : file_names) {
        std::cout << "Processing file: " << file_name << std::endl;
        // don't go into enwik8
        if (file_name.find("enwik") == std::string::npos) {

          Aws::S3::Model::GetObjectRequest req;
          req.WithBucket(bucket).WithKey(file_name);

          auto outcome = s3_client.GetObject(req);

          // a list of lines
          std::vector<std::string> lines;
          if (outcome.IsSuccess()) {
            auto &stream = outcome.GetResult().GetBody();

            // process line by line
            std::string line;
            // second degree of parallelism
            while (std::getline(stream, line)) {
              if (line.find(pattern) != std::string::npos) {
                total_matching++;
                // push file_name ":" line to the lines
                all_matching_lines.push_back(file_name + ": " + line);
              }
            }
          } else {
            std::cout << "Error: " << outcome.GetError().GetMessage() << std::endl;
          }
        }
      }
    }

    for (auto line : all_matching_lines) {
      std::cout << line << std::endl;
    }

    std::cout << "Total matching lines: " << total_matching << std::endl;

    Aws::ShutdownAPI(options);

    return 0;
}
