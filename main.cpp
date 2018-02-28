#include <sstream>
#include <pistache/endpoint.h>
#include <pistache/router.h>
#include <json/json.h>
#include <openssl/sha.h>
#include "stateful.hpp"

using namespace Pistache;

static int hexdigit(char c) {
  switch (c) {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '4':
    return 4;
  case '5':
    return 5;
  case '6':
    return 6;
  case '7':
    return 7;
  case '8':
    return 8;
  case '9':
    return 9;
  case 'a':
  case 'A':
    return 10;
  case 'b':
  case 'B':
    return 11;
  case 'c':
  case 'C':
    return 12;
  case 'd':
  case 'D':
    return 13;
  case 'e':
  case 'E':
    return 14;
  case 'f':
  case 'F':
    return 15;
  default:
    return -1;
  }
}

class Controller {
public:
  Controller(const std::shared_ptr<StatefulDrmaa> &state) throw(
      drmaa::exception)
      : statefulDrmaa(state) {}
  void run(const Rest::Request &request, Http::ResponseWriter writer) {
    static const char *psk = getenv("DRMAA_PSK");
    static const size_t psk_length = strlen(psk);

    auto authheader = request.headers().tryGetRaw("Authorization");
    if (authheader.isEmpty()) {
      writer.send(Http::Code::Bad_Request, "Request is not signed.");
      return;
    }
    auto authorization = authheader.get().value();
    if (authorization.compare(0, 7, "signed ") != 0) {
      writer.send(Http::Code::Bad_Request, "Request is not signed.");
      return;
    }

    SHA_CTX shaContext;
    if (!SHA1_Init(&shaContext)) {
      writer.send(Http::Code::Internal_Server_Error,
                  "Security checking error.");
      return;
    }
    if (!SHA1_Update(&shaContext, psk, psk_length)) {
      writer.send(Http::Code::Internal_Server_Error,
                  "Security checking error.");
      return;
    }
    if (!SHA1_Update(&shaContext, request.body().c_str(),
                     request.body().length())) {
      writer.send(Http::Code::Internal_Server_Error,
                  "Security checking error.");
      return;
    }
    unsigned char sum[SHA_DIGEST_LENGTH];
    if (!SHA1_Final(sum, &shaContext)) {
      writer.send(Http::Code::Internal_Server_Error,
                  "Security checking error.");
      return;
    }

    for (size_t i = 0; i < SHA_DIGEST_LENGTH; i++) {
      auto provided = hexdigit(authorization[7 + i * 2]) * 0x10 +
                      hexdigit(authorization[7 + i * 2 + 1]);
      if (sum[i] != provided) {
        writer.send(Http::Code::Unauthorized, "Invalid signature.");
        return;
      }
    }

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
  if (getenv("DRMAA_PSK") == nullptr) {
    std::cerr << "No preshared key set via DRMAA_PSK." << std::endl;
    return 1;
  }
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
