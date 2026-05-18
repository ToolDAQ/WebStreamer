#include "HitmapFakeData.h"
#include <sstream>
#include <cmath>
#include <chrono>
#include <iomanip>

HitmapFakeData_args::HitmapFakeData_args()  : Thread_args() {}
HitmapFakeData_args::~HitmapFakeData_args() {}
HitmapFakeData::HitmapFakeData()            : Tool() {}

bool HitmapFakeData::Initialise(std::string configfile, DataModel &data) {
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  args = new HitmapFakeData_args();
  args->m_data = m_data;
  if (!m_variables.Get("broadcast_interval_s", args->broadcast_interval_s))
    args->broadcast_interval_s = 5;

  args->db_conn = FakeDataDB_Connect();

  m_data->channels["monitoring_hitmap"];
  m_util = new Utilities();
  m_util->CreateThread("hitmap", &Thread, args);

  ExportConfiguration();
  return true;
}

bool HitmapFakeData::Execute()   { return true; }

bool HitmapFakeData::Finalise() {
  m_util->KillThread(args);
  delete args;   args   = 0;
  delete m_util; m_util = 0;
  return true;
}

// ---------------------------------------------------------------------------

std::string HitmapFakeData::GenerateDistribution(const std::string& key_pmt,
                                                  const std::string& key_hits,
                                                  int n_pmts,
                                                  double background_mean) {
  static std::mt19937 rng(std::random_device{}());
  std::poisson_distribution<int> background(background_mean);
  std::uniform_int_distribution<int> centre_dist(0, n_pmts - 1);
  std::uniform_real_distribution<double> amp_dist(200.0, 600.0);
  std::uniform_real_distribution<double> width_dist(n_pmts * 0.02, n_pmts * 0.06);
  std::uniform_int_distribution<int> dead_dist(0, n_pmts - 1);

  std::vector<int> hits(n_pmts, 0);

  // Background
  for (int i = 0; i < n_pmts; i++)
    hits[i] = background(rng);

  // Dead channels (~2%)
  int n_dead = static_cast<int>(n_pmts * 0.02);
  for (int d = 0; d < n_dead; d++)
    hits[dead_dist(rng)] = 0;

  // Hot clusters
  for (int c = 0; c < 3; c++) {
    int    cen = centre_dist(rng);
    double amp = amp_dist(rng);
    double w   = width_dist(rng);
    int lo = std::max(0,       cen - static_cast<int>(w * 3));
    int hi = std::min(n_pmts,  cen + static_cast<int>(w * 3));
    for (int i = lo; i < hi; i++) {
      double g = amp * std::exp(-0.5 * std::pow((i - cen) / w, 2));
      std::poisson_distribution<int> cluster(g);
      hits[i] += cluster(rng);
    }
  }

  // Build sparse JSON arrays (nonzero only)
  std::ostringstream pmt_arr, hit_arr;
  pmt_arr << "[";
  hit_arr << "[";
  bool first = true;
  for (int i = 0; i < n_pmts; i++) {
    if (hits[i] <= 0) continue;
    if (!first) { pmt_arr << ","; hit_arr << ","; }
    pmt_arr << i;
    hit_arr << hits[i];
    first = false;
  }
  pmt_arr << "]";
  hit_arr << "]";

  return "\"" + key_pmt + "\":" + pmt_arr.str() +
         ",\"" + key_hits + "\":" + hit_arr.str();
}


void HitmapFakeData::Thread(Thread_args* arg) {
  HitmapFakeData_args* args = reinterpret_cast<HitmapFakeData_args*>(arg);

  // Timestamp
  auto now = std::chrono::system_clock::now();
  auto t   = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ts;
  ts << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
  std::string time_iso = ts.str();

  // Data fields
  std::string id_fields   = HitmapFakeData::GenerateDistribution("id_pmt",   "id_hits",   20000, 80.0);
  std::string od_fields   = HitmapFakeData::GenerateDistribution("od_pmt",   "od_hits",   3000,  40.0);
  std::string mpmt_fields = HitmapFakeData::GenerateDistribution("mpmt_pmt", "mpmt_hits", 5000,  60.0);
  std::string data_json   = "{" + id_fields + "," + od_fields + "," + mpmt_fields + "}";

  // DB write
  FakeDataDB_WriteMonitoring(args->db_conn, time_iso, "DAQController", "hitmap", data_json);

  // WS broadcast
  std::string payload = "{\"time\":\"" + time_iso + "\","
                        "\"device\":\"DAQController\","
                        "\"subject\":\"hitmap\","
                        "\"data\":" + data_json + "}";
  args->m_data->channels["monitoring_hitmap"].Send(payload, "DAQController");
  args->m_data->channels["monitoring_hitmap"].MessageClear();
  args->m_data->channels["monitoring_hitmap"].ConnectMessageClear();

  sleep(args->broadcast_interval_s);
}
