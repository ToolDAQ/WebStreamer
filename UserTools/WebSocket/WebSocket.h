#ifndef WebSocket_H
#define WebSocket_H

#include <string>
#include <iostream>

#include "Tool.h"
#include "DataModel.h"

#include <App.h>
//#include <libusockets.h>

/**
 * \struct WebSocket_args
 *
 * This is a struct to place data you want your thread to acess or exchange with it. The idea is the datainside is only used by the thread and so will be thread safe
 *
 * $Author: B.Richards $
 * $Date: 2019/05/28 10:44:00 $
 */

struct WebSocket_args:Thread_args{

  WebSocket_args();
  ~WebSocket_args();

  DataModel* m_data = 0;
  uWS::Loop* loop = 0;
  uWS::App* app = 0;  
};


/**
 * \class WebSocket
 *
 * This is a balnk template for a Tool used by the script to generate a new custom tool. Please fill out the descripton and author information.
*
* $Author: B.Richards $
* $Date: 2019/05/28 10:44:00 $
*/

class WebSocket: public Tool {


 public:

  WebSocket(); ///< Simple constructor
  bool Initialise(std::string configfile,DataModel &data); ///< Initialise Function for setting up Tool resorces. @param configfile The path and name of the dynamic configuration file to read in. @param data A reference to the transient data class used to pass information between Tools.
  bool Execute(); ///< Executre function used to perform Tool perpose. 
  bool Finalise(); ///< Finalise funciton used to clean up resorces.
  static DataModel* s_data;
  //  static us_listen_socket_t* s_token;


 private:

  static void Thread(Thread_args* arg); ///< Function to be run by the thread in a loop. Make sure not to block in it
  Utilities* m_util; ///< Pointer to utilities class to help with threading
  std::vector<WebSocket_args*> args; ///< Vector of thread args (also holds pointers to the threads)
  unsigned int m_freethreads; ///< Keeps track of free threads

  static void onUpgrade(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, us_socket_context_t* context);
  static void onOpen(uWS::WebSocket<false, true, PerSocketData>* ws);
  static void onMessage(uWS::WebSocket<false, true, PerSocketData>* ws, std::string_view message, uWS::OpCode);
  static void onClose(uWS::WebSocket<false, true, PerSocketData>* ws, int, std::string_view);
  static void onListen(us_listen_socket_t* token);
  //  us_loop_t *globalLoop = nullptr;
 };


#endif
