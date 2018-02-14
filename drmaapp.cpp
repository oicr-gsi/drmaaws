#include <iostream>
#include "drmaa.h"
#include "drmaapp.hpp"

drmaa::exception::exception(int errcode_, const char *diagnosis_)
    : diagnosis(diagnosis_ == nullptr ? drmaa_strerror(errcode_) : diagnosis_) {
}
drmaa::exception::~exception() {}
const char *drmaa::exception::what() const throw() { return diagnosis.c_str(); }

drmaa::session::session() throw(drmaa::exception) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  int errcode = drmaa_init(nullptr, error_diagnosis, sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
}

drmaa::session::~session() {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  int errcode = drmaa_exit(error_diagnosis, sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    std::cerr << "Error in session destructor: " << drmaa_strerror(errcode)
              << " " << error_diagnosis << std::endl;
  }
}

drmaa::job_template::job_template(
    std::shared_ptr<drmaa::session> &owner_) throw(drmaa::exception)
    : owner(owner_), impl(nullptr) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  drmaa_job_template_t *jt;
  int errcode = drmaa_allocate_job_template(&jt, error_diagnosis,
                                            sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  } else {
    impl = jt;
  }
}

drmaa::job_template::~job_template() {
  if (impl != nullptr) {
    char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
    int errcode = drmaa_delete_job_template(
        (drmaa_job_template_t *)impl, error_diagnosis, sizeof(error_diagnosis));
    if (errcode != DRMAA_ERRNO_SUCCESS) {
      std::cerr << "Error in session destructor: " << drmaa_strerror(errcode)
                << " " << error_diagnosis << std::endl;
    }
  }
}

std::string
drmaa::job_template::get(const std::string &name) throw(drmaa::exception) {
  if (impl == nullptr) {
    return "";
  }

  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  char value[DRMAA_ATTR_BUFFER];
  int errcode = drmaa_get_attribute((drmaa_job_template_t *)impl, name.c_str(),
                                    value, sizeof(value), error_diagnosis,
                                    sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
  return value;
}

std::vector<std::string>
drmaa::job_template::getv(const std::string &name) throw(drmaa::exception) {
  if (impl == nullptr) {
    return {};
  }

  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  drmaa_attr_values_t *values;
  int errcode = drmaa_get_vector_attribute(
      (drmaa_job_template_t *)impl, name.c_str(), &values, error_diagnosis,
      sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
  std::vector<std::string> output;
  char current[DRMAA_ATTR_BUFFER];
  while (drmaa_get_next_attr_value(values, current, sizeof(current)) ==
         DRMAA_ERRNO_SUCCESS) {
    output.push_back(current);
  }
  return output;
}

void drmaa::job_template::set(
    const std::string &name, const std::string &value) throw(drmaa::exception) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  int errcode = drmaa_set_attribute((drmaa_job_template_t *)impl, name.c_str(),
                                    value.c_str(), error_diagnosis,
                                    sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
}

void drmaa::job_template::setv(
    const std::string &name,
    const std::vector<std::string> &values) throw(drmaa::exception) {
  const char *array[values.size() + 1];
  for (auto i = 0; i < values.size(); i++) {
    array[i] = values[i].c_str();
  }
  array[values.size()] = nullptr;

  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  int errcode = drmaa_set_vector_attribute((drmaa_job_template_t *)impl,
                                           name.c_str(), array, error_diagnosis,
                                           sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
}

template <int (*getter)(drmaa_attr_names_t **, char *, size_t)>
static std::vector<std::string> getnames() throw(drmaa::exception) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  drmaa_attr_names_t *names;
  int errcode = getter(&names, error_diagnosis, sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
  std::vector<std::string> output;
  char current[DRMAA_ATTR_BUFFER];
  while (drmaa_get_next_attr_name(names, current, sizeof(current)) ==
         DRMAA_ERRNO_SUCCESS) {
    output.push_back(current);
  }
  return output;
}
std::vector<std::string> drmaa::attribute_names() throw(drmaa::exception) {
  return getnames<&drmaa_get_attribute_names>();
}
std::vector<std::string> drmaa::attribute_namesv() throw(drmaa::exception) {
  return getnames<&drmaa_get_vector_attribute_names>();
}

std::shared_ptr<drmaa::job> drmaa::job_template::run() throw(exception) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  char id[DRMAA_JOBNAME_BUFFER];
  int errcode = drmaa_run_job(id, sizeof(id), (drmaa_job_template_t *)impl,
                              error_diagnosis, sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
  return std::make_shared<drmaa::job>(owner, id);
}

drmaa::job::job(std::shared_ptr<drmaa::session> &owner_, const char *id_)
    : owner(owner_), id(id_) {}

drmaa::job::job(std::shared_ptr<drmaa::session> &owner_, const std::string &id_)
    : owner(owner_), id(id_) {}

bool drmaa::job::control(int action) throw(drmaa::exception) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  int errcode = drmaa_control(id.c_str(), action, error_diagnosis,
                              sizeof(error_diagnosis));
  switch (errcode) {
  case DRMAA_ERRNO_SUCCESS:
    return true;
  case DRMAA_ERRNO_RESUME_INCONSISTENT_STATE:
  case DRMAA_ERRNO_SUSPEND_INCONSISTENT_STATE:
  case DRMAA_ERRNO_HOLD_INCONSISTENT_STATE:
  case DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE:
    return false;
  default:
    throw drmaa::exception(errcode, error_diagnosis);
  }
}

bool drmaa::job::suspend() throw(drmaa::exception) {
  return control(DRMAA_CONTROL_SUSPEND);
}
bool drmaa::job::resume() throw(drmaa::exception) {
  return control(DRMAA_CONTROL_RESUME);
}
bool drmaa::job::hold() throw(drmaa::exception) {
  return control(DRMAA_CONTROL_HOLD);
}
bool drmaa::job::release() throw(drmaa::exception) {
  return control(DRMAA_CONTROL_RELEASE);
}
bool drmaa::job::kill() throw(drmaa::exception) {
  return control(DRMAA_CONTROL_TERMINATE);
}
const std::string &drmaa::job::name() { return id; }

std::pair<drmaa::state, drmaa::reason> drmaa::job::
operator*() throw(exception) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  int ps;
  int errcode =
      drmaa_job_ps(id.c_str(), &ps, error_diagnosis, sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }
  switch (ps) {
  case DRMAA_PS_QUEUED_ACTIVE:
    return std::make_pair(drmaa::Queued, drmaa::None);
  case DRMAA_PS_SYSTEM_ON_HOLD:
    return std::make_pair(drmaa::Queued, drmaa::System);
  case DRMAA_PS_USER_ON_HOLD:
    return std::make_pair(drmaa::Queued, drmaa::User);
  case DRMAA_PS_USER_SYSTEM_ON_HOLD:
    return std::make_pair(drmaa::Queued, drmaa::UserAndSystem);
  case DRMAA_PS_RUNNING:
    return std::make_pair(drmaa::Running, drmaa::None);
  case DRMAA_PS_SYSTEM_SUSPENDED:
    return std::make_pair(drmaa::Suspended, drmaa::System);
  case DRMAA_PS_USER_SUSPENDED:
    return std::make_pair(drmaa::Suspended, drmaa::User);
  case DRMAA_PS_USER_SYSTEM_SUSPENDED:
    return std::make_pair(drmaa::Suspended, drmaa::UserAndSystem);
  case DRMAA_PS_DONE:
    return std::make_pair(drmaa::Done, drmaa::None);
  case DRMAA_PS_FAILED:
    return std::make_pair(drmaa::Failed, drmaa::None);
  default:
    return {Unknown, None};
  }
}

drmaa::job_result::job_result(const char *name,
                              const std::pair<int, bool> &exit_status_,
                              const std::pair<std::string, bool> &signal_name_,
                              bool aborted_)
    : id(name), exit_status(exit_status_), signal_name(signal_name_),
      has_aborted(aborted_) {}

const std::pair<int, bool> &drmaa::job_result::exited() { return exit_status; }
const std::pair<std::string, bool> &drmaa::job_result::signalled() {
  return signal_name;
}
bool drmaa::job_result::aborted() { return has_aborted; }

const std::string &drmaa::job_result::name() { return id; }

static std::shared_ptr<drmaa::job_result>
wait_for_job(const char *ids,
             std::shared_ptr<drmaa::session> &owner) throw(drmaa::exception) {
  char error_diagnosis[DRMAA_ERROR_STRING_BUFFER];
  char id[DRMAA_JOBNAME_BUFFER];
  int stat;
  drmaa_attr_values_t *rusage = nullptr;
  int errcode = drmaa_wait(ids, id, sizeof(id), &stat, DRMAA_TIMEOUT_NO_WAIT,
                           &rusage, error_diagnosis, sizeof(error_diagnosis));
  if (errcode == DRMAA_ERRNO_EXIT_TIMEOUT) {
    return {};
  }
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }

  if (rusage != nullptr) {
    drmaa_release_attr_values(rusage);
  }
  int exited;
  int exit_status = 0;
  int signalled;
  char signal_name[DRMAA_SIGNAL_BUFFER];
  int aborted;

  errcode =
      drmaa_wifexited(&exited, stat, error_diagnosis, sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }

  if (exited) {
    errcode = drmaa_wexitstatus(&exit_status, stat, error_diagnosis,
                                sizeof(error_diagnosis));
    if (errcode != DRMAA_ERRNO_SUCCESS) {
      throw drmaa::exception(errcode, error_diagnosis);
    }
  }

  errcode = drmaa_wifsignaled(&signalled, stat, error_diagnosis,
                              sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }

  if (signalled) {
    errcode = drmaa_wtermsig(signal_name, sizeof(signal_name), stat,
                             error_diagnosis, sizeof(error_diagnosis));
    if (errcode != DRMAA_ERRNO_SUCCESS) {
      throw drmaa::exception(errcode, error_diagnosis);
    }

  } else {

    signal_name[0] = '\0';
  }

  errcode = drmaa_wifaborted(&aborted, stat, error_diagnosis,
                             sizeof(error_diagnosis));
  if (errcode != DRMAA_ERRNO_SUCCESS) {
    throw drmaa::exception(errcode, error_diagnosis);
  }

  std::pair<int, bool> exit_state{exit_status, exited != 0};
  std::pair<std::string, bool> signal_state{signal_name, signalled != 0};
  return std::make_shared<drmaa::job_result>(id, exit_state, signal_state,
                                             aborted != 0);
}

std::shared_ptr<drmaa::job_result>
drmaa::wait(std::shared_ptr<drmaa::session> &owner) throw(drmaa::exception) {
  return wait_for_job(DRMAA_JOB_IDS_SESSION_ANY, owner);
}

std::shared_ptr<drmaa::job_result> drmaa::job::wait() throw(drmaa::exception) {
  return wait_for_job(id.c_str(), owner);
}

const std::string drmaa::remote_command = DRMAA_REMOTE_COMMAND;
const std::string drmaa::js_state = DRMAA_JS_STATE;
const std::string drmaa::wd = DRMAA_WD;
const std::string drmaa::job_category = DRMAA_JOB_CATEGORY;
const std::string drmaa::native_specification = DRMAA_NATIVE_SPECIFICATION;
const std::string drmaa::block_email = DRMAA_BLOCK_EMAIL;
const std::string drmaa::start_time = DRMAA_START_TIME;
const std::string drmaa::job_name = DRMAA_JOB_NAME;
const std::string drmaa::input_path = DRMAA_INPUT_PATH;
const std::string drmaa::output_path = DRMAA_OUTPUT_PATH;
const std::string drmaa::error_path = DRMAA_ERROR_PATH;
const std::string drmaa::join_files = DRMAA_JOIN_FILES;
const std::string drmaa::transfer_files = DRMAA_TRANSFER_FILES;
const std::string drmaa::deadline_time = DRMAA_DEADLINE_TIME;
const std::string drmaa::wct_hlimit = DRMAA_WCT_HLIMIT;
const std::string drmaa::wct_slimit = DRMAA_WCT_SLIMIT;
const std::string drmaa::duration_hlimit = DRMAA_DURATION_HLIMIT;
const std::string drmaa::duration_slimit = DRMAA_DURATION_SLIMIT;
const std::string drmaa::v_argv = DRMAA_V_ARGV;
const std::string drmaa::v_env = DRMAA_V_ENV;
const std::string drmaa::v_email = DRMAA_V_EMAIL;
