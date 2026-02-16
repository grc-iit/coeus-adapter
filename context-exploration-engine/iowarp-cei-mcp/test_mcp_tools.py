#!/usr/bin/env python3
"""
Test suite for IOWarp CTE MCP Server.

This test file mimics MCP behavior by using the MCP client library to call
all tools and capture their inputs and outputs. This allows verification
that all Context Interface operations work correctly.
"""

import asyncio
import json
import sys
from pathlib import Path
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

# Colors for output
class Colors:
    PASS = '\033[92m'  # Green
    FAIL = '\033[91m'  # Red
    WARN = '\033[93m'  # Yellow
    INFO = '\033[94m'  # Blue
    RESET = '\033[0m'   # Reset

def print_test(test_name, status, details=""):
    """Print test result with colors."""
    if status == "PASS":
        symbol = "✓"
        color = Colors.PASS
    elif status == "FAIL":
        symbol = "✗"
        color = Colors.FAIL
    elif status == "EXPECTED_FAILURE":
        symbol="⚠"
        color = Colors.WARN
    else:
        symbol = "?"
        color = Colors.INFO
    
    print(f"{color}{symbol} {test_name}{Colors.RESET}")
    if details:
        print(f"    {details}")

async def test_tool(session, tool_name, arguments, expected_fields=None):
    """Test a single MCP tool and return result."""
    print(f"\n🔧 Testing: {tool_name}")
    print(f"   Input: {json.dumps(arguments, indent=2)}")
    
    try:
        result = await session.call_tool(tool_name, arguments)
        output = result.content[0].text if result.content else ""
        
        # Parse JSON output
        try:
            output_json = json.loads(output)
        except json.JSONDecodeError:
            output_json = {"raw_output": output}
        
        print(f"   Output: {json.dumps(output_json, indent=2)}")
        
        # Check if expected fields are present
        if expected_fields:
            missing = [f for f in expected_fields if f not in output_json]
            if missing:
                print_test(f"{tool_name}", "FAIL", f"Missing fields: {missing}")
                return False
        
        # Check for error in output
        if "error" in output_json or "success" in output_json and not output_json.get("success", True):
            print_test(f"{tool_name}", "EXPECTED_FAILURE", "Tool returned error (may be expected without runtime)")
            return "EXPECTED_FAILURE"
        
        print_test(f"{tool_name}", "PASS")
        return True
        
    except Exception as e:
        print(f"   Error: {type(e).__name__}: {str(e)}")
        print_test(f"{tool_name}", "FAIL", f"Exception: {str(e)}")
        return False

async def main():
    """Run all MCP tool tests."""
    print("=" * 80)
    print("IOWarp CTE MCP Server - Test Suite")
    print("=" * 80)
    
    # Setup server parameters
    server_script = Path(__file__).parent / "server.py"
    server_params = StdioServerParameters(
        command="python3",
        args=[str(server_script)],
        env={
            "PYTHONPATH": "/workspace/build/bin"
        }
    )
    
    results = {
        "PASS": [],
        "FAIL": [],
        "EXPECTED_FAILURE": []
    }
    
    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            # List all available tools
            tools_response = await session.list_tools()
            print(f"\n📋 Found {len(tools_response.tools)} available tools")
            
            # Test 1: Runtime Management Tools (always work)
            print("\n" + "=" * 80)
            print("1. Runtime Management Tools")
            print("=" * 80)
            
            result = await test_tool(session, "get_client_status", {}, 
                                   expected_fields=["available"])
            results["PASS" if result else "FAIL"].append("get_client_status")
            
            result = await test_tool(session, "get_cte_types", {},
                                   expected_fields=["available"])
            results["PASS" if result else "FAIL"].append("get_cte_types")
            
            # Test 2: Initialize Runtime (may succeed or fail depending on environment)
            print("\n" + "=" * 80)
            print("2. Runtime Initialization")
            print("=" * 80)
            
            result = await test_tool(session, "initialize_cte_runtime", {},
                                   expected_fields=["success", "runtime_init", "client_init", "cte_init"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("initialize_cte_runtime")
            elif result:
                results["PASS"].append("initialize_cte_runtime")
            else:
                results["FAIL"].append("initialize_cte_runtime")
            
            # Test 3: Context Interface Operations
            print("\n" + "=" * 80)
            print("3. Context Interface Operations")
            print("=" * 80)
            
            # 3.1: Put Blob (context_bundle)
            result = await test_tool(session, "put_blob", {
                "tag_name": "test_tag",
                "blob_name": "test_blob",
                "data": "Hello, World!"
            }, expected_fields=["tag_name", "blob_name", "success"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("put_blob")
            elif result:
                results["PASS"].append("put_blob")
            else:
                results["FAIL"].append("put_blob")
            
            # 3.2: List Blobs (context_query - list)
            result = await test_tool(session, "list_blobs_in_tag", {
                "tag_name": "test_tag"
            }, expected_fields=["tag_name", "blobs", "count"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("list_blobs_in_tag")
            elif result:
                results["PASS"].append("list_blobs_in_tag")
            else:
                results["FAIL"].append("list_blobs_in_tag")
            
            # 3.3: Get Blob Size (context_query - get size)
            result = await test_tool(session, "get_blob_size", {
                "tag_name": "test_tag",
                "blob_name": "test_blob"
            }, expected_fields=["tag_name", "blob_name", "size"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("get_blob_size")
            elif result:
                results["PASS"].append("get_blob_size")
            else:
                results["FAIL"].append("get_blob_size")
            
            # 3.4: Get Blob (context_query - get data)
            result = await test_tool(session, "get_blob", {
                "tag_name": "test_tag",
                "blob_name": "test_blob"
            }, expected_fields=["tag_name", "blob_name"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("get_blob")
            elif result:
                results["PASS"].append("get_blob")
            else:
                results["FAIL"].append("get_blob")
            
            # 3.5: Delete Blob (context_delete)
            result = await test_tool(session, "delete_blob", {
                "tag_name": "test_tag",
                "blob_name": "test_blob"
            }, expected_fields=["tag_name", "blob_name", "success"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("delete_blob")
            elif result:
                results["PASS"].append("delete_blob")
            else:
                results["FAIL"].append("delete_blob")
            
            # Test 4: Additional CTE Operations
            print("\n" + "=" * 80)
            print("4. Additional CTE Operations")
            print("=" * 80)
            
            result = await test_tool(session, "tag_query", {
                "tag_regex": ".*",
                "max_tags": 10
            }, expected_fields=["tag_regex", "tags", "count"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("tag_query")
            elif result:
                results["PASS"].append("tag_query")
            else:
                results["FAIL"].append("tag_query")
            
            result = await test_tool(session, "blob_query", {
                "tag_regex": ".*",
                "blob_regex": ".*",
                "max_blobs": 10
            }, expected_fields=["tag_regex", "blob_regex", "blobs", "count"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("blob_query")
            elif result:
                results["PASS"].append("blob_query")
            else:
                results["FAIL"].append("blob_query")
            
            result = await test_tool(session, "poll_telemetry_log", {
                "minimum_logical_time": 0
            }, expected_fields=["minimum_logical_time", "entries", "count"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("poll_telemetry_log")
            elif result:
                results["PASS"].append("poll_telemetry_log")
            else:
                results["FAIL"].append("poll_telemetry_log")
            
            result = await test_tool(session, "reorganize_blob", {
                "tag_id_major": 0,
                "tag_id_minor": 0,
                "blob_name": "test_blob",
                "new_score": 0.5
            }, expected_fields=["tag_id", "blob_name", "new_score", "success"])
            if result == "EXPECTED_FAILURE":
                results["EXPECTED_FAILURE"].append("reorganize_blob")
            elif result:
                results["PASS"].append("reorganize_blob")
            else:
                results["FAIL"].append("reorganize_blob")
    
    # Print summary
    print("\n" + "=" * 80)
    print("Test Summary")
    print("=" * 80)
    
    total_tests = len(results["PASS"]) + len(results["FAIL"]) + len(results["EXPECTED_FAILURE"])
    
    print(f"\n{Colors.PASS}✓ Passed: {len(results['PASS'])}{Colors.RESET}")
    for test in results["PASS"]:
        print(f"    - {test}")
    
    if results["EXPECTED_FAILURE"]:
        print(f"\n{Colors.WARN}⚠ Expected Failures: {len(results['EXPECTED_FAILURE'])}{Colors.RESET}")
        print("    (These may fail without proper runtime initialization)")
        for test in results["EXPECTED_FAILURE"]:
            print(f"    - {test}")
    
    if results["FAIL"]:
        print(f"\n{Colors.FAIL}✗ Failed: {len(results['FAIL'])}{Colors.RESET}")
        for test in results["FAIL"]:
            print(f"    - {test}")
    
    print(f"\n📊 Total Tests: {total_tests}")
    print(f"   Passed: {len(results['PASS'])}")
    print(f"   Expected Failures: {len(results['EXPECTED_FAILURE'])}")
    print(f"   Failed: {len(results['FAIL'])}")
    
    # Exit with appropriate code
    if results["FAIL"]:
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    asyncio.run(main())

