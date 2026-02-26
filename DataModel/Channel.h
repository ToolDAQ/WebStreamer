#ifndef CHANNEL_H
#define CHANNEL_H

#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <App.h>
#include <PerSocketData.h>
#include <shared_mutex>

class Channel{

 public:
  
  std::unordered_map<std::string, std::vector<uWS::WebSocket<false, true, PerSocketData>* > > filtered_clients;
  std::shared_mutex mtx;
  
  void RemoveAll();
  void RemoveClient(std::vector<std::string> keys, std::string client_id);
  void AddClient(std::vector<std::string> keys, uWS::WebSocket<false, true, PerSocketData>* ws);
  void Send(std::string message, std::string filter);
  
};




#endif
