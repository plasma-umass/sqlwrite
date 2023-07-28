#include <fmt/format.h>
#include <iostream>
#include <list>
#include "openai.hpp"
#include "json.hpp"

using namespace nlohmann;
using namespace openai;

namespace ai {
  
  enum class ai_config { GPT_35 = 1, GPT_4 = 2 };
  enum class ai_exception_value { NO_KEY_DEFINED, TOO_MANY_RETRIES };

  class ai_exception {
  public:
    ai_exception(ai_exception_value e, const std::string& msg) {
      _exception = e;
      _msg = msg;
    }
    std::string what() const {
      return _msg;
    }
  private:
    ai_exception_value _exception;
    std::string _msg;
  };

  class ai_stream {
  public:
    struct params {
      const std::string& apiKey = "";
      const std::string& keyName = "OPENAI_API_KEY";
      const unsigned int maxRetries = 3;
    };
    explicit ai_stream(params p)
    {
      _key = p.apiKey;
      _maxRetries = p.maxRetries;
      if (_key == "") {
	// Try to fetch the key from the environment if no key was provided.
	std::string envKey(std::getenv(p.keyName.c_str()));
	if (envKey != "") {
	  _key = envKey;
	}
      }
      // std::cout << "KEY [" << _key << "]" << std::endl;
      // Check environment.
      // Throw exception if no key found or provided.
      if (_key == "") {
	throw ai_exception(ai_exception_value::NO_KEY_DEFINED,
			   fmt::format("There was no key defined in the constructor or in the environment variable {}.", p.keyName.c_str()));
      }
    }

    // Overload << operator for configuration
    ai_stream& operator<<(const ai_config& config) {
      switch (config) {
      case ai_config::GPT_35:
	_model = "gpt-3.5-turbo";
	// std::cout << "GPT 3.5" << std::endl;
	break;
      case ai_config::GPT_4:
	_model = "gpt-4.0";
	// std::cout << "GPT 4" << std::endl;
	break;
      };
      return *this;
    }
  
    // Overload << operator to send queries
    ai_stream& operator<<(const json& js) {
      _messages.push_back(js);
      return *this;
    }

    // Overload >> operator to read responses
    ai_stream& operator>>(json& response_json) {
      response_json = {{}};
      if (!_started) {
	openai::start();
	_started = true;
      }
      auto retries = _maxRetries;
      while (true) {
	if (retries == 0) {
	  throw ai_exception(ai_exception_value::TOO_MANY_RETRIES, fmt::format("Maximum number of retries exceeded ({}).", _maxRetries));
	}
	try {
	  json j;
	  j["model"] = _model;
	  j["messages"] = _messages;
	  // std::cout << "CHAT " << j.dump() << std::endl;
	  auto chat = openai::chat().create(j);
	  _result = chat["choices"][0]["message"]["content"].get<std::string>();
	  _completion_tokens += chat["usage"]["completion_tokens"].get<unsigned int>();
	  _prompt_tokens += chat["usage"]["prompt_tokens"].get<unsigned int>();
	  _total_tokens += chat["usage"]["total_tokens"].get<unsigned int>();
	  //	  std::cerr << _total_tokens << std::endl;
	  response_json = json::parse(_result);
	  break;
	} catch (nlohmann::json_abi_v3_11_2::detail::parse_error& pe) {
	  // Retry if there were JSON parse errors.
	} catch (nlohmann::json_abi_v3_11_2::detail::type_error& te) {
	  // Retry if there were JSON parse errors.
	}
	retries -= 1;
	std::cerr << "Retry." << std::endl;
      }
      _result = "";
      _messages.clear();
      return *this;
    }
  
  private:
    std::string _key;
    std::list<json> _messages;
    std::string _model;
    std::string _result;
    OpenAI _ai;
    bool _started = false;
    unsigned int _maxRetries;
    unsigned int _completion_tokens = 0;
    unsigned int _prompt_tokens = 0;
    unsigned int _total_tokens = 0;
  };

}

