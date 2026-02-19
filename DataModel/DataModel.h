#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <map>
#include <string>
#include <vector>

//#include "TTree.h"

#include "Store.h"
#include "BoostStore.h"
//#include "DAQLogging.h"
#include "DAQUtilities.h"
#include "SlowControlCollection.h"
#include "DAQDataModelBase.h"

#include <zmq.hpp>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <App.h>

using namespace ToolFramework;

// Per-connection data
struct PerSocketData {
  std::string channel;
  std::string client_id;
  std::unordered_map<std::string, std::unordered_set<std::string>> device_keys;
  std::mutex* mtx = 0;
};

struct Channel{

  std::unordered_set<uWS::WebSocket<false, true, PerSocketData>*> clients;
  std::unordered_map<std::string, std::unordered_set<std::string>> device_keys;

};

/**
 * \class DataModel
 *
 * This class Is a transient data model class for your Tools within the ToolChain. If Tools need to comunicate they pass all data objects through the data model. There fore inter tool data objects should be deffined in this class. 
 *
 *
 * $Author: B.Richards $ 
 * $Date: 2019/05/26 18:34:00 $
 *
 */

class DataModel : public DAQDataModelBase {


 public:
  
  DataModel(); ///< Simple constructor 

  JobQueue job_queue;
  
  uint32_t thread_cap = 20;
  std::atomic<uint32_t> num_threads{0};

  Store monitoring_store;
  std::mutex monitoring_store_mtx;


  std::unordered_map<std::string, Channel> channels;
  std::shared_mutex channels_mutex;
  std::atomic<int> client_counter{0};
  
 private:
  
  
  
};



#endif
