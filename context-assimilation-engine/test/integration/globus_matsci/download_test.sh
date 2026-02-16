#!/bin/bash
#
# download_test.sh - Test downloading from Globus endpoint using HTTPS API
#
# This script demonstrates downloading files from a Globus endpoint using
# the Globus HTTPS API with an access token.
#
# Prerequisites:
# - GLOBUS_ACCESS_TOKEN environment variable must be set
#
# Usage:
#   export GLOBUS_ACCESS_TOKEN="your_token_here"
#   ./download_test.sh

set -e  # Exit on error

# Globus endpoint details (extracted from the web URL)
ENDPOINT_ID="e8cf0e9a-f96a-11ed-9a83-83ef71fbf0ae"
REMOTE_PATH="/FINAL_CYCLE_FILES/centroid_data_SEM_103_keyhole.txt"  # URL-decoded from %2FFINAL_CYCLE_FILES%2F

echo "========================================="
echo "Globus HTTPS Download Test"
echo "========================================="
echo ""

# Check for Globus access token
if [ -z "${GLOBUS_ACCESS_TOKEN}" ]; then
    echo "ERROR: GLOBUS_ACCESS_TOKEN environment variable is not set"
    echo ""
    echo "To obtain a Globus access token:"
    echo "1. Visit: https://app.globus.org/settings/developers"
    echo "2. Create a new app or use an existing one"
    echo "3. Generate a new access token"
    echo "4. Export it: export GLOBUS_ACCESS_TOKEN='your_token_here'"
    echo ""
    exit 1
fi

echo "Configuration:"
echo "  Endpoint ID:  ${ENDPOINT_ID}"
echo "  Remote Path:  ${REMOTE_PATH}"
echo ""

# Step 1: Get endpoint details to find the HTTPS server
echo "Step 1: Getting endpoint details..."
ENDPOINT_DETAILS=$(curl -s -X GET \
  "https://transfer.api.globus.org/v0.10/endpoint/${ENDPOINT_ID}" \
  -H "Authorization: Bearer ${GLOBUS_ACCESS_TOKEN}")

echo "Endpoint details response:"
echo "${ENDPOINT_DETAILS}" | jq '.' || echo "${ENDPOINT_DETAILS}"
echo ""

# Step 2: Extract HTTPS server URL
HTTPS_SERVER=$(echo "${ENDPOINT_DETAILS}" | jq -r '.https_server // empty')

if [ -z "${HTTPS_SERVER}" ]; then
    echo "ERROR: Could not find https_server in endpoint details"
    echo "This endpoint may not have HTTPS access enabled"
    exit 1
fi

echo "Step 2: Found HTTPS server: ${HTTPS_SERVER}"
echo ""

# Step 3: List files in the directory
echo "Step 3: Listing files in ${REMOTE_PATH}..."
FILE_LIST_URL="${HTTPS_SERVER}${REMOTE_PATH}"
echo "Requesting: ${FILE_LIST_URL}"
echo ""

curl -v -X GET \
  "${FILE_LIST_URL}" \
  -H "Authorization: Bearer ${GLOBUS_ACCESS_TOKEN}"

echo ""
echo "========================================="
echo "Test Complete"
echo "========================================="
