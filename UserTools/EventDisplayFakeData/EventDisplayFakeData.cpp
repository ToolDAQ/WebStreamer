#include "EventDisplayFakeData.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Geometry constants — CSV coordinates are in centimetres
// Values match mock_webstreamer.py (BARREL_RADIUS=3220 etc.)
// ---------------------------------------------------------------------------
static const double BARREL_RADIUS = 3220.0;    // cm
static const double BARREL_Z_MIN  = -3352.0;   // cm
static const double BARREL_Z_MAX  =  3182.0;   // cm
static const double CAP_Z_TOP     =  3267.55;  // cm
static const double CAP_Z_BOTTOM  = -3267.55;  // cm
static const double C_WATER       =    22.0;   // cm/ns  (c / n_water, n≈1.34)

// OD noise hits added to every physics event (as in Python)
static const int OD_NOISE_MIN = 0;
static const int OD_NOISE_MAX = 15;

// ---------------------------------------------------------------------------
// Physics parameters — faithfully translated from mock_webstreamer.py
// PHYSICS_PARAMS (hit_smear converted mm→cm: Python 250mm → 25cm)
// ---------------------------------------------------------------------------
struct PhysicsParams {
  int   n_hits_mean,  n_hits_std;
  float c_ring_mean,  c_ring_std;
  float c_noise_mean, c_noise_std;
  float ring_fraction;
  float t_ring_mean,  t_ring_std;   // ns
  float t_noise_mean, t_noise_std;  // ns
  float cherenkov_angle_deg;
  float hit_smear;   // cm (Python mm / 10)
};

// Index order matches PHYS_KEYS[]
static const PhysicsParams PHYS[] = {
  // HE
  { 2200, 800,  3.0f, 1.5f,  1.1f, 0.4f,  0.18f,  30.0f,  6.0f,  250.0f, 200.0f,  42.0f,  25.0f },
  // LE
  { 1600, 700,  2.0f, 1.2f,  1.0f, 0.4f,  0.12f,  30.0f,  8.0f,  250.0f, 200.0f,  42.0f,  35.0f },
  // SHE
  { 3000, 900,  4.0f, 2.0f,  1.2f, 0.5f,  0.20f,  28.0f,  5.0f,  250.0f, 200.0f,  42.0f,  20.0f },
  // SLE
  {  600, 200,  1.5f, 0.8f,  0.9f, 0.3f,  0.10f,  30.0f, 10.0f,  250.0f, 200.0f,  42.0f,  40.0f },
};
static const char* PHYS_KEYS[] = { "HE", "LE", "SHE", "SLE" };
static const int   N_PHYS      = 4;

static const PhysicsParams* GetPhysicsParams(const std::string& type) {
  for (int i = 0; i < N_PHYS; i++)
    if (type == PHYS_KEYS[i]) return &PHYS[i];
  return nullptr;
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

EventDisplayFakeData_args::EventDisplayFakeData_args() : Thread_args() {}
EventDisplayFakeData_args::~EventDisplayFakeData_args() {
  if (db_conn) { PQfinish(db_conn); db_conn = nullptr; }
}
EventDisplayFakeData::EventDisplayFakeData() : Tool() {}

// ---------------------------------------------------------------------------
// Initialise / Execute / Finalise
// ---------------------------------------------------------------------------

bool EventDisplayFakeData::Initialise(std::string configfile, DataModel &data) {
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  args = new EventDisplayFakeData_args();
  args->m_data = m_data;
  if (!m_variables.Get("broadcast_interval_s", args->broadcast_interval_s))
    args->broadcast_interval_s = 10;
  if (!m_variables.Get("geo_path", args->geo_path))
    args->geo_path = "/web/html-Detector/Shift/EventDisplay/geometry/geofile_HyperK.txt";

  args->rng.seed(std::random_device{}());
  args->db_conn = FakeDataDB_Connect();

  if (!LoadGeometry(args)) {
    std::cerr << "EventDisplayFakeData: failed to load geometry from "
              << args->geo_path << std::endl;
    return false;
  }

  if (m_verbose >= 1) {
    std::cout << "EventDisplayFakeData: geometry loaded"
              << " barrel=" << args->barrel_ids.size()
              << " top="    << args->top_ids.size()
              << " bottom=" << args->bottom_ids.size()
              << " od="     << args->od_ids.size()
              << " calib_col=" << args->calib_col_ids.size()
              << " calib_dif=" << args->calib_dif_ids.size()
              << std::endl;
  }

  if (args->db_conn)
    args->event_counter = FakeDataDB_GetNextEventNumber(args->db_conn);

  m_data->channels["event_display"];
  m_util = new Utilities();
  m_util->CreateThread("event_display", &Thread, args);

  ExportConfiguration();
  return true;
}

bool EventDisplayFakeData::Execute()  { return true; }

bool EventDisplayFakeData::Finalise() {
  m_util->KillThread(args);
  delete args;   args   = nullptr;
  delete m_util; m_util = nullptr;
  return true;
}

// ---------------------------------------------------------------------------
// Geometry loader
// CSV columns: id, y, z, x, location   (x is column 3)
//
// PMT classification by ID range (matches HK_PMT_ENUM_RANGES in JS):
//   1     – 29999  : ID PMTs      → barrel/top/bottom by CSV location
//   30000 – 49999  : OD PMTs      → od_ids
//   100000– 199999 : mPMT PMTs    → barrel/top/bottom by CSV location
//                    (mPMTs are inward-facing, same pools as ID)
//   200000– 209999 : calib_id_collimator → calib_col_ids
//   220000– 229999 : calib_id_diffuser   → calib_dif_ids
//   (210000-219999 calib_od_collimator and 230000+ calib_od_diffuser not used)
// ---------------------------------------------------------------------------

bool EventDisplayFakeData::LoadGeometry(EventDisplayFakeData_args* args) {
  std::ifstream f(args->geo_path);
  if (!f.is_open()) return false;

  std::string line;
  std::getline(f, line); // skip header

  while (std::getline(f, line)) {
    if (line.empty()) continue;
    std::istringstream ss(line);
    std::string tok;
    std::vector<std::string> cols;
    while (std::getline(ss, tok, ',')) cols.push_back(tok);
    if (cols.size() < 5) continue;

    int id = std::stoi(cols[0]);
    float y  = std::stof(cols[1]);
    float z  = std::stof(cols[2]);
    float x  = std::stof(cols[3]);
    std::string loc = cols[4];
    while (!loc.empty() && (loc.back() == '\r' || loc.back() == ' ')) loc.pop_back();

    args->pmts[id]    = {x, y, z};
    args->pmt_loc[id] = loc;

    if (id >= 30000 && id < 50000) {
      args->od_ids.push_back(id);
    } else if (id >= 200000 && id < 210000) {
      args->calib_col_ids.push_back(id);   // calib_id_collimator
    } else if (id >= 220000 && id < 230000) {
      args->calib_dif_ids.push_back(id);   // calib_id_diffuser
    } else if ((id >= 1 && id < 30000) || (id >= 100000 && id < 200000)) {
      // ID PMTs and mPMTs are both inward-facing — use CSV location for spatial split
      if      (loc == "barrel") args->barrel_ids.push_back(id);
      else if (loc == "top")    args->top_ids.push_back(id);
      else if (loc == "bottom") args->bottom_ids.push_back(id);
    }
    // calib_od_collimator (210000-219999) and calib_od_diffuser (230000+) not used
  }

  return !args->pmts.empty();
}

// ---------------------------------------------------------------------------
// BuildHitsJson — serialise hit map to JSON array
// ---------------------------------------------------------------------------

std::string EventDisplayFakeData::BuildHitsJson(
    const std::unordered_map<int, std::pair<float,float>>& hits) {
  std::ostringstream out;
  out << "[";
  bool first = true;
  for (const auto& kv : hits) {
    if (!first) out << ",";
    out << "{\"c\":" << kv.second.first
        << ",\"t\":"  << kv.second.second
        << ",\"pmt\":" << kv.first << "}";
    first = false;
  }
  out << "]";
  return out.str();
}

// ---------------------------------------------------------------------------
// Internal helpers (file-scope static)
// ---------------------------------------------------------------------------

// Random sample of up to search_n candidates (with replacement) — fast approx
// to Python's random.sample(candidates, min(search_n, len)), avoids O(N) alloc.
static int FindNearestPmt(double tx, double ty, double tz,
                           const std::vector<int>& candidates,
                           const std::unordered_map<int, std::array<float,3>>& pmts,
                           std::mt19937& rng, int search_n = 500) {
  int n = static_cast<int>(candidates.size());
  int k = std::min(search_n, n);
  std::uniform_int_distribution<int> pick(0, n - 1);

  double best_d2 = 1e18;
  int best_id = candidates[0];
  for (int i = 0; i < k; i++) {
    int id = candidates[pick(rng)];
    const auto& p = pmts.at(id);
    double d2 = (p[0]-tx)*(p[0]-tx) + (p[1]-ty)*(p[1]-ty) + (p[2]-tz)*(p[2]-tz);
    if (d2 < best_d2) { best_d2 = d2; best_id = id; }
  }
  return best_id;
}

// Gaussian noise hits on ID pool + small OD component.
// Matches Python GenerateNoiseHits() logic.
static void GenerateNoiseHits(
    int n_noise, float c_mean, float c_std,
    float t_mean, float t_std,
    const std::vector<int>& barrel,
    const std::vector<int>& top,
    const std::vector<int>& bottom,
    const std::vector<int>& od,
    const std::unordered_map<int, std::array<float,3>>& /*pmts*/,
    std::unordered_set<int>& used_ids,
    std::unordered_map<int, std::pair<float,float>>& hits,
    std::mt19937& rng) {

  std::normal_distribution<double> c_dist(c_mean, c_std);
  std::normal_distribution<double> t_dist(t_mean, t_std);

  // ID pool = barrel + top + bottom
  // Build combined reference without copying
  const std::vector<int>* id_segs[3] = { &barrel, &top, &bottom };
  int id_total = static_cast<int>(barrel.size() + top.size() + bottom.size());

  int n_od = 0;
  if (!od.empty()) {
    std::uniform_int_distribution<int> od_count(OD_NOISE_MIN, OD_NOISE_MAX);
    n_od = od_count(rng);
  }
  int n_id = std::max(0, n_noise - n_od);

  auto add_noise = [&](const std::vector<int>* segs[], int n_segs, int total, int n_target) {
    if (total == 0 || n_target <= 0) return;
    std::uniform_int_distribution<int> pick(0, total - 1);
    int done = 0, attempts = 0;
    while (done < n_target && attempts < n_target * 4) {
      attempts++;
      int flat = pick(rng);
      int id = -1;
      for (int s = 0; s < n_segs; s++) {
        int sz = static_cast<int>(segs[s]->size());
        if (flat < sz) { id = (*segs[s])[flat]; break; }
        flat -= sz;
      }
      if (id < 0 || used_ids.count(id)) continue;
      used_ids.insert(id);
      float c = std::max(0.05f, static_cast<float>(c_dist(rng)));
      float t = std::fabs(static_cast<float>(t_dist(rng)));
      hits[id] = {c, t};
      done++;
    }
  };

  add_noise(id_segs, 3, id_total, n_id);

  if (!od.empty() && n_od > 0) {
    const std::vector<int>* od_segs[1] = { &od };
    add_noise(od_segs, 1, static_cast<int>(od.size()), n_od);
  }
}

// ---------------------------------------------------------------------------
// Physics event (HE / LE / SHE / SLE)
// Cherenkov ring (ring_fraction of hits) + noise hits (rest).
// Translated directly from GeneratePhysicsEvent + GenerateRingHitPositions
// in mock_webstreamer.py.
// ---------------------------------------------------------------------------

std::string EventDisplayFakeData::GeneratePhysicsEvent(
    EventDisplayFakeData_args* args, const std::string& type) {

  const PhysicsParams* p = GetPhysicsParams(type);
  if (!p) return "[]";
  if (args->barrel_ids.empty() && args->top_ids.empty() && args->bottom_ids.empty())
    return "[]";

  std::normal_distribution<double> n_dist(p->n_hits_mean, p->n_hits_std);
  int n_total = std::max(10, static_cast<int>(n_dist(args->rng)));
  int n_ring  = std::max(5,  static_cast<int>(n_total * p->ring_fraction));
  int n_noise = n_total - n_ring;

  // Random vertex inside barrel at 50% radius
  std::uniform_real_distribution<double> uni01(0.0, 1.0);
  std::uniform_real_distribution<double> uni_phi(0.0, 2.0 * M_PI);
  double vr   = BARREL_RADIUS * 0.5 * std::sqrt(uni01(args->rng));
  double vphi = uni_phi(args->rng);
  double vx   = vr * std::cos(vphi);
  double vy   = vr * std::sin(vphi);
  double vz   = BARREL_Z_MIN*0.6 + (BARREL_Z_MAX*0.6 - BARREL_Z_MIN*0.6) * uni01(args->rng);

  // Random particle direction (matching Python: theta uniform [0.1, pi-0.1])
  std::uniform_real_distribution<double> theta_dist(0.1, M_PI - 0.1);
  double theta = theta_dist(args->rng);
  double phi   = uni_phi(args->rng);
  double dx    = std::sin(theta) * std::cos(phi);
  double dy    = std::sin(theta) * std::sin(phi);
  double dz    = std::cos(theta);

  // Orthonormal basis perpendicular to (dx,dy,dz) — matches Python exactly
  double e1x, e1y, e1z;
  if (std::fabs(dx) < 0.9) { e1x = dz;  e1y = 0.0; e1z = -dx; }
  else                      { e1x = 0.0; e1y = dz;  e1z = -dy; }
  double e1n = std::sqrt(e1x*e1x + e1y*e1y + e1z*e1z);
  e1x/=e1n; e1y/=e1n; e1z/=e1n;
  double e2x = dy*e1z - dz*e1y;
  double e2y = dz*e1x - dx*e1z;
  double e2z = dx*e1y - dy*e1x;

  double angle_rad = p->cherenkov_angle_deg * M_PI / 180.0;
  double sin_c     = std::sin(angle_rad);
  double cos_c     = std::cos(angle_rad);

  std::normal_distribution<double> smear(0.0, p->hit_smear);
  std::normal_distribution<double> t_ring_dist(p->t_ring_mean, p->t_ring_std);
  std::normal_distribution<double> c_ring_dist(p->c_ring_mean, p->c_ring_std);

  std::unordered_set<int> used_ids;
  std::unordered_map<int, std::pair<float,float>> hits;

  // Ring hits
  int ring_done = 0, ring_att = 0;
  while (ring_done < n_ring && ring_att < n_ring * 8) {
    ring_att++;
    double phi_r = uni_phi(args->rng);
    double rx = cos_c*dx + sin_c*(std::cos(phi_r)*e1x + std::sin(phi_r)*e2x);
    double ry = cos_c*dy + sin_c*(std::cos(phi_r)*e1y + std::sin(phi_r)*e2y);
    double rz = cos_c*dz + sin_c*(std::cos(phi_r)*e1z + std::sin(phi_r)*e2z);
    double rn = std::sqrt(rx*rx + ry*ry + rz*rz);
    rx/=rn; ry/=rn; rz/=rn;

    double hit_x = 0, hit_y = 0, hit_z = 0;
    bool found = false;

    // Barrel intersection
    double A    = rx*rx + ry*ry;
    double B    = 2.0*(vx*rx + vy*ry);
    double C    = vx*vx + vy*vy - BARREL_RADIUS*BARREL_RADIUS;
    double disc = B*B - 4.0*A*C;

    if (A > 1e-9 && disc >= 0) {
      double sq = std::sqrt(disc);
      // Try both roots in ascending order (matches Python's sorted([...]))
      double t_roots[2] = { (-B - sq)/(2.0*A), (-B + sq)/(2.0*A) };
      for (double ts : t_roots) {
        if (ts > 1.0) {   // > 1 cm minimum travel (Python uses > 10 in mm scale)
          double hz = vz + rz*ts;
          if (hz >= BARREL_Z_MIN && hz <= BARREL_Z_MAX) {
            hit_x = vx + rx*ts;
            hit_y = vy + ry*ts;
            hit_z = hz;
            found = true;
            break;
          }
        }
      }
    }
    // Endcap intersection
    if (!found && std::fabs(rz) > 1e-9) {
      for (double cap_z : { CAP_Z_TOP, CAP_Z_BOTTOM }) {
        double ts = (cap_z - vz) / rz;
        if (ts > 1.0) {
          double hx = vx + rx*ts;
          double hy = vy + ry*ts;
          if (hx*hx + hy*hy < BARREL_RADIUS*BARREL_RADIUS) {
            hit_x = hx; hit_y = hy; hit_z = cap_z;
            found = true;
            break;
          }
        }
      }
    }
    if (!found) continue;

    // Positional smear before nearest-PMT search (makes ring noisier)
    hit_x += smear(args->rng);
    hit_y += smear(args->rng);
    hit_z += smear(args->rng);

    // Select candidate pool by smeared position
    double r_hit = std::sqrt(hit_x*hit_x + hit_y*hit_y);
    const std::vector<int>* candidates;
    if (r_hit > BARREL_RADIUS * 0.85 && hit_z > BARREL_Z_MIN && hit_z < BARREL_Z_MAX)
      candidates = &args->barrel_ids;
    else if (hit_z > 0)
      candidates = &args->top_ids;
    else
      candidates = &args->bottom_ids;

    if (candidates->empty()) continue;

    int pid = FindNearestPmt(hit_x, hit_y, hit_z, *candidates, args->pmts, args->rng);
    if (used_ids.count(pid)) continue;
    used_ids.insert(pid);

    const auto& pm = args->pmts.at(pid);
    double dist = std::sqrt((pm[0]-vx)*(pm[0]-vx) +
                            (pm[1]-vy)*(pm[1]-vy) +
                            (pm[2]-vz)*(pm[2]-vz));
    float t = std::max(0.1f, static_cast<float>(dist / C_WATER + t_ring_dist(args->rng)));
    float c = std::max(0.05f, static_cast<float>(c_ring_dist(args->rng)));
    hits[pid] = {c, t};
    ring_done++;
  }

  // Noise hits: ID pool + 0-15 OD hits (matches Python GenerateNoiseHits)
  GenerateNoiseHits(
    n_noise, p->c_noise_mean, p->c_noise_std,
    p->t_noise_mean, p->t_noise_std,
    args->barrel_ids, args->top_ids, args->bottom_ids, args->od_ids,
    args->pmts, used_ids, hits, args->rng
  );

  return BuildHitsJson(hits);
}

// ---------------------------------------------------------------------------
// OD trigger event
// Localised Gaussian patch on OD barrel + small ID/mPMT spillover.
// Translated from GenerateODEvent() in mock_webstreamer.py.
// ---------------------------------------------------------------------------

std::string EventDisplayFakeData::GenerateODEvent(EventDisplayFakeData_args* args) {
  if (args->od_ids.empty()) return "[]";

  // OD trigger params (from Python OD_TRIGGER_PARAMS)
  const int    n_od_mean      = 200;
  const int    n_od_std       =  60;
  const int    n_spill_mean   =  15;
  const int    n_spill_std    =   8;
  const float  c_mean         = 1.8f;
  const float  c_std          = 0.8f;
  const float  t_mean         = 30.0f;
  const float  t_std          = 15.0f;
  const double phi_spread     = 0.6;    // rad
  const double z_spread       = 800.0;  // cm

  std::uniform_real_distribution<double> uni_phi(0.0, 2.0 * M_PI);
  std::uniform_real_distribution<double> uni_z(BARREL_Z_MIN * 0.7, BARREL_Z_MAX * 0.7);
  double patch_phi = uni_phi(args->rng);
  double patch_z   = uni_z(args->rng);

  std::normal_distribution<double> n_od_dist(n_od_mean, n_od_std);
  std::normal_distribution<double> c_dist(c_mean, c_std);
  std::normal_distribution<double> t_dist(t_mean, t_std);
  std::uniform_real_distribution<double> prob_pick(0.0, 1.0);

  std::unordered_set<int> used_ids;
  std::unordered_map<int, std::pair<float,float>> hits;

  int n_od = std::max(10, static_cast<int>(n_od_dist(args->rng)));
  std::uniform_int_distribution<int> od_pick(0, static_cast<int>(args->od_ids.size()) - 1);

  int od_done = 0, od_att = 0;
  while (od_done < n_od && od_att < n_od * 6) {
    od_att++;
    int pid = args->od_ids[od_pick(args->rng)];
    if (used_ids.count(pid)) continue;

    const auto& pm = args->pmts.at(pid);
    double pmt_phi = std::atan2(pm[1], pm[0]);
    double dphi    = std::fabs(std::atan2(
        std::sin(pmt_phi - patch_phi), std::cos(pmt_phi - patch_phi)));
    double dz      = std::fabs(pm[2] - patch_z);

    // Gaussian acceptance probability (matches Python)
    double prob = std::exp(-0.5 * (dphi/phi_spread) * (dphi/phi_spread)) *
                  std::exp(-0.5 * (dz  /z_spread  ) * (dz  /z_spread  ));
    if (prob_pick(args->rng) > prob) continue;

    used_ids.insert(pid);
    double r3 = std::sqrt(pm[0]*pm[0] + pm[1]*pm[1] + pm[2]*pm[2]);
    float t = std::max(0.1f, static_cast<float>(r3 / C_WATER + t_dist(args->rng)));
    float c = std::max(0.05f, static_cast<float>(c_dist(args->rng)));
    hits[pid] = {c, t};
    od_done++;
  }

  // Small ID/mPMT spillover — random, not localised (matches Python)
  std::normal_distribution<double> n_spill_dist(n_spill_mean, n_spill_std);
  int n_spill = std::max(0, static_cast<int>(n_spill_dist(args->rng)));

  std::vector<int> id_pool;
  id_pool.insert(id_pool.end(), args->barrel_ids.begin(), args->barrel_ids.end());
  id_pool.insert(id_pool.end(), args->top_ids.begin(),    args->top_ids.end());
  id_pool.insert(id_pool.end(), args->bottom_ids.begin(), args->bottom_ids.end());

  if (!id_pool.empty() && n_spill > 0) {
    std::uniform_int_distribution<int> spill_pick(0, static_cast<int>(id_pool.size()) - 1);
    std::normal_distribution<double> t_spill(t_mean, t_std * 2.0);
    std::normal_distribution<double> c_spill(c_mean * 0.6, c_std);
    int done = 0, att = 0;
    while (done < n_spill && att < n_spill * 4) {
      att++;
      int pid = id_pool[spill_pick(args->rng)];
      if (used_ids.count(pid)) continue;
      used_ids.insert(pid);
      float t = std::fabs(static_cast<float>(t_spill(args->rng)));
      float c = std::max(0.05f, static_cast<float>(c_spill(args->rng)));
      hits[pid] = {c, t};
      done++;
    }
  }

  return BuildHitsJson(hits);
}

// ---------------------------------------------------------------------------
// Calibration event
// Picks a random source PMT (collimator or diffuser), aims beam inward,
// and illuminates a patch of ID PMTs on the opposite wall.
// The hits are on ID PMTs — NOT on the calibration source PMTs.
// Translated from GenerateCalibEvent() in mock_webstreamer.py.
// ---------------------------------------------------------------------------

std::string EventDisplayFakeData::GenerateCalibEvent(EventDisplayFakeData_args* args) {
  // Randomly choose collimator or diffuser
  std::uniform_real_distribution<double> uni01(0.0, 1.0);
  bool use_col = (uni01(args->rng) < 0.5);

  const std::vector<int>& source_pool =
      use_col ? args->calib_col_ids : args->calib_dif_ids;

  // Fallback: try the other pool if chosen one is empty
  if (source_pool.empty()) {
    const std::vector<int>& other =
        use_col ? args->calib_dif_ids : args->calib_col_ids;
    if (other.empty()) return "[]";
    use_col = !use_col;
  }
  const std::vector<int>& src = use_col ? args->calib_col_ids : args->calib_dif_ids;

  // Calibration params (from Python COLLIMATOR_PARAMS / DIFFUSER_PARAMS)
  float half_angle_deg = use_col ?  3.0f :  40.0f;
  int   n_hits_mean    = use_col ?    30 :    150;
  int   n_hits_std     = use_col ?    10 :     40;
  float c_mean         = use_col ?  1.5f :   1.2f;
  float c_std          = use_col ?  0.5f :   0.4f;
  float t_smear        = use_col ?  2.0f :   3.0f;
  int   n_noise        = 5;

  // Pick a random source PMT
  std::uniform_int_distribution<int> src_pick(0, static_cast<int>(src.size()) - 1);
  int source_id = src[src_pick(args->rng)];
  const auto& sp = args->pmts.at(source_id);
  double sx = sp[0], sy = sp[1], sz = sp[2];

  // Beam direction: from source toward detector centre (inward-pointing)
  double source_phi = std::atan2(sy, sx);
  double bdx = -std::cos(source_phi);
  double bdy = -std::sin(source_phi);
  double bdz = 0.0;

  // Cap sources point straight down/up (matches Python is_cap_source logic)
  double source_r = std::sqrt(sx*sx + sy*sy);
  if (source_r < 1000.0) {
    bdx = 0.0; bdy = 0.0;
    bdz = (sz > 0) ? -1.0 : 1.0;
  }

  // Beam hits the opposite wall ~2*R away
  double beam_travel = 2.0 * BARREL_RADIUS;
  double patch_cx = sx + bdx * beam_travel;
  double patch_cy = sy + bdy * beam_travel;
  double patch_cz = sz + bdz * beam_travel;
  patch_cz = std::max(BARREL_Z_MIN, std::min(BARREL_Z_MAX, patch_cz));

  double half_angle_rad = half_angle_deg * M_PI / 180.0;
  double patch_radius   = beam_travel * std::tan(half_angle_rad);

  // All ID + mPMT PMTs
  std::vector<int> id_pool;
  id_pool.insert(id_pool.end(), args->barrel_ids.begin(), args->barrel_ids.end());
  id_pool.insert(id_pool.end(), args->top_ids.begin(),    args->top_ids.end());
  id_pool.insert(id_pool.end(), args->bottom_ids.begin(), args->bottom_ids.end());

  // Find ID PMTs within the beam patch (FindPmtsWithinPatch in Python)
  std::vector<int> patch_pmts;
  for (int pid : id_pool) {
    const auto& pm = args->pmts.at(pid);
    double d2 = (pm[0]-patch_cx)*(pm[0]-patch_cx) +
                (pm[1]-patch_cy)*(pm[1]-patch_cy) +
                (pm[2]-patch_cz)*(pm[2]-patch_cz);
    if (std::sqrt(d2) <= patch_radius)
      patch_pmts.push_back(pid);
  }

  std::normal_distribution<double> n_dist(n_hits_mean, n_hits_std);
  std::normal_distribution<double> c_dist(c_mean, c_std);
  std::normal_distribution<double> t_dist(0.0, t_smear);

  int n_hits = std::max(5, static_cast<int>(n_dist(args->rng)));

  std::unordered_set<int> used_ids;
  std::unordered_map<int, std::pair<float,float>> hits;

  if (!patch_pmts.empty()) {
    // Sample min(n_hits, patch_pmts.size()) without replacement (partial shuffle)
    int n_select = std::min(n_hits, static_cast<int>(patch_pmts.size()));
    for (int i = 0; i < n_select; i++) {
      std::uniform_int_distribution<int> d(i, static_cast<int>(patch_pmts.size()) - 1);
      std::swap(patch_pmts[i], patch_pmts[d(args->rng)]);
    }
    for (int i = 0; i < n_select; i++) {
      int pid = patch_pmts[i];
      used_ids.insert(pid);
      const auto& pm = args->pmts.at(pid);
      double dist = std::sqrt((pm[0]-sx)*(pm[0]-sx) +
                              (pm[1]-sy)*(pm[1]-sy) +
                              (pm[2]-sz)*(pm[2]-sz));
      float t = std::max(0.1f, static_cast<float>(dist / C_WATER + t_dist(args->rng)));
      float c = std::max(0.05f, static_cast<float>(c_dist(args->rng)));
      hits[pid] = {c, t};
    }
  }

  // Small noise contribution (matches Python noise_hits=5, 0.8±0.3, 200±150 ns)
  GenerateNoiseHits(
    n_noise, 0.8f, 0.3f, 200.0f, 150.0f,
    args->barrel_ids, args->top_ids, args->bottom_ids, args->od_ids,
    args->pmts, used_ids, hits, args->rng
  );

  return BuildHitsJson(hits);
}

// ---------------------------------------------------------------------------
// Thread — called repeatedly by ToolDAQ's thread runner
// ---------------------------------------------------------------------------

void EventDisplayFakeData::Thread(Thread_args* arg) {
  EventDisplayFakeData_args* args = reinterpret_cast<EventDisplayFakeData_args*>(arg);

  // Weighted random type selection (matches Python EVENT_TYPE_WEIGHTS)
  static const std::vector<std::pair<const char*, float>> type_weights = {
    { "HE",    0.4750f },
    { "LE",    0.4750f },
    { "SHE",   0.0125f },
    { "SLE",   0.0125f },
    { "OD",    0.0125f },
    { "calib", 0.0125f },
  };

  std::uniform_real_distribution<float> uni(0.0f, 1.0f);
  float r = uni(args->rng);
  float cumulative = 0.0f;
  std::string event_type = type_weights.back().first;
  for (const auto& kv : type_weights) {
    cumulative += kv.second;
    if (r < cumulative) { event_type = kv.first; break; }
  }

  // Generate hits
  std::string hits_json;
  if (event_type == "OD") {
    hits_json = GenerateODEvent(args);
  } else if (event_type == "calib") {
    hits_json = GenerateCalibEvent(args);
  } else {
    hits_json = GeneratePhysicsEvent(args, event_type);
  }

  // Timestamp
  auto now = std::chrono::system_clock::now();
  auto t   = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ts;
  ts << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
  std::string time_iso = ts.str();

  long event_number = args->event_counter++;

  // Write to DB
  FakeDataDB_WriteEventDisplay(args->db_conn, event_number, time_iso, event_type, hits_json);

  // Broadcast over WebSocket
  std::string payload =
    "{\"event_number\":" + std::to_string(event_number) +
    ",\"time\":\""       + time_iso + "\""
    ",\"type\":\""       + event_type + "\""
    ",\"data\":"         + hits_json + "}";

  // Do NOT call MessageClear/ConnectMessageClear — keep last event in channel
  // buffer so clients connecting between broadcasts get it immediately.
  args->m_data->channels["event_display"].Send(payload, event_type);

  sleep(args->broadcast_interval_s);
}
