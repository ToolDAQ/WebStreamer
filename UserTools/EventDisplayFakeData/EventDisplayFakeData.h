#ifndef EventDisplayFakeData_H
#define EventDisplayFakeData_H

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <random>
#include "Tool.h"
#include "DataModel.h"
#include "FakeDataDB.h"

struct EventDisplayFakeData_args : Thread_args {
  EventDisplayFakeData_args();
  ~EventDisplayFakeData_args();

  DataModel*  m_data            = nullptr;
  int         broadcast_interval_s = 10;
  PGconn*     db_conn           = nullptr;
  long        event_counter     = 1;
  std::string geo_path;

  // Geometry: pmt_id -> {x, y, z}
  std::unordered_map<int, std::array<float, 3>> pmts;
  std::unordered_map<int, std::string>          pmt_loc;

  // PMT id lists by type/location.
  // barrel/top/bottom include BOTH ID (1-29999) and mPMT (100000-199999) PMTs
  // since both are inward-facing.
  std::vector<int> barrel_ids;     // barrel-location ID + mPMT PMTs
  std::vector<int> top_ids;        // top-cap ID + mPMT PMTs
  std::vector<int> bottom_ids;     // bottom-cap ID + mPMT PMTs
  std::vector<int> od_ids;         // OD PMTs (30000-49999)
  std::vector<int> calib_col_ids;  // calib_id_collimator (200000-209999)
  std::vector<int> calib_dif_ids;  // calib_id_diffuser   (220000-229999)

  std::mt19937 rng;
};

class EventDisplayFakeData : public Tool {
 public:
  EventDisplayFakeData();
  bool Initialise(std::string configfile, DataModel &data);
  bool Execute();
  bool Finalise();

 private:
  static void Thread(Thread_args* arg);
  static bool LoadGeometry(EventDisplayFakeData_args* args);

  // Event generators - each returns a JSON hits array string
  static std::string GeneratePhysicsEvent(EventDisplayFakeData_args* args,
                                          const std::string& type);
  static std::string GenerateODEvent(EventDisplayFakeData_args* args);
  static std::string GenerateCalibEvent(EventDisplayFakeData_args* args);

  static std::string BuildHitsJson(
      const std::unordered_map<int, std::pair<float,float>>& hits);

  Utilities*                 m_util = nullptr;
  EventDisplayFakeData_args* args   = nullptr;
};

#endif
