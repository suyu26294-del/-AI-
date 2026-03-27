#include "edge/edge_node.hpp"

#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
  std::string node_id = argc > 1 ? argv[1] : "node-a";
  int port = argc > 2 ? std::atoi(argv[2]) : 9000;
  bool fp16 = argc > 3 ? std::string(argv[3]) == "fp16" : true;
  edge::EdgeNode node(node_id, "127.0.0.1", port, fp16);
  node.run();
  return 0;
}
