#include "WebSocket.h"

DataModel* WebSocket::s_data=0;
//us_listen_socket_t* WebSocket::s_token=0;

WebSocket_args::WebSocket_args():Thread_args() {}

WebSocket_args::~WebSocket_args() {}

WebSocket::WebSocket() : Tool() {}

bool WebSocket::Initialise(std::string configfile, DataModel &data) {
    InitialiseTool(data);
    InitialiseConfiguration(configfile);
    
    if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

    unsigned int threadcount = 0;
    if (!m_variables.Get("Threads", threadcount)) threadcount = 1;

    m_util = new Utilities();

    for (unsigned int i = 0; i < threadcount; i++) {
        WebSocket_args* tmparg = new WebSocket_args();
        tmparg->m_data = m_data;
        args.push_back(tmparg);
        
        std::stringstream tmp;
        tmp << "T" << i;
        m_util->CreateThread(tmp.str(), &Thread, args.at(i));
    }

    m_freethreads = threadcount;
    ExportConfiguration();

    s_data=&data;
    
    return true;
}

bool WebSocket::Execute() {
  usleep(100);
  return true;
}

bool WebSocket::Finalise() {
  
  m_data->channels_mtx.lock();
  for(std::unordered_map<std::string, Channel>::iterator it=m_data->channels.begin(); it!=m_data->channels.end(); it++){
    it->second.RemoveAll();
  }
  m_data->channels.clear();
  m_data->channels_mtx.unlock();
  
  for (unsigned int i = 0; i < args.size(); i++) {
    WebSocket_args* tmp =  args.at(i);
    tmp->loop->defer([tmp](){ tmp->app->close(); });
  }
  
  // us_loop_stop(globalLoop);                 // stop the event loop
  //us_listen_socket_close(s_token)
  
  for (unsigned int i = 0; i < args.size(); i++) {
    m_util->KillThread(args.at(i));
  }

  args.clear();

  delete m_util;
  m_util = nullptr;

  return true;
}

void WebSocket::Thread(Thread_args* arg) {
  WebSocket_args* args = reinterpret_cast<WebSocket_args*>(arg);
  
  uWS::App app;
  args->loop = uWS::Loop::get();
  args->app =  &app;
  
  app.ws<PerSocketData>("/*", {
      .compression = uWS::SHARED_COMPRESSOR,
      .maxPayloadLength = 16 * 1024,
      .idleTimeout = 30,
      
      .upgrade = WebSocket::onUpgrade,
      .open = WebSocket::onOpen,
      .message = WebSocket::onMessage,
      .close = WebSocket::onClose
    })
    .listen(666, WebSocket::onListen)  
    .run();
}

// Static function for handling the upgrade (offering connection)
void WebSocket::onUpgrade(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, us_socket_context_t* context) {
  //printf("onupgrade\n");
  std::string_view query = req->getQuery();

  printf("query = %s\n",std::string(query).c_str()); 

    std::string channel = "";
    size_t pos = query.find("channel=");
    if (pos != std::string::npos) {
        size_t end = query.find('&', pos);
        channel = std::string(query.substr(pos + 8, end - (pos + 8)));
    }

    std::string client_id = "unknown";
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
}

// Static function for handling the open event (when conenciton sucessfully opens)
void WebSocket::onOpen(uWS::WebSocket<false, true, PerSocketData>* ws) {
  //printf("onopen\n");
  PerSocketData* user = ws->getUserData();
  //   WebSocket_args* args = reinterpret_cast<WebSocket_args*>(ws->getUserData()); // Assuming args can be accessed here
  
  //args->m_data->channels_mutex.lock();
  //args->m_data->channels[user->channel].clients.insert(ws);
  //args->m_data->channels_mutex.unlock();
  printf("%s connected: \n", user->client_id.c_str());
  s_data->client_counter++;
}

// Static function for handling incoming messages
void WebSocket::onMessage(uWS::WebSocket<false, true, PerSocketData>* ws, std::string_view message, uWS::OpCode) {
  //printf("onmessage\n");
   PerSocketData* user = ws->getUserData();
   printf("%s says: %s\n", user->client_id.c_str(), std::string(message).c_str());
   
    Store tmp;
    tmp.JsonParser(std::string(message));  // Ensure std::string_view to string conversion
    tmp.Print();

    bool update=false;

    std::string channel="";
    update = tmp.Get("Channel",channel);
   
    std::vector<std::string> keys;
    update =  tmp.Get("filter", keys) || update;

    //printf("keys.size() = %u\n", keys.size());

    if(!update) return;

    std::string current_channel="";
    std::vector<std::string> current_keys;
    
    user->mtx->lock();
    current_channel=user->channel;
    user->channel=channel;
    current_keys = user->keys;
    user->keys = keys;
    user->mtx->unlock();
    
    if(current_channel!="" && current_channel!=channel){
      //printf("d1\n");
      s_data->channels[current_channel].RemoveClient(current_keys, user->client_id);
    }
    else if(current_keys!=keys){
      //printf("d2\n");
      s_data->channels[current_channel].RemoveClient(current_keys, user->client_id);
    }
      
    //    printf("add channel = %s, filter = %s\n",);
    s_data->channels[channel].AddClient(keys, ws);
       
}

// Static function for handling the close event
void WebSocket::onClose(uWS::WebSocket<false, true, PerSocketData>* ws, int, std::string_view) {
    PerSocketData* user = ws->getUserData();

    if(user->channel!="") s_data->channels[user->channel].RemoveClient(user->keys, user->client_id);

    printf("%s disconnected: \n", user->client_id.c_str());
    delete user->mtx;
    user->mtx = 0;
    s_data->client_counter--;
    
}

// Static function for handling the listen event
//static void onListen(struct us_listen_socket_t *listen_socket)
void WebSocket::onListen(struct us_listen_socket_t* in_token) {
  if (in_token) {
    //s_token = in_token;
    std::cout << "Listening on port 666"<<std::endl;
    
    //   auto *context = us_socket_context( (us_listen_socket_t *) token );
    // globalLoop = us_socket_context_loop(context);
    

  }

}
