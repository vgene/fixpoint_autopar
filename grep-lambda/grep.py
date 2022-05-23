import boto3
# Access S3 from AWS lambda
def lambda_handler(event, context):
    s3 = boto3.client('s3')
    # bucket name is "fixpoint-s3"
    # get the bucket name and the search keyword from event
    bucket_name = event['bucket']
    keyword = event['keyword']

    found_list = []
    # go over all the files in the bucket
    for obj in s3.list_objects(Bucket=bucket_name)['Contents']:
        # get the file name
        key = obj['Key']
        # get the file content
        content = s3.get_object(Bucket=bucket_name, Key=key)['Body'].read().decode('utf-8')

        # check if the keyword is in the file content, line by line
        for line in content.splitlines():
            if keyword in line:
                found_list.append(key + ":" + line)

    # format as lambda response
    response = {
        "statusCode": 200,
        "body": {
            "found": found_list
        }
    }

    return response
