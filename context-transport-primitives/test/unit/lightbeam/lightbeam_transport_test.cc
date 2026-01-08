#include <hermes_shm/lightbeam/zmq_transport.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace hshm::lbm;

void TestZeroMQ() {
#ifdef HSHM_ENABLE_ZMQ
  std::cout << "\n==== Testing ZeroMQ ====\n";

  std::string addr = "127.0.0.1";
  std::string protocol = "tcp";
  int port = 8192;

  auto server = std::make_unique<ZeroMqServer>(addr, protocol, port);
  auto client = std::make_unique<ZeroMqClient>(addr, protocol, port);

  // Give ZMQ time to connect
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  const std::string magic = "unit_test_magic";

  // Client creates metadata and sends
  LbmMeta send_meta;
  Bulk send_bulk = client->Expose(magic.data(), magic.size(), BULK_XFER);
  send_meta.send.push_back(send_bulk);

  int rc = client->Send(send_meta);
  assert(rc == 0);
  std::cout << "Client sent data successfully\n";

  // Server receives metadata
  LbmMeta recv_meta;
  while (true) {
    rc = server->RecvMetadata(recv_meta);
    if (rc == 0) break;
    if (rc != EAGAIN) {
      std::cerr << "RecvMetadata failed with error: " << rc << "\n";
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(recv_meta.send.size() == 1);

  // Allocate buffer and receive bulks
  std::vector<char> recv_buf(recv_meta.send[0].size);
  recv_meta.recv.push_back(server->Expose(recv_buf.data(), recv_buf.size(),
                                          recv_meta.send[0].flags.bits_));

  rc = server->RecvBulks(recv_meta);
  if (rc != 0) {
    std::cerr << "RecvBulks failed with error: " << rc << "\n";
    return;
  }

  std::string received(recv_buf.begin(), recv_buf.end());
  std::cout << "Received: " << received << std::endl;
  assert(received == magic);

  std::cout << "[ZeroMQ] Test passed!\n";
#else
  std::cout << "ZeroMQ not enabled, skipping test\n";
#endif
}

int main() {
  TestZeroMQ();
  std::cout << "\nAll transport tests passed!" << std::endl;
  return 0;
}