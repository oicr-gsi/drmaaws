#include <iostream>
#include <numeric>
#include <sstream>
#include "stateful.hpp"

static const char *determineStatus(drmaa::job &job) {
  auto status = job.wait();
  if (status) {
    if (status->exited().second && status->exited().first == 0) {
      return "SUCCEEDED";
    }
    return "FAILED";
  }

  switch ((*job).first) {
  case drmaa::Queued:
    return "QUEUED";
  case drmaa::Running:
    return "INFLIGHT";
  case drmaa::Suspended:
    return "WAITING";
  case drmaa::Done:
    return "SUCCEEDED";
  case drmaa::Failed:
    return "FAILED";
  }

  return nullptr;
}

JobRequest::JobRequest() {}

std::map<std::string, std::string> &JobRequest::attributes() { return attrs; }
std::map<std::string, std::vector<std::string>> &JobRequest::v_attributes() {
  return v_attrs;
}
const std::map<std::string, std::string> &JobRequest::attributes() const {
  return attrs;
}
const std::map<std::string, std::vector<std::string>> &
JobRequest::v_attributes() const {
  return v_attrs;
}

std::string JobRequest::str() const {
  std::stringstream output;
  for (auto attr : attrs) {
    output << (std::hash<std::string>{}(attr.second) * 31 +
               std::hash<std::string>{}(attr.first));
  }
  for (auto attr : v_attrs) {

    output << (std::accumulate(
                   attr.second.begin(), attr.second.end(), (size_t)0,
                   [](size_t a, const std::string &value) {
                     return a * 31 + std::hash<std::string>{}(value);
                   }) *
                   31 +
               std::hash<std::string>{}(attr.first));
  }
  return output.str();
}

StatefulDrmaa::StatefulDrmaa() throw(drmaa::exception)
    : sess(std::make_shared<drmaa::session>()),
      db("drmaaws.db3", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
  // When we start up, we need to initialise the database, if it isn't already
  db.exec("CREATE TABLE IF NOT EXISTS jobs (name text NOT NULL, drmaa text NOT "
          "NULL, status text NOT NULL DEFAULT 'UNKNOWN', updated_at DATETIME "
          "DEFAULT CURRENT_TIMESTAMP)");
  // And purge any ancient cruft
  db.exec(
      "DELETE FROM jobs WHERE julianday('now') - julianday(updated_at) > 10");

  // We've restarted from a crash and we need to figure out the status of all
  // in-flight jobs from when we were last running.
  SQLite::Statement query(db, "SELECT name, drmaa FROM jobs WHERE status IN "
                              "('INFLIGHT', 'QUEUED', 'THROTTLED', "
                              "'UNKNOWN', 'WAITING')");
  if (query.executeStep()) {
    jobs[query.getColumn(0).getString()] =
        std::make_shared<drmaa::job>(sess, query.getColumn(1).getString());
  }
}

std::string StatefulDrmaa::run(const JobRequest &job) throw(drmaa::exception) {
  auto job_id = job.str();

  auto it = jobs.find(job_id);
  if (it != jobs.end()) {
    std::cerr << job_id << ": Job exists in DRMAA: " << it->second->name()
              << std::endl;
    try {
      auto strstatus = determineStatus(*it->second);

      if (strstatus != nullptr) {
        SQLite::Statement count(db, "SELECT * FROM jobs WHERE name = ?");
        count.bind(1, job_id);
        if (count.executeStep()) {
          SQLite::Statement update(db, "UPDATE jobs SET updated_at = "
                                       "datetime('now'),status = ?, drmaa = ? "
                                       "WHERE name = ? ");
          update.bind(1, strstatus);
          update.bind(2, it->second->name());
          update.bind(3, job_id);
          update.exec();
        } else {
          SQLite::Statement insert(db, "INSERT jobs (name, "
                                       "drmaa, status) VALUES (?, ?, ?)");
          insert.bind(1, job_id);
          insert.bind(2, it->second->name());
          insert.bind(3, strstatus);
          insert.exec();
        }
        std::cerr << job_id << ": Status from DRMAA: " << strstatus
                  << std::endl;
        return strstatus;
      }
    } catch (std::exception &e) {
      // If the DRMAA client doesn't know what we're talking about, then stop
      // asking it and just rely on what's in the DB
      jobs.erase(job_id);
      std::cerr << job_id << ": DRMAA error for " << it->second->name() << ": "
                << e.what() << std::endl;
    }
  }

  SQLite::Statement query(db, "SELECT status FROM jobs WHERE name = ?");
  query.bind(1, job_id);
  if (query.executeStep()) {
    auto status = query.getColumn(0).getString();
    std::cerr << job_id << ": Cached status: " << status << std::endl;
    return status;
  }

  // This isn't something we know about, then it must be new. How exciting!
  drmaa::job_template tmpl(sess);
  for (auto attr : job.attributes()) {
    tmpl.set(attr.first, attr.second);
  }
  for (auto attr : job.v_attributes()) {
    tmpl.setv(attr.first, attr.second);
  }

  auto j = tmpl.run();
  jobs[job_id] = j;
  SQLite::Statement insert(db, "INSERT OR REPLACE INTO jobs (name, drmaa, "
                               "status) VALUES (?, ?, 'WAITING')");
  insert.bind(1, job_id);
  insert.bind(2, j->name());
  insert.exec();
  std::cerr << job_id << ": Started as " << j->name() << std::endl;
  return "QUEUED";
}

size_t StatefulDrmaa::cacheSize() const { return jobs.size(); }
size_t StatefulDrmaa::dbSize() {
  SQLite::Statement query(db, "SELECT COUNT(*) FROM jobs");
  if (query.executeStep()) {
    return query.getColumn(0).getInt();
  }
  return 0;
}
