#!/usr/bin/env bash
set -e

TAG_NAME="$1"
PROJECT_ID="$2"
DESCRIPTION_FILE_PATH="$3"
ASSET_URL="$4"
PRIVATE_TOKEN="$5"


if [ "$5" == "" ]; then
    echo "Missing parameter! Parameters are TAG_NAME, PROJECT_ID, DESCRIPTION_FILE_PATH and PRIVATE_TOKEN.";
    exit 1;
fi

DESCRIPTION=''

# Load data from file
while read -r line; do
    DESCRIPTION="${DESCRIPTION}${line}\n";
done < "${DESCRIPTION_FILE_PATH}"

curl --request POST\
     --header 'Content-Type: application/json'\
     --header "Private-Token: ${PRIVATE_TOKEN}"\
     --data-binary "{\"name\": \"${TAG_NAME}\", \"tag_name\": \"${TAG_NAME}\", \"description\": \"${DESCRIPTION}\", \"assets\": { \"links\": [{ \"name\": \"package\", \"url\": \"${ASSET_URL}\", \"link_type\":\"package\" }] } }"\
     "https://git.list.family/api/v4/projects/${PROJECT_ID}/releases" 

