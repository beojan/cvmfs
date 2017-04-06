/**
 * This file is part of the CernVM File System.
 */

#include "reactor.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "../json_document.h"

#include "../logging.h"
#include "session_token.h"
#include "util/pointer.h"
#include "util/string.h"

namespace receiver {

Reactor::Request Reactor::ReadRequest(int fd, std::string* data) {
  using namespace receiver;  // NOLINT

  // First, read the command identifier
  int32_t req_id = 0;
  int nb = read(fd, &req_id, 4);

  if (nb != 4) {
    return kError;
  }

  // Then, read message size
  int32_t msg_size = 0;
  nb = read(fd, &msg_size, 4);

  if (req_id == kError || nb != 4) {
    return kError;
  }

  // Finally read the message body
  if (msg_size > 0) {
    std::vector<char> buffer(msg_size);
    nb = read(fd, &buffer[0], msg_size);

    if (nb != msg_size) {
      return kError;
    }

    *data = std::string(&buffer[0], msg_size);
    return static_cast<Request>(req_id);
  }

  return kQuit;
}

bool Reactor::WriteRequest(int fd, Request req, const std::string& data) {
  const int32_t msg_size = data.size();
  const int32_t total_size = 8 + data.size();  // req + msg_size + data

  std::vector<char> buffer(total_size);

  memcpy(&buffer[0], &req, 4);
  memcpy(&buffer[4], &msg_size, 4);

  if (!data.empty()) {
    memcpy(&buffer[8], &data[0], data.size());
  }

  int nb = write(fd, &buffer[0], total_size);

  return nb == total_size;
}

bool Reactor::ReadReply(int fd, std::string* data) {
  int32_t msg_size(0);
  int nb = read(fd, &msg_size, 4);

  if (nb != 4) {
    return false;
  }

  std::vector<char> buffer(msg_size);
  nb = read(fd, &buffer[0], msg_size);

  if (nb != msg_size) {
    return false;
  }

  *data = std::string(&buffer[0], msg_size);

  return true;
}

bool Reactor::WriteReply(int fd, const std::string& data) {
  const int32_t msg_size = data.size();
  const int32_t total_size = 4 + data.size();

  std::vector<char> buffer(total_size);

  memcpy(&buffer[0], &msg_size, 4);

  if (!data.empty()) {
    memcpy(&buffer[4], &data[0], data.size());
  }

  int nb = write(fd, &buffer[0], total_size);

  return nb == total_size;
}

Reactor::Reactor(int fdin, int fdout) : fdin_(fdin), fdout_(fdout) {}

Reactor::~Reactor() {}

bool Reactor::run() {
  std::string msg_body;
  Request req = kQuit;
  do {
    msg_body.clear();
    req = ReadRequest(fdin_, &msg_body);
    if (!HandleRequest(fdout_, req, msg_body)) {
      LogCvmfs(kLogCvmfs, kLogStderr,
               "Reactor: could not handle request. Exiting");
      return false;
    }
  } while (req != kQuit);

  return true;
}

int Reactor::HandleGenerateToken(const std::string& req, std::string* reply) {
  if (reply == NULL) {
    return 1;
  }

  UniquePtr<JsonDocument> req_json(JsonDocument::Create(req));
  if (!req_json.IsValid()) {
    return 2;
  }

  const JSON* key_id =
      JsonDocument::SearchInObject(req_json->root(), "key_id", JSON_STRING);
  const JSON* path =
      JsonDocument::SearchInObject(req_json->root(), "path", JSON_STRING);
  const JSON* max_lease_time = JsonDocument::SearchInObject(
      req_json->root(), "max_lease_time", JSON_INT);

  if (key_id == NULL || path == NULL || max_lease_time == NULL) {
    return 3;
  }

  std::string session_token;
  std::string public_token_id;
  std::string token_secret;

  if (receiver::GenerateSessionToken(key_id->string_value, path->string_value,
                                     max_lease_time->int_value, &session_token,
                                     &public_token_id, &token_secret)) {
    return 4;
  }

  json_string_input input;
  input.push_back(std::make_pair("token", session_token.c_str()));
  input.push_back(std::make_pair("id", public_token_id.c_str()));
  input.push_back(std::make_pair("secret", token_secret.c_str()));

  ToJsonString(input, reply);

  return 0;
}

int Reactor::HandleGetTokenId(const std::string& req, std::string* reply) {
  if (reply == NULL) {
    return 1;
  }

  std::string token_id;
  json_string_input input;
  if (receiver::GetTokenPublicId(req, &token_id)) {
    input.push_back(std::make_pair("status", "error"));
    input.push_back(std::make_pair("reason", "invalid_token"));
  } else {
    input.push_back(std::make_pair("status", "ok"));
    input.push_back(std::make_pair("id", token_id.c_str()));
  }

  ToJsonString(input, reply);
  return 0;
}

int Reactor::HandleCheckToken(const std::string& req, std::string* reply) {
  if (reply == NULL) {
    return 1;
  }

  UniquePtr<JsonDocument> req_json(JsonDocument::Create(req));
  if (!req_json.IsValid()) {
    return 2;
  }

  const JSON* token =
      JsonDocument::SearchInObject(req_json->root(), "token", JSON_STRING);
  const JSON* secret =
      JsonDocument::SearchInObject(req_json->root(), "secret", JSON_STRING);

  if (token == NULL || secret == NULL) {
    return 3;
  }

  std::string path;
  json_string_input input;
  int ret =
      receiver::CheckToken(token->string_value, secret->string_value, &path);
  if (ret == 10) {
    // Expired token
    input.push_back(std::make_pair("status", "error"));
    input.push_back(std::make_pair("reason", "expired_token"));
  } else if (ret > 0) {
    // Invalid token
    input.push_back(std::make_pair("status", "error"));
    input.push_back(std::make_pair("reason", "invalid_token"));
  } else {
    // All ok
    input.push_back(std::make_pair("status", "ok"));
    input.push_back(std::make_pair("path", path.c_str()));
  }

  ToJsonString(input, reply);
  return 0;
}

int Reactor::HandleSubmitPayload(const std::string& /*req*/,
                                 std::string* /*reply*/) {
  return 0;
}

bool Reactor::HandleRequest(int fdout, Request req, const std::string& data) {
  bool ok = true;
  std::string reply;
  switch (req) {
    case kQuit:
      ok = WriteReply(fdout, "ok");
      break;
    case kEcho:
      ok = WriteReply(fdout, data);
      break;
    case kGenerateToken:
      ok &= (HandleGenerateToken(data, &reply) == 0);
      ok &= WriteReply(fdout, reply);
      break;
    case kGetTokenId:
      ok &= (HandleGetTokenId(data, &reply) == 0);
      ok &= WriteReply(fdout, reply);
      break;
    case kCheckToken:
      ok &= (HandleCheckToken(data, &reply) == 0);
      ok &= WriteReply(fdout, reply);
      break;
    case kSubmitPayload:
      // if (HandleSubmitPayload(data, &reply) == 0) {
      // ok = WriteReply(fdout, reply);
      //}
      break;
    case kError:
      LogCvmfs(kLogCvmfs, kLogStderr, "Reactor: unknown command received.");
      ok = false;
      break;
    default:
      break;
  }

  return ok;
}

}  // namespace receiver
