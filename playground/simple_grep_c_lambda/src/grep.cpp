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
char *pattern;

// create a mutex
std::mutex lines_mutex;

void handle_lambda(const Aws::Lambda::LambdaClient*  /*client*/, const Aws::Lambda::Model::InvokeRequest&  /*req*/, Aws::Lambda::Model::InvokeOutcome outcome,
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
    {
      std::lock_guard<std::mutex> guard(lines_mutex);
      for (int i = 0; i < line_size; i++) {
        all_matching_lines.push_back(lines.GetItem(i).AsString());
      }

      total_matching += lines_count;
      outstanding_cnt--;
      // std::cout << "Total matching: " << total_matching << std::endl;
    }
  }
}

void handle_object(const Aws::S3::S3Client*, const Aws::S3::Model::GetObjectRequest& req, Aws::S3::Model::GetObjectOutcome outcome,
    const std::shared_ptr<const Aws::Client::AsyncCallerContext>&) {

  if (outcome.IsSuccess()) {
    auto file_name = req.GetKey();
    auto &stream = outcome.GetResult().GetBody();

    std::string line;
    std::vector<std::string> lines;

    // process line by line
    while (std::getline(stream, line)) {
      if (line.find(pattern) != std::string::npos) {
        // push file_name ":" line to the lines
        lines.push_back(file_name + ": " + line);
      }
    }

    // guard by mutex
    std::lock_guard<std::mutex> guard(lines_mutex);
    all_matching_lines.insert(all_matching_lines.end(), lines.begin(), lines.end());
    outstanding_cnt--;
    total_matching += lines.size();
  } else {
    std::cout << "Error: " << outcome.GetError().GetMessage() << std::endl;
  }
}

int main(int argc, char *argv[])
{
    int i;
    char *bucket;
    FILE *fp;
    char line[1024];

    if (argc != 5) {
        fprintf(stderr, "Usage: %s pattern directory exp_type (LAMBDA[0]/local seq[1]/local async[2]) repeat_count\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pattern = argv[1];
    bucket = argv[2];
    enum exp_type { LAMBDA = 0, LOCAL_SEQ, LOCAL_ASYNC };
    auto type = (exp_type) atoi(argv[3]);
    auto repeat_count = atoi(argv[4]);


    Aws::SDKOptions options;
    Aws::InitAPI(options);

    std::vector<std::string> file_names = find_files(bucket);

    // duplicate the file_names repeat_count times
    auto file_name_size = file_names.size();
    file_names.resize(file_name_size * repeat_count);
    for (int i = 1; i < repeat_count; i++) {
      std::copy_n(file_names.begin(), file_name_size, file_names.begin() + i * file_name_size);
    }

    std::cout << "Total number of files:" << file_names.size() << " files" << std::endl;

    /*
     * // print all file names
     * for (i = 0; i < list_size; i++) {
     *   // print file name
     *   printf("%s\n", list[i]);
     * }
     */

    Aws::Client::ClientConfiguration clientConfig;
    auto m_client = Aws::Lambda::LambdaClient(clientConfig);

    // start recording time
    auto start = std::chrono::high_resolution_clock::now();
    if (type == LAMBDA) {
      outstanding_cnt = file_names.size();
      for (auto file_name : file_names) {
        // std::cout << "Processing file: " << file_name << std::endl;
        // create a AWS lambda function to execute the following
        // 1. open the file through s3
        // 2. grep the pattern
        // 3. return the list of matching lines


        // process this asynchronously

        // invoke grep-single aws lambda function

        Aws::Lambda::Model::InvokeRequest invokeRequest;
        invokeRequest.SetFunctionName("grep-single");
        invokeRequest.SetInvocationType(Aws::Lambda::Model::InvocationType::RequestResponse);
        invokeRequest.SetLogType(Aws::Lambda::Model::LogType::Tail);
        std::shared_ptr<Aws::IOStream> payload = Aws::MakeShared<Aws::StringStream>("FunctionTest");
        Aws::Utils::Json::JsonValue jsonPayload;
        jsonPayload.WithString("file_name", file_name);
        jsonPayload.WithString("bucket", bucket);
        jsonPayload.WithString("pattern", pattern);
        *payload << jsonPayload.View().WriteReadable();
        invokeRequest.SetBody(payload);
        invokeRequest.SetContentType("application/json");

        m_client.InvokeAsync(invokeRequest, handle_lambda);
      }

      // wait for all lambda functions to finish
      while (outstanding_cnt > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
    else if (type == LOCAL_SEQ) {
      // grep all files locally
      Aws::Client::ClientConfiguration config;
      Aws::S3::S3Client s3_client(config);

      for (auto file_name : file_names) {
        // std::cout << "Processing file: " << file_name << std::endl;

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
    else if (type == LOCAL_ASYNC) {
      outstanding_cnt = file_names.size();
      // grep all files locally asynchronously
      Aws::Client::ClientConfiguration config;
      Aws::S3::S3Client s3_client(config);

      for (auto file_name : file_names) {
        // std::cout << "Processing file: " << file_name << std::endl;

        Aws::S3::Model::GetObjectRequest req;
        req.WithBucket(bucket).WithKey(file_name);

        s3_client.GetObjectAsync(req, handle_object);
      }

      // wait for all lambda functions to finish
      while (outstanding_cnt > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }

    // stop recording time
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "Time taken: " << diff.count() << " seconds" << std::endl;

    // for (auto line : all_matching_lines) {
    //   std::cout << line << std::endl;
    // }

    std::cout << "Total matching lines: " << total_matching << std::endl;

    Aws::ShutdownAPI(options);

    return 0;
}
