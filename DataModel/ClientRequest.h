#ifndef CLIENT_REQUEST_H
#define CLIENT_REQUEST_H

#include <PerSocketData.h>
#include <App.h>

struct ClientRequest{

  ClientRequest(std::string& in_request, uWS::WebSocket<false, true, PerSocketData>* in_ws, std::string& in_client_id){
    request =  in_request;
    ws = in_ws;
    client_id = in_client_id;
  }
  std::string request="";
  uWS::WebSocket<false, true, PerSocketData>* ws = 0;
  std::string client_id ="";
  
};


#endif
