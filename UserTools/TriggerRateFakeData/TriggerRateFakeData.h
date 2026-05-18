#ifndef TriggerRateFakeData_H
#define TriggerRateFakeData_H

#include <string>
#include <random>
#include <unordered_map>
#include "Tool.h"
#include "DataModel.h"
#include "FakeDataDB.h"

struct TriggerRateFakeData_args : Thread_args {
  TriggerRateFakeData_args();
  ~TriggerRateFakeData_args();
  DataModel* m_data = 0;
  int broadcast_interval_s = 1;
  PGconn* db_conn = nullptr;
};

class TriggerRateFakeData : public Tool {

 public:
  TriggerRateFakeData();
  bool Initialise(std::string configfile, DataModel &data);
  bool Execute();
  bool Finalise();

 private:
  static void Thread(Thread_args* arg);

  Utilities* m_util = nullptr;
  TriggerRateFakeData_args* args = nullptr;
};

#endif
