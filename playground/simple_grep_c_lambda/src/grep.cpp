/*
 * A simple program to grep for a pattern in all files in a directory.
 */


#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/S3Client.h>

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

int main(int argc, char *argv[])
{
    int i;
    char *pattern;
    char *bucket;
    FILE *fp;
    char line[1024];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s pattern directory\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pattern = argv[1];
    bucket = argv[2];


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

    // grep all files
    for (auto file_name : file_names) {
      Aws::Client::ClientConfiguration config;
      Aws::S3::S3Client s3_client(config);

      Aws::S3::Model::GetObjectRequest request;
      request.WithBucket(bucket).WithKey(file_name);

      auto outcome = s3_client.GetObject(request);

      if (outcome.IsSuccess()) {
        auto &stream = outcome.GetResultWithOwnership().GetBody();

        // process line by line
        std::string line;
        while (std::getline(stream, line)) {
          if (line.find(pattern) != std::string::npos) {
            std::cout << file_name << ": " << line << std::endl;
          }
        }
      }
    }

    Aws::ShutdownAPI(options);

    return 0;
}
