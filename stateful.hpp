#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>
#include "drmaapp.hpp"

class JobRequest {
public:
  JobRequest();

  std::map<std::string, std::string> &attributes();
  std::map<std::string, std::vector<std::string>> &v_attributes();
  const std::map<std::string, std::string> &attributes() const;
  const std::map<std::string, std::vector<std::string>> &v_attributes() const;
  std::string str() const;

private:
  std::map<std::string, std::string> attrs;
  std::map<std::string, std::vector<std::string>> v_attrs;
};

class StatefulDrmaa {
public:
  StatefulDrmaa() throw(drmaa::exception);

  std::string run(const JobRequest &job) throw(drmaa::exception);

  size_t cacheSize() const;
  size_t dbSize();

private:
  std::shared_ptr<drmaa::session> sess;
  SQLite::Database db;
  std::map<std::string, std::shared_ptr<drmaa::job>> jobs;
};
