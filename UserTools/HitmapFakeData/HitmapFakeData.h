#ifndef HitmapFakeData_H
#define HitmapFakeData_H

#include <string>
#include <vector>
#include <random>
#include "Tool.h"
#include "DataModel.h"
#include "FakeDataDB.h"

struct HitmapFakeData_args : Thread_args {
  HitmapFakeData_args();
  ~HitmapFakeData_args();
  DataModel* m_data;
  int broadcast_interval_s;
  PGconn* db_conn = nullptr;
};

class HitmapFakeData : public Tool {
 public:
  HitmapFakeData();
  bool Initialise(std::string configfile, DataModel &data);
  bool Execute();
  bool Finalise();

 private:
  static void Thread(Thread_args* arg);
  static std::string GenerateDistribution(const std::string& key_pmt,
                                          const std::string& key_hits,
                                          int n_pmts,
                                          double background_mean);
  Utilities* m_util;
  HitmapFakeData_args* args;
};

#endif
