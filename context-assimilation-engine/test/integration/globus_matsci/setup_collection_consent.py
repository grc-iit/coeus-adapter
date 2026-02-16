#!/usr/bin/env python3
"""
setup_collection_consent.py - Set up consent for HTTPS access to a Globus collection

This script handles getting the proper consent for accessing a guest collection
via HTTPS. Guest collections require collection-specific scopes.

Usage:
  ./setup_collection_consent.py <collection_id>
"""

import sys
from globus_cli.login_manager import LoginManager

def main():
    if len(sys.argv) < 2:
        print("Usage: setup_collection_consent.py <collection_id>")
        sys.exit(1)

    collection_id = sys.argv[1]

    print(f"Setting up HTTPS access consent for collection: {collection_id}")
    print("")

    # The scope needed for HTTPS access to this collection
    https_scope = f"https://auth.globus.org/scopes/{collection_id}/https"

    print(f"Required scope: {https_scope}")
    print("")
    print("This collection requires collection-specific consent.")
    print("You need to add this scope when logging in.")
    print("")
    print("Run the following command:")
    print(f"  globus session consent '{https_scope}'")
    print("")
    print("Or re-login with the required scopes:")
    print(f"  globus logout")
    print(f"  globus login")
    print("")
    print("Note: The Globus CLI may automatically request consent when needed.")

if __name__ == "__main__":
    main()
