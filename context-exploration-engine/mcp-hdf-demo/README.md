# MCP HDF5 Demo

This project demonstrates an MCP server exposing HDF5 data and a client that queries it.

## Structure
- `server/`: MCP server exposing HDF5 datasets as resources
- `client/`: MCP client fetching/querying HDF5 data

## Setup

### Prerequisites
- Python 3.8+

### Install dependencies
```
pip install -r requirements.txt
```

## Usage

### Start the server
```
python server/server.py
```

### Run the client
```
python client/client.py
```
