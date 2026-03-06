#include <Channel.h>

Channel::Channel(){

  cctx = ZSTD_createCCtx();
  
}

Channel::~Channel(){

  delete cctx;
  cctx = 0;

}

void Channel::RemoveAll(){
  mtx.lock();
  message_mtx.lock();
  filtered_clients.clear();
  on_connect.clear();
  on_message.clear();
  message_mtx.unlock();
  mtx.unlock();
}

void Channel::RemoveClient(std::vector<std::string> keys, std::string client_id){
  std::vector<std::deque<ClientRequest>::iterator> del_list;
  
  mtx.lock();
  message_mtx.lock();
  for(std::string& key : keys) {
    if(filtered_clients.count(key) !=0){
      for(std::vector<uWS::WebSocket<false, true, PerSocketData>* >::iterator it=filtered_clients[key].begin(); it!=filtered_clients[key].end(); it++){
	if((*it)->getUserData()->client_id == client_id){
	  filtered_clients[key].erase(it);
	  std::cout << client_id << " left channel: " << key << std::endl;
	  if(filtered_clients[key].size()==0) filtered_clients.erase(key);
	  break;
	}
      }
    }
  }

  
  for(std::deque<ClientRequest>::iterator it=on_connect.begin(); it!=on_connect.end(); it++){
    if(client_id == (*it).client_id) del_list.push_back(it);
  }
  for(size_t i=0; i<del_list.size(); i++){
    on_connect.erase(del_list.at(i));
  }
  del_list.clear();

  for(std::deque<ClientRequest>::iterator it=on_message.begin(); it!=on_message.end(); it++){
    if(client_id == (*it).client_id) del_list.push_back(it);
  }
  for(size_t i=0; i<del_list.size(); i++){
    on_message.erase(del_list.at(i));
  }
  del_list.clear();

  
  
  message_mtx.unlock();
  mtx.unlock();
}

void Channel::AddClient(std::vector<std::string> keys, uWS::WebSocket<false, true, PerSocketData>* ws){
  
  mtx.lock();
  for(std::string& key : keys) {
    filtered_clients[key].push_back(ws);
    std::cout << ws->getUserData()->client_id << " joinned channel: " << key << std::endl;
  }
  mtx.unlock();
}

void Channel::Send(const std::string &message, std::string filter){
  
  mtx.lock_shared();
  for(std::unordered_map<std::string, std::vector<uWS::WebSocket<false, true, PerSocketData>* > >::iterator it=filtered_clients.begin(); it!=filtered_clients.end(); it++){
    
    if(it->first.find(filter) != std::string::npos && it->first!="*"){
      for( std::vector<uWS::WebSocket<false, true, PerSocketData>* >::iterator client=it->second.begin(); client!=it->second.end(); client++){

	compressed_data.resize(ZSTD_compressBound(message.length()));
	 
        cSize = ZSTD_compressCCtx(cctx, compressed_data.data(), compressed_data.length(), message.data(), message.length(), 1);
        if(CHECK_ZSTD(cSize)){
	  compressed_data.resize(cSize);
	  
	  (*client)->getUserData()->mtx->lock();
	  //(*client)->send(message, uWS::OpCode::TEXT);
	  (*client)->send(compressed_data, uWS::OpCode::BINARY);
	  (*client)->getUserData()->mtx->unlock();
	  //printf("sent %s to %s\n", message.c_str(), (*client)->getUserData()->client_id.c_str());
	}
      }
      break;
    }
  }

  
  for(std::vector<uWS::WebSocket<false, true, PerSocketData>* >::iterator client=filtered_clients["*"].begin(); client!=filtered_clients["*"].end(); client++){
    (*client)->send(message, uWS::OpCode::TEXT);
    //printf("sent %s to %s\n", message.c_str(), (*client)->getUserData()->client_id.c_str());
  }
  
  mtx.unlock_shared();
}


void Channel::Send(std::vector<std::string> &messages, std::string filter){
  std::string tmp="{[";

  for(std::vector<std::string>::iterator it=messages.begin(); it!=messages.end(); it++){
    tmp.append(*it);
  }
  tmp.append("]}");
  Send(tmp, filter);
}


void Channel::Send(std::unordered_map<std::string, std::vector<std::string> > &messages){
  
  for(std::unordered_map<std::string, std::vector<std::string> >::iterator it= messages.begin(); it!= messages.end(); it++){
    if(filtered_clients.count(it->first)) Send(it->second, it->first);  
  }
}

 // Helper function to check ZSTD errors
bool Channel::CHECK_ZSTD(size_t const code) {
    if (ZSTD_isError(code)) {
      std::cerr << "ZSTD error: " << ZSTD_getErrorName(code) << std::endl;
      return false;
    }
    return true;
    
  }
 
void Channel::AddMessage(std::string message, uWS::WebSocket<false, true, PerSocketData>* ws){
  message_mtx.lock();
  on_message.emplace_back(message, ws, ws->getUserData()->client_id);
  message_mtx.unlock();
}

void Channel::AddConnectMessage(std::string message, uWS::WebSocket<false, true, PerSocketData>* ws){
  message_mtx.lock();
  on_connect.emplace_back(message, ws, ws->getUserData()->client_id);
  message_mtx.unlock();  
}

ClientRequest Channel::GetMessage(){
  message_mtx.lock();
  ClientRequest ret = on_message.front();
  on_message.pop_front();
  message_mtx.unlock();
  return ret;
}

ClientRequest Channel::GetConnectMessage(){
  message_mtx.lock();
  ClientRequest ret = on_connect.front();
  on_connect.pop_front();
  message_mtx.unlock();
  return ret;
}

void Channel::MessageClear(){
  message_mtx.lock();
  on_message.clear();
  message_mtx.unlock();
}

void Channel::ConnectMessageClear(){
  message_mtx.lock();
  on_connect.clear();
  message_mtx.unlock();
  
}
