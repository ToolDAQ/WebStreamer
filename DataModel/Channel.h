#ifndef CHANNEL_H
#define CHANNEL_H

#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <App.h>
#include <PerSocketData.h>
#include <shared_mutex>
#include <unordered_map>
#include <zstd.h>
#include <vector>
#include <ClientRequest.h>
#include <deque>

class Channel{

 public:

  Channel();
  ~Channel();
  void RemoveAll();
  void RemoveClient(std::vector<std::string> keys, std::string client_id);
  void AddClient(std::vector<std::string> keys, uWS::WebSocket<false, true, PerSocketData>* ws);
  void Send(const std::string &message, std::string filter);
  void Send(std::vector<std::string> &messages, std::string filter);
  void Send(std::unordered_map<std::string, std::vector<std::string> > &messages);
  void AddMessage(std::string message, uWS::WebSocket<false, true, PerSocketData>* ws);
  void AddConnectMessage(std::string message, uWS::WebSocket<false, true, PerSocketData>* ws);
  ClientRequest GetMessage();
  ClientRequest GetConnectMessage();
  void MessageClear();
  void ConnectMessageClear();
  
private:

  bool CHECK_ZSTD(size_t const code);
  
  std::unordered_map<std::string, std::vector<uWS::WebSocket<false, true, PerSocketData>* > > filtered_clients;
  std::shared_mutex mtx;
  ZSTD_CCtx* cctx = 0 ;
  std::string compressed_data="";
  size_t cSize = 0;
  
  std::deque<ClientRequest> on_connect;
  std::deque<ClientRequest> on_message;
  std::mutex message_mtx;
  
    
};

#endif
