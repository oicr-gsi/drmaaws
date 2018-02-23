#include <sstream>
#include <pistache/endpoint.h>
#include <pistache/router.h>
#include <json/json.h>
#include "stateful.hpp"

using namespace Pistache;

class Controller {
public:
  Controller(const std::shared_ptr<StatefulDrmaa> &state) throw(
      drmaa::exception)
      : statefulDrmaa(state) {}
  void run(const Rest::Request &request, Http::ResponseWriter writer) {

    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    Json::Value value;
    JSONCPP_STRING errs;
    std::istringstream is(request.body());
    if (!Json::parseFromStream(builder, is, &value, &errs)) {
      writer.send(Http::Code::Bad_Request, errs);
      return;
    }
    if (!value.isObject()) {
      writer.send(Http::Code::Bad_Request, "Request is not a JSON object");
      return;
    }

    JobRequest job;
    for (auto attribute = value.begin(); attribute != value.end();
         attribute++) {
      if (attribute->isString()) {
        job.attributes()[attribute.name()] = attribute->asString();
      } else if (attribute->isArray()) {
        std::vector<std::string> items;

        for (auto item : *attribute) {
          if (!item.isString()) {
            writer.send(Http::Code::Bad_Request,
                        "Element in array is not a string.");
            return;
          }
          items.push_back(item.asString());
        }
        job.v_attributes()[attribute.name()] = std::move(items);
      } else {
        writer.send(Http::Code::Bad_Request,
                    "Argument must be array or string.");
      }
    }
    try {
      auto status = statefulDrmaa->run(job);
      auto response = writer.stream(Http::Code::Ok);
      response << "\"" << status.c_str() << "\"" << Http::ends;
    } catch (drmaa::exception &e) {
      writer.send(Http::Code::Conflict, e.what());
    }
  }

  void listAttributes(const Rest::Request &request,
                      Http::ResponseWriter writer) {
    try {
      Json::Value value(Json::objectValue);

      for (auto name : drmaa::attribute_names()) {
        value[name] = false;
      }
      for (auto name : drmaa::attribute_namesv()) {
        value[name] = true;
      }
      Json::StyledWriter jsonWriter;
      auto json = jsonWriter.write(value);
      auto response = writer.stream(Http::Code::Ok);
      response << json.c_str() << Http::ends;
    } catch (drmaa::exception &e) {
      writer.send(Http::Code::Internal_Server_Error, e.what());
    }
  }

private:
  std::shared_ptr<StatefulDrmaa> statefulDrmaa;
};

int main() {
  auto statefulDrmaa = std::make_shared<StatefulDrmaa>();
  Controller controller(statefulDrmaa);

  Rest::Router router;
  Rest::Routes::Post(router, "/run",
                     Rest::Routes::bind(&Controller::run, &controller));
  Rest::Routes::Get(
      router, "/attributes",
      Rest::Routes::bind(&Controller::listAttributes, &controller));
  auto options = Http::Endpoint::options().threads(1);
  Address address = "*:9080";
  Http::Endpoint endpoint(address);
  endpoint.init(options);
  endpoint.setHandler(router.handler());
  endpoint.serve();
}
