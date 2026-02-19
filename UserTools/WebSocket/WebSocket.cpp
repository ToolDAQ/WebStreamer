#include "WebSocket.h"

WebSocket_args::WebSocket_args():Thread_args() {}

WebSocket_args::~WebSocket_args() {}

WebSocket::WebSocket() : Tool() {}

bool WebSocket::Initialise(std::string configfile, DataModel &data) {
    InitialiseTool(data);
    InitialiseConfiguration(configfile);
    
    if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

    unsigned int threadcount = 0;
    if (!m_variables.Get("Threads", threadcount)) threadcount = 4;

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

    return true;
}

bool WebSocket::Execute() {
    return true;
}

bool WebSocket::Finalise() {
    for (unsigned int i = 0; i < args.size(); i++) {
        us_listen_socket_close(0, args.at(i)->token);
        m_util->KillThread(args.at(i));
    }

    args.clear();

    delete m_util;
    m_util = nullptr;

    return true;
}

void WebSocket::Thread(Thread_args* arg) {
    WebSocket_args* args = reinterpret_cast<WebSocket_args*>(arg);

    uWS::App()
        .ws<PerSocketData>("/*", {
            .compression = uWS::SHARED_COMPRESSOR,
            .maxPayloadLength = 16 * 1024,
            .idleTimeout = 30,

            .upgrade = WebSocket::onUpgrade,
            .open = WebSocket::onOpen,
            .message = WebSocket::onMessage,
            .close = WebSocket::onClose
        })
        .listen(9001, WebSocket::onListen)
        .run();
}

// Static function for handling the upgrade (offering connection)
void WebSocket::onUpgrade(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, uWS::Context* context) {
    std::string_view query = req->getQuery();

    std::string channel = "default";
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
    userData.mtx = new std::mutex; //think this leaking ben

    res->template upgrade<PerSocketData>(
        std::move(userData),
        req->getHeader("sec-websocket-key"),
        req->getHeader("sec-websocket-protocol"),
        req->getHeader("sec-websocket-extensions"),
        context
    );
}

// Static function for handling the open event (when conenciton sucessfully opens)
void WebSocket::onOpen(uWS::WebSocket<false, true>* ws) {
    PerSocketData* user = ws->getUserData();
    WebSocket_args* args = reinterpret_cast<WebSocket_args*>(ws->getUserData()); // Assuming args can be accessed here

    args->m_data->channels_mutex.lock();
    args->m_data->channels[user->channel].clients.insert(ws);
    args->m_data->channels_mutex.unlock();

    std::cout << user->client_id << " joined channel: " << user->channel << std::endl;
}

// Static function for handling incoming messages
void WebSocket::onMessage(uWS::WebSocket<false, true>* ws, std::string_view message, uWS::OpCode) {
    PerSocketData* user = ws->getUserData();
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
}

// Static function for handling the close event
void WebSocket::onClose(uWS::WebSocket<false, true>* ws, int, std::string_view) {
    PerSocketData* user = ws->getUserData();
    WebSocket_args* args = reinterpret_cast<WebSocket_args*>(ws->getUserData());

    args->m_data->channels_mutex.lock();
    delete user->mtx;
    args->m_data->channels[user->channel].clients.erase(ws);
    args->m_data->channels_mutex.unlock();

    std::cout << user->client_id << " left channel: " << user->channel << std::endl;
}

// Static function for handling the listen event
void WebSocket::onListen(uWS::ListenSocket* token) {
    if (token) {
        std::cout << "Listening on port 9001\n";
    }
}
