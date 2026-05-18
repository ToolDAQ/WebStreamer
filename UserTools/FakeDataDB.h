#ifndef FakeDataDB_H
#define FakeDataDB_H

// FakeDataDB.h — header-only libpq helpers shared by all fake data tools.

#include <libpq-fe.h>
#include <string>
#include <iostream>

inline PGconn* FakeDataDB_Connect() {
  PGconn* conn = PQconnectdb("dbname=daq user=root host=127.0.0.1");
  if (PQstatus(conn) != CONNECTION_OK) {
    std::cerr << "FakeDataDB: connection failed: " << PQerrorMessage(conn) << std::endl;
    PQfinish(conn);
    return nullptr;
  }
  return conn;
}

inline bool FakeDataDB_WriteMonitoring(PGconn* conn,
                                       const std::string& time_iso,
                                       const std::string& device,
                                       const std::string& subject,
                                       const std::string& data_json) {
  if (!conn) return false;
  const char* params[4] = { time_iso.c_str(), device.c_str(),
                             subject.c_str(), data_json.c_str() };
  PGresult* res = PQexecParams(conn,
    "INSERT INTO monitoring (time, device, subject, data) VALUES ($1, $2, $3, $4::jsonb)",
    4, nullptr, params, nullptr, nullptr, 0);
  bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
  if (!ok)
    std::cerr << "FakeDataDB_WriteMonitoring: " << PQerrorMessage(conn) << std::endl;
  PQclear(res);
  return ok;
}

inline bool FakeDataDB_WriteEventDisplay(PGconn* conn,
                                         long event_number,
                                         const std::string& time_iso,
                                         const std::string& type,
                                         const std::string& data_json) {
  if (!conn) return false;
  std::string evnum = std::to_string(event_number);
  const char* params[4] = { evnum.c_str(), time_iso.c_str(),
                             type.c_str(), data_json.c_str() };
  PGresult* res = PQexecParams(conn,
    "INSERT INTO event_display (event_number, time, type, data) "
    "VALUES ($1::bigint, $2::timestamptz, $3, $4::jsonb) "
    "ON CONFLICT (event_number) DO NOTHING",
    4, nullptr, params, nullptr, nullptr, 0);
  bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
  if (!ok)
    std::cerr << "FakeDataDB_WriteEventDisplay: " << PQerrorMessage(conn) << std::endl;
  PQclear(res);
  return ok;
}

inline long FakeDataDB_GetNextEventNumber(PGconn* conn) {
  if (!conn) return 1;
  PGresult* res = PQexec(conn,
    "SELECT COALESCE(MAX(event_number), 0) + 1 FROM event_display");
  long next = 1;
  if (PQresultStatus(res) == PGRES_TUPLES_OK &&
      PQntuples(res) > 0 && !PQgetisnull(res, 0, 0))
    next = std::stol(PQgetvalue(res, 0, 0));
  else
    std::cerr << "FakeDataDB_GetNextEventNumber: " << PQerrorMessage(conn) << std::endl;
  PQclear(res);
  return next;
}

#endif
