#include <fmt/format.h>
#include <iostream>
#include <list>
#include "openai.hpp"
#include "json.hpp"

/*

  Example usage:

  using namespace ai;
  std::string assistant_msg("You are a programming assistant who ONLY responds with JSON objects.");
  std::string query("Your query goes here");
  try {
    ai_stream ai({.maxRetries = 3 });
    ai << ai_config::GPT_35
       << json({
	   {"role", "assistant"},
	   {"content", assistant_msg.c_str()}
	 })
       << json({
	   {"role", "user"},
	   {"content", query.c_str()}
	 });
    json j;
    ai >> j; // performs query
    std::cout << "j = " << j.dump() << std::endl;
    ai_stats stats;
    ai >> stats;
    std::cout << "total tokens = " << stats.total_tokens << std::endl;
    
  } catch (const ai_exception& e) {
    std::cerr << e.what() << std::endl;
  }
  
 */

using namespace nlohmann;
using namespace openai;

#include <functional>

namespace ai {
  
  enum class ai_config { GPT_35 = 1, GPT_4 = 2 };
  enum class ai_exception_value { NO_KEY_DEFINED, INVALID_KEY, TOO_MANY_RETRIES };

  class ai_stats {
  public:
    unsigned int completion_tokens = 0;
    unsigned int prompt_tokens = 0;
    unsigned int total_tokens = 0;
  };

  class ai_validate {
  public:
    explicit ai_validate(bool validator(const json&))
      : validator (validator)
    {
    }
    std::function<bool(const json&)> validator = [](const json&){ return true; };
  };
  
  
  class ai_exception {
  public:
    explicit ai_exception(ai_exception_value e, const std::string& msg) {
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
      : _maxRetries (p.maxRetries),
	_apiKey (p.apiKey),
	_keyName (p.keyName)
    {
      openai::start();
      _key = _apiKey;
      if (_key == "") {
	// Try to fetch the key from the environment if no key was provided.
	std::string envKey(std::getenv(_keyName.c_str()));
	if (envKey != "") {
	  _key = envKey;
	}
      }
      // std::cout << "KEY [" << _key << "]" << std::endl;
      // Check environment.
      // Throw exception if no key found or provided.
      if (_key == "") {
	throw ai_exception(ai_exception_value::NO_KEY_DEFINED,
			   fmt::format("There was no key defined in the constructor or in the environment variable {}.", _keyName.c_str()));
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
  
    // Overload << operator for validation
    ai_stream& operator<<(const ai_validate& v) {
      _validator = v.validator;
      return *this;
    }
  
    // Overload << operator to send queries
    ai_stream& operator<<(const json& js) {
      _messages.push_back(js);
      return *this;
    }

    // Overload >> operator to save stats
    ai_stream& operator>>(ai_stats& stats) {
      stats = _stats;
      return *this;
    }
      
    // Overload >> operator to read responses
    ai_stream& operator>>(json& response_json) {
      response_json = {{}};
      auto retries = _maxRetries;
      while (true) {
	if (retries == 0) {
	  throw ai_exception(ai_exception_value::TOO_MANY_RETRIES,
			     fmt::format("Maximum number of retries exceeded ({}).", _maxRetries));
	}
	try {
	  json j;
	  j["model"] = _model;
	  j["messages"] = _messages;
	  auto chat = openai::chat().create(j);
	  _result = chat["choices"][0]["message"]["content"].get<std::string>();
	  _stats.completion_tokens += chat["usage"]["completion_tokens"].get<unsigned int>();
	  _stats.prompt_tokens += chat["usage"]["prompt_tokens"].get<unsigned int>();
	  _stats.total_tokens += chat["usage"]["total_tokens"].get<unsigned int>();
	  response_json = json::parse(_result);
	  if (_validator(response_json)) {
	    break;
	  }
	}
	catch (nlohmann::json_abi_v3_11_2::detail::parse_error& pe) {
	  // Retry if there were JSON parse errors.
	}
	catch (nlohmann::json_abi_v3_11_2::detail::type_error& te) {
	  // Retry if there were JSON parse errors.
	}
	catch (std::runtime_error& e) {
	  std::string msg(e.what());
	  // If we find "API key" in the message, we assume we had an invalid key.
	  if (msg.find("API key") != std::string::npos) {
	    throw ai_exception(ai_exception_value::INVALID_KEY,
			       fmt::format("The API key ({}) was invalid.", _key.c_str()));
	  } else {
	    // Otherwise, pass up the exception.
	    throw e;
	  }
	}
	retries -= 1;
	std::cerr << "Retry." << std::endl;
      }
      return *this;
    }

    void clearHistory() {
      // Clears chat history.
      _result = "";
      _messages.clear();
    }
  
  private:
    std::string _key;
    std::list<json> _messages;
    std::string _model;
    std::string _result;
    OpenAI _ai;
    const unsigned int _maxRetries;
    const std::string _apiKey;
    const std::string _keyName;
    ai_stats _stats;
    std::function<bool(const json&)> _validator = [](const json&){ return true; };
  };

}

