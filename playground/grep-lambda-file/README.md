## Steps to build

1. Follow [AWS Guide](https://aws.amazon.com/blogs/compute/introducing-the-c-lambda-runtime/) to compile the runtime
2. Build the code locally
    ``` bash
    mkdir build; cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1
    make;  make aws-lambda-package-hello
    ```
3. Create lambda function
    ``` bash
    aws lambda create-function \
    --function-name grep-single \
    --role {arn of role} \
    --runtime provided \
    --timeout 15 \
    --memory-size 512 \
    --handler hello \
    --zip-file fileb://hello.zip
    ```
4. Test function by `aws lambda invoke`
