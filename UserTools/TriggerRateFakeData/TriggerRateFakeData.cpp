#include "TriggerRateFakeData.h"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>

TriggerRateFakeData_args::TriggerRateFakeData_args() : Thread_args() {}
TriggerRateFakeData_args::~TriggerRateFakeData_args() {}
TriggerRateFakeData::TriggerRateFakeData() : Tool() {}

bool TriggerRateFakeData::Initialise(std::string configfile, DataModel &data) {
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  args = new TriggerRateFakeData_args();
  args->m_data = m_data;
  if (!m_variables.Get("broadcast_interval_s", args->broadcast_interval_s))
    args->broadcast_interval_s = 1;

  args->db_conn = FakeDataDB_Connect();

  m_data->channels["monitoring_trigger_rates"];
  m_util = new Utilities();
  m_util->CreateThread("trigger_rates", &Thread, args);

  ExportConfiguration();
  return true;
}

bool TriggerRateFakeData::Execute() { return true; }

bool TriggerRateFakeData::Finalise() {
  m_util->KillThread(args);
  delete args;   args   = 0;
  delete m_util; m_util = 0;
  return true;
}

// ---------------------------------------------------------------------------

void TriggerRateFakeData::Thread(Thread_args* arg) {
  TriggerRateFakeData_args* args = reinterpret_cast<TriggerRateFakeData_args*>(arg);

  // Centre values and ordered keys
  static const std::unordered_map<std::string, double> centres = {
    {"t2k",       0.5},
    {"sle",       8.0},
    {"le",        4.0},
    {"he",        1.5},
    {"od",        0.3},
    {"she",       0.2},
    {"autocalib", 0.1},
    {"gps",      15.2},
  };
  static const std::vector<std::string> keys = {
    "t2k", "sle", "le", "he", "od", "she", "autocalib", "gps"
  };
  static std::mt19937 rng(std::random_device{}());
  static std::unordered_map<std::string, double> drift;

  // Ornstein-Uhlenbeck drift per variable
  for (const auto& key : keys) {
    double centre = centres.at(key);
    double sigma  = (key == "gps") ? centre * 0.1 : centre * 0.15;
    drift[key]    = drift[key] * 0.98 + std::normal_distribution<double>(0, sigma * 0.1)(rng);
    drift[key]   += std::normal_distribution<double>(0, sigma * 0.3)(rng);
  }

  // Timestamp
  auto now = std::chrono::system_clock::now();
  auto t   = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ts;
  ts << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
  std::string time_iso = ts.str();

  // Data JSON
  std::ostringstream data;
  data << std::fixed << std::setprecision(4);
  for (size_t i = 0; i < keys.size(); i++) {
    const std::string& key = keys[i];
    double val = std::max(0.0, centres.at(key) + drift[key]);
    if (i > 0) data << ",";
    data << "\"" << key << "\":" << val;
  }
  std::string data_json = "{" + data.str() + "}";

  // DB write
  FakeDataDB_WriteMonitoring(args->db_conn, time_iso, "DAQController", "trigger_rates", data_json);

  // WS broadcast
  std::string payload = "{\"time\":\"" + time_iso + "\","
                        "\"device\":\"DAQController\","
                        "\"subject\":\"trigger_rates\","
                        "\"data\":" + data_json + "}";
  args->m_data->channels["monitoring_trigger_rates"].Send(payload, "DAQController");
  args->m_data->channels["monitoring_trigger_rates"].MessageClear();
  args->m_data->channels["monitoring_trigger_rates"].ConnectMessageClear();

  sleep(args->broadcast_interval_s);
}
