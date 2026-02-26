#include "FakeDataSource.h"

FakeDataSource_args::FakeDataSource_args():Thread_args(){}

FakeDataSource_args::~FakeDataSource_args(){}


FakeDataSource::FakeDataSource():Tool(){}


bool FakeDataSource::Initialise(std::string configfile, DataModel &data){

  InitialiseTool(data);
  InitialiseConfiguration(configfile);
  //m_variables.Print();

  if(!m_variables.Get("verbose",m_verbose)) m_verbose=1;

  m_util=new Utilities();
  args=new FakeDataSource_args();
  args->m_data=m_data;

  m_data->channels["test"];
  
  m_util->CreateThread("test", &Thread, args);

  ExportConfiguration();
  
  return true;
}


bool FakeDataSource::Execute(){

  return true;
}


bool FakeDataSource::Finalise(){

  m_util->KillThread(args);

  delete args;
  args=0;

  delete m_util;
  m_util=0;

  return true;
}

void FakeDataSource::Thread(Thread_args* arg){

  FakeDataSource_args* args=reinterpret_cast<FakeDataSource_args*>(arg);
  //printf("vecsize = %d\n",args->m_data->channels["test"].filtered_clients.size());
  //for(std::unordered_map<std::string, std::vector<uWS::WebSocket<false, true, PerSocketData>* > >::iterator it=args->m_data->channels["test"].filtered_clients.begin(); it!=args->m_data->channels["test"].filtered_clients.end(); it++){

    // printf("filter = %s, size = %u\n", it->first.c_str(), it->second.size());
  //}
  
  args->m_data->channels["test"].Send("hello world","pmt26");
  sleep(1);

}
