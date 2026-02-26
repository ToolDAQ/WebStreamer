#include <Channel.h>

void Channel::RemoveAll(){
  mtx.lock();
  filtered_clients.clear();
  mtx.unlock();
}

void Channel::RemoveClient(std::vector<std::string> keys, std::string client_id){
  mtx.lock();
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

void Channel::Send(std::string message, std::string filter){
  
  mtx.lock_shared();
  for(std::unordered_map<std::string, std::vector<uWS::WebSocket<false, true, PerSocketData>* > >::iterator it=filtered_clients.begin(); it!=filtered_clients.end(); it++){
    
    if(it->first==filter && it->first!="*"){
      for( std::vector<uWS::WebSocket<false, true, PerSocketData>* >::iterator client=it->second.begin(); client!=it->second.end(); client++){
	(*client)->getUserData()->mtx->lock();
	(*client)->send(message, uWS::OpCode::TEXT);
	(*client)->getUserData()->mtx->unlock();
	//printf("sent %s to %s\n", message.c_str(), (*client)->getUserData()->client_id.c_str());
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


