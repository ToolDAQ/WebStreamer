#ifndef PER_SOCKET_DATA_H
#define PER_SOCKET_DATA_H

// Per-connection data
struct PerSocketData {
  std::string channel="";
  std::string client_id="";
  std::vector<std::string> keys;
  std::mutex* mtx=0;
};


#endif
