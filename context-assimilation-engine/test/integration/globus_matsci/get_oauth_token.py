#!/usr/bin/env python3
"""
get_oauth_token.py - Get Globus OAuth2 tokens programmatically

This demonstrates how to get Globus access tokens using the OAuth2 flow
for REST API access to download data.

For production use, you should:
1. Register a Native App or Confidential Client at https://app.globus.org/settings/developers
2. Use the client ID and implement proper OAuth2 flow

This script uses the Globus SDK's Native App flow.
"""

import sys
import globus_sdk

def get_tokens_for_collection(collection_id):
    """
    Get OAuth2 tokens for Transfer API and HTTPS access to a collection.

    Args:
        collection_id: The Globus collection/endpoint ID

    Returns:
        Dictionary with access tokens
    """

    # Use the Globus SDK's tutorial client ID (for demonstration only)
    # For production, register your own app at https://app.globus.org/settings/developers
    CLIENT_ID = "61338d24-54d5-408f-a10d-66c06b59f6d2"  # Tutorial Native App client ID

    # Define the scopes we need
    # 1. Transfer API scope - for getting endpoint details
    transfer_scope = "urn:globus:auth:scope:transfer.api.globus.org:all"

    # 2. Collection-specific HTTPS scope - for downloading files via HTTPS
    https_scope = f"https://auth.globus.org/scopes/{collection_id}/https"

    # Combine scopes
    scopes = [transfer_scope, https_scope]

    print("=== Globus OAuth2 Token Generation ===")
    print("")
    print(f"Collection ID: {collection_id}")
    print(f"Required scopes:")
    for scope in scopes:
        print(f"  - {scope}")
    print("")

    # Create a Native App client
    client = globus_sdk.NativeAppAuthClient(CLIENT_ID)

    # Start the OAuth2 flow
    client.oauth2_start_flow(requested_scopes=scopes)

    # Get the authorization URL
    authorize_url = client.oauth2_get_authorize_url()

    print("Please visit this URL to authorize the application:")
    print("")
    print(authorize_url)
    print("")

    # Wait for the user to authorize and paste the code
    auth_code = input("Paste the authorization code here: ").strip()

    # Exchange the code for tokens
    token_response = client.oauth2_exchange_code_for_tokens(auth_code)

    # Get tokens for each resource server
    transfer_tokens = token_response.by_resource_server['transfer.api.globus.org']

    # The HTTPS scope is a dependent scope, so it might be in the transfer tokens
    # or we need to get it separately

    print("")
    print("=== Tokens Retrieved ===")
    print("")
    print("Transfer API Access Token:")
    print(transfer_tokens['access_token'])
    print("")

    # Check if we have HTTPS scope tokens
    # For dependent scopes, they may be included in the transfer token response
    print("You can now use this token with:")
    print(f"  export GLOBUS_ACCESS_TOKEN='{transfer_tokens['access_token']}'")
    print("")

    # Save tokens to a file
    with open('/tmp/globus_tokens.txt', 'w') as f:
        f.write(f"GLOBUS_ACCESS_TOKEN={transfer_tokens['access_token']}\n")

    print("Token saved to: /tmp/globus_tokens.txt")
    print("Load it with: source <(cat /tmp/globus_tokens.txt | sed 's/^/export /')")

    return {
        'transfer': transfer_tokens['access_token'],
        'refresh': transfer_tokens.get('refresh_token')
    }

if __name__ == "__main__":
    collection_id = "e8cf0e9a-f96a-11ed-9a83-83ef71fbf0ae"

    if len(sys.argv) > 1:
        collection_id = sys.argv[1]

    try:
        tokens = get_tokens_for_collection(collection_id)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
