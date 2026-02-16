# Globus REST API Authentication Guide

This guide shows how to get access tokens for downloading data from Globus using pure REST API calls.

## Prerequisites

1. **Register an Application** at https://app.globus.org/settings/developers
   - Choose "Register a thick client or script that will be installed and run by users on their devices"
   - Note your **Client ID**
   - For Native Apps, you don't get a client secret

## OAuth2 Flow for REST API

### Step 1: Get Authorization Code

**Manual Method (for testing):**

Visit this URL in your browser (replace `YOUR_CLIENT_ID`):

```
https://auth.globus.org/v2/oauth2/authorize?client_id=YOUR_CLIENT_ID&redirect_uri=https://auth.globus.org/v2/web/auth-code&scope=urn:globus:auth:scope:transfer.api.globus.org:all%20https://auth.globus.org/scopes/e8cf0e9a-f96a-11ed-9a83-83ef71fbf0ae/https&response_type=code
```

**Scopes needed:**
- `urn:globus:auth:scope:transfer.api.globus.org:all` - Transfer API access
- `https://auth.globus.org/scopes/<collection_id>/https` - HTTPS download access for specific collection

After authorizing, you'll get an **authorization code**.

### Step 2: Exchange Code for Tokens

```bash
curl -X POST https://auth.globus.org/v2/oauth2/token \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "client_id=YOUR_CLIENT_ID" \
  -d "grant_type=authorization_code" \
  -d "code=YOUR_AUTH_CODE" \
  -d "redirect_uri=https://auth.globus.org/v2/web/auth-code"
```

**Response:**
```json
{
  "access_token": "AgVxxx...",
  "expires_in": 172800,
  "refresh_token": "AgRxxx...",
  "resource_server": "transfer.api.globus.org",
  "scope": "...",
  "token_type": "Bearer"
}
```

### Step 3: Use the Access Token

**Get endpoint details:**
```bash
curl -H "Authorization: Bearer YOUR_ACCESS_TOKEN" \
  https://transfer.api.globus.org/v0.10/endpoint/e8cf0e9a-f96a-11ed-9a83-83ef71fbf0ae
```

**Download file via HTTPS:**
```bash
# First, get the https_server from endpoint details
HTTPS_SERVER=$(curl -s -H "Authorization: Bearer YOUR_ACCESS_TOKEN" \
  https://transfer.api.globus.org/v0.10/endpoint/e8cf0e9a-f96a-11ed-9a83-83ef71fbf0ae | \
  jq -r '.https_server')

# Then download the file
curl -H "Authorization: Bearer YOUR_ACCESS_TOKEN" \
  "${HTTPS_SERVER}/path/to/file.txt" \
  -o local_file.txt
```

## Quick Token Generation (Web UI)

**Easiest method for testing:**

1. Go to: https://app.globus.org/settings/developers
2. Click "Generate New Access Token"
3. Select scopes:
   - `urn:globus:auth:scope:transfer.api.globus.org:all`
   - `https://auth.globus.org/scopes/e8cf0e9a-f96a-11ed-9a83-83ef71fbf0ae/https`
4. Copy the token
5. Use with: `export GLOBUS_ACCESS_TOKEN="your_token"`

## Using with CAE Integration

Once you have a valid access token:

```bash
export GLOBUS_ACCESS_TOKEN="your_token_here"
./test/integration/globus_matsci/run_test.sh
```

Or in the OMNI file:
```yaml
transfers:
  - src: "https://app.globus.org/file-manager?origin_id=e8cf0e9a-f96a-11ed-9a83-83ef71fbf0ae&origin_path=%2F"
    dst: "file::/tmp/output.txt"
    format: "binary"
    src_token: "${GLOBUS_ACCESS_TOKEN}"
```

## Troubleshooting

### "Token is not active"
- Token has expired (default 48 hours)
- Use refresh token to get new access token
- Or generate a new token

### "Bearer token is not valid" (for HTTPS downloads)
- Token doesn't have collection-specific HTTPS scope
- Regenerate token with: `https://auth.globus.org/scopes/<collection_id>/https`

### "AuthenticationFailed"
- Token format is invalid
- Token was revoked
- Generate a new token
