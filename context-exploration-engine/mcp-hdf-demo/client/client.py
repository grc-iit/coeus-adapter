import asyncio
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def main():
    # Use stdio_client to launch the server as a subprocess for testing
    server_params = StdioServerParameters(
        command="python",
        args=["server/server.py"],
        env=None
    )
    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            print("Available tools:")
            tools = await session.list_tools()
            for tool in tools:
                print("-", tool)

            print("\nAvailable resources:")
            resources = await session.list_resources()
            for resource in resources:
                print("-", resource)

            # List all HDF5/HDF4 paths using the tool
            result = await session.call_tool("list_hdf_paths")
            print("\nHDF5/HDF4 paths in server:")
            print(result)

            # Fetch a specific HDF5 dataset as a resource
            hdf5_dataset_path = "group1/dataset1"
            hdf5_resource_uri = f"hdf://hdf5/{hdf5_dataset_path}"
            content = await session.read_resource(hdf5_resource_uri)
            print(f"\nFetched HDF5 resource '{hdf5_resource_uri}':")
            print(content)

            # Fetch a specific HDF4 dataset as a resource
            hdf4_dataset_path = "dataset1"
            hdf4_resource_uri = f"hdf://hdf4/{hdf4_dataset_path}"
            content = await session.read_resource(hdf4_resource_uri)
            print(f"\nFetched HDF4 resource '{hdf4_resource_uri}':")
            print(content)

if __name__ == "__main__":
    asyncio.run(main())
