#include "WebSocketApp.h"

WebSocketApp_args::WebSocketApp_args():Thread_args(){}

WebSocketApp_args::~WebSocketApp_args(){}


WebSocketApp::WebSocketApp():Tool(){}


bool WebSocketApp::Initialise(std::string configfile, DataModel &data){

  InitialiseTool(data);                                                                                               
  InitialiseConfiguration(configfile);
  //m_variables.Print();

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  unsigned int threadcount=0;
  if(!m_variables.Get("Threads",threadcount)) threadcount=4;

  m_util=new Utilities();

  for(unsigned int i=0;i<threadcount;i++){
    WebSocketApp_args* tmparg=new WebSocketApp_args();   
    tmparg->m_data = m_data;
    args.push_back(tmparg);
    std::stringstream tmp;
    tmp<<"T"<<i; 
    m_util->CreateThread(tmp.str(), &Thread, args.at(i));
  }
  
  m_freethreads=threadcount;
  
  ExportConfiguration();  
  
  return true;
}


bool WebSocketApp::Execute(){

  
  return true;
}


bool WebSocketApp::Finalise(){

  for(unsigned int i=0;i<args.size();i++){
    //us_loop_stop((us_loop_t *) args.at(i)->loop);
    us_listen_socket_close(0, args.at(i)->token);
    m_util->KillThread(args.at(i));
  }
  
  args.clear();
  
  delete m_util;
  m_util=0;

  return true;
}

void WebSocketApp::Thread(Thread_args* arg){

  WebSocketApp_args* args=reinterpret_cast<WebSocketApp_args*>(arg);

  //args->loop = uWS::Loop::get();

  uWS::App()
    .ws<PerSocketData>("/*", {
        .compression = uWS::SHARED_COMPRESSOR,
        .maxPayloadLength = 16 * 1024,
        .idleTimeout = 30,
	
        .upgrade = [](auto *res, auto *req, auto *context) {
	  std::string_view query = req->getQuery();
	  
	  std::string channel = "default";
	  size_t pos = query.find("channel=");
	  if (pos != std::string::npos) {
	    size_t end = query.find('&', pos);
	    channel = std::string(query.substr(pos + 8, end - (pos + 8)));
	  }
	  
	  std::string client_id = "unknown"; //"client_" + std::to_string(client_counter.fetch_add(1));
	  pos = query.find("client=");
	  if (pos != std::string::npos) {
	    size_t end = query.find('&', pos);
	    client_id = std::string(query.substr(pos + 7, end - (pos + 7)));
	  }
	  
	  PerSocketData userData;
	  userData.channel = channel;
	  userData.client_id = client_id;
	  userData.mtx = new std::mutex;
	  
	  res->template upgrade<PerSocketData>(
					       std::move(userData),
					       req->getHeader("sec-websocket-key"),
					       req->getHeader("sec-websocket-protocol"),
					       req->getHeader("sec-websocket-extensions"),
					       context
					       );
        },
	
        .open = [args](auto *ws) {
	  auto* user = ws->getUserData();
	  {
	    args->m_data->channels_mutex.lock();
	    args->m_data->channels[user->channel].clients.insert(ws);
	    args->m_data->channels_mutex.unlock();
	  }
	  std::cout << user->client_id << " joined channel: " << user->channel << std::endl;
        },
	
        .message = [](auto *ws, std::string_view message, uWS::OpCode) {
          auto* user = ws->getUserData();
          std::cout << user->client_id << " says: " << message << std::endl;
	  
	  Store tmp;
	  tmp.JsonParser(std::string(message));  // Ensure std::string_view to string conversion
	  tmp.Print();
	  for (auto it = tmp.begin(); it != tmp.end(); ++it) {
	    std::vector<std::string> tmp_keys;
	    tmp.Get(it->first, tmp_keys);
	    user->mtx->lock();
	    for (const std::string& key : tmp_keys) {
	      std::cout << user->client_id << " joined channel: " << it->first << std::endl;
	      user->device_keys[it->first].insert(key);
	    }
	    user->mtx->unlock();
	  }
        },
	
        .close = [args](auto *ws, int, std::string_view) {
	  auto* user = ws->getUserData();
	  {
	    //std::lock_guard<std::mutex> lock(clients_mutex);
	    args->m_data->channels_mutex.lock();
	    delete user->mtx;
	    args->m_data->channels[user->channel].clients.erase(ws);
	    args->m_data->channels_mutex.unlock();
	  }
	  std::cout << user->client_id << " left channel: " << user->channel << std::endl;
        }
      })
    .listen(9001, [args](auto *token) {
      if (token) {
	args->token = token;
	std::cout << "Listening on port 9001\n";
      }
    })
    .run();
  
}
