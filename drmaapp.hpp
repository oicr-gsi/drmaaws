#pragma once
#include <memory>
#include <string>
#include <vector>

namespace drmaa {

class exception : public std::exception {
public:
  explicit exception(int errcode, const char *diagnosis);
  ~exception() throw();
  const char *what() const throw();

private:
  std::string diagnosis;
};

typedef enum { Unknown, Queued, Running, Suspended, Done, Failed } state;

typedef enum { None, User, System, UserAndSystem } reason;

std::vector<std::string> attribute_names() throw(exception);
std::vector<std::string> attribute_namesv() throw(exception);

class session {
public:
  session() throw(exception);
  ~session();
};

class job;
class job_result;

class job_template {
public:
  job_template(std::shared_ptr<session> &owner) throw(exception);
  ~job_template();

  std::string get(const std::string &name) throw(exception);
  std::vector<std::string> getv(const std::string &name) throw(exception);
  void set(const std::string &name, const std::string &value) throw(exception);
  void setv(const std::string &name,
            const std::vector<std::string> &values) throw(exception);

  std::shared_ptr<job> run() throw(exception);

private:
  std::shared_ptr<session> owner;
  void *impl;
};

class job {
public:
  job(std::shared_ptr<session> &owner, const char *id);
  job(std::shared_ptr<session> &owner, const std::string &id);

  bool suspend() throw(exception);
  bool resume() throw(exception);
  bool hold() throw(exception);
  bool release() throw(exception);
  bool kill() throw(exception);
  const std::string &name();
  std::shared_ptr<job_result> wait() throw(exception);

  std::pair<state, reason> operator*() throw(exception);

private:
  bool control(int action) throw(exception);
  std::string id;
  std::shared_ptr<session> owner;
};

class job_result {
public:
  job_result(const char *name, const std::pair<int, bool> &exit_status,
             const std::pair<std::string, bool> &signal_name, bool aborted);

  const std::pair<int, bool> &exited();
  const std::pair<std::string, bool> &signalled();
  bool aborted();
  const std::string &name();

private:
  std::string id;
  std::pair<int, bool> exit_status;
  std::pair<std::string, bool> signal_name;
  bool has_aborted;
};

std::shared_ptr<job_result>
wait(std::shared_ptr<session> &owner) throw(exception);
extern const std::string remote_command;
extern const std::string js_state;
extern const std::string wd;
extern const std::string job_category;
extern const std::string native_specification;
extern const std::string block_email;
extern const std::string start_time;
extern const std::string job_name;
extern const std::string input_path;
extern const std::string output_path;
extern const std::string error_path;
extern const std::string join_files;
extern const std::string transfer_files;
extern const std::string deadline_time;
extern const std::string wct_hlimit;
extern const std::string wct_slimit;
extern const std::string duration_hlimit;
extern const std::string duration_slimit;
extern const std::string v_argv;
extern const std::string v_env;
extern const std::string v_email;
}
