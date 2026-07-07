#include "ServiceDiscoverySource.h"

ServiceDiscoverySource_args::ServiceDiscoverySource_args():Thread_args(){}

ServiceDiscoverySource_args::~ServiceDiscoverySource_args(){

  delete m_utils;
  m_utils = 0;

}


ServiceDiscoverySource::ServiceDiscoverySource():Tool(){}


bool ServiceDiscoverySource::Initialise(std::string configfile, DataModel &data){

  InitialiseTool(data);
  InitialiseConfiguration(configfile);
  //m_variables.Print();

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  m_util=new Utilities();
  args=new ServiceDiscoverySource_args();
  args->m_data = m_data;
  args->m_utils = new DAQUtilities(m_data->context); 

  m_data->channels["SD"];
  
  m_util->CreateThread("SD", &Thread, args);

  ExportConfiguration();
  
  return true;
}


bool ServiceDiscoverySource::Execute(){

  return true;
}


bool ServiceDiscoverySource::Finalise(){

  m_util->KillThread(args);

  delete args;
  args=0;

  delete m_util;
  m_util=0;

  return true;
}

void ServiceDiscoverySource::Thread(Thread_args* arg){

  ServiceDiscoverySource_args* args=reinterpret_cast<ServiceDiscoverySource_args*>(arg);
  
  args->m_utils->GetServices(args->services);
  if(args->services.size()>0){
    args->message="{[";
    
    for(size_t i=0; i< args->services.size()-1; i++){
      std::string tmp="";
      services.at(i) >> tmp; 
      args->message+=tmp + ",";   
    }

    std::string tmp="";
    services.at(args->services.size()-1) >> tmp; 
    args->message+=tmp + "]}";   
    
    args->m_data->channels["SD"].Send(args->message);
    args->m_data->channels["SD"].MessageClear();
    args->m_data->channels["SD"].ConnectMessageClear();
  } 
  
  sleep(2);
  
}
