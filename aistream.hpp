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
    aistream ai({.maxRetries = 3 });
    ai << config::GPT_3_5
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
    stats stats;
    ai >> stats;
    std::cout << "total tokens = " << stats.total_tokens << std::endl;
    
  } catch (const ai::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  
 */

using namespace nlohmann;
using namespace openai;

#include <functional>

namespace ai {
  
  enum class config { GPT_3_5, GPT_4_0 };
  enum class exception_value { NO_KEY_DEFINED, INVALID_KEY, TOO_MANY_RETRIES, OTHER };

  class stats {
  public:
    unsigned int completion_tokens = 0;
    unsigned int prompt_tokens = 0;
    unsigned int total_tokens = 0;
  };

  class validator {
  public:
    explicit validator(std::function<bool(const json&)> v)
      : function (v)
    {
    }
    std::function<bool(const json&)> function = [](const json&){ return true; };
  };
  
  
  class exception {
  public:
    explicit exception(exception_value e, const std::string& msg) {
      _exception = e;
      _msg = msg;
    }
    std::string what() const {
      return _msg;
    }
  private:
    exception_value _exception;
    std::string _msg;
  };

  class aistream {
  public:
    struct params {
      const std::string& apiKey = "";
      const std::string& keyName = "OPENAI_API_KEY";
      const unsigned int maxRetries = 3;
      const bool debug = false;
    };
    explicit aistream(params p)
      : _maxRetries (p.maxRetries),
	_apiKey (p.apiKey),
	_keyName (p.keyName),
	_debug (p.debug)
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
	throw ai::exception(ai::exception_value::NO_KEY_DEFINED,
			   fmt::format("There was no key defined in the constructor or in the environment variable {}.", _keyName.c_str()));
      }
    }

    // Overload << operator for configuration
    aistream& operator<<(const ai::config& config) {
      switch (config) {
      case ai::config::GPT_3_5:
	_model = "gpt-3.5-turbo";
	// std::cout << "GPT 3.5" << std::endl;
	break;
      case ai::config::GPT_4_0:
	_model = "gpt-4.0";
	// std::cout << "GPT 4" << std::endl;
	break;
      };
      return *this;
    }
  
    // Overload << operator for validation
    aistream& operator<<(const validator& v) {
      _validator = v.function;
      return *this;
    }
  
    // Overload << operator to send queries
    aistream& operator<<(const json& js) {
      _messages.push_back(js);
      return *this;
    }

    // Overload >> operator to save stats
    aistream& operator>>(stats& stats) {
      stats = _stats;
      return *this;
    }
      
    // Overload >> operator to read responses
    aistream& operator>>(json& response_json) {
      response_json = {{}};
      auto retries = _maxRetries;
      json j;
      j["model"] = _model;
      j["messages"] = _messages;
      while (true) {
	if (retries == 0) {
	  throw ai::exception(ai::exception_value::TOO_MANY_RETRIES,
			     fmt::format("Maximum number of retries exceeded ({}).", _maxRetries));
	}
	try {
	  if (_debug) {
	    std::cerr << "Sending: " << j.dump() << std::endl;
	  }
	  auto chat = openai::chat().create(j);
	  _result = chat["choices"][0]["message"]["content"].get<std::string>();
	  if (_debug) {
	    std::cerr << "Received: " << _result << std::endl;
	  }
	  _stats.completion_tokens += chat["usage"]["completion_tokens"].get<unsigned int>();
	  _stats.prompt_tokens += chat["usage"]["prompt_tokens"].get<unsigned int>();
	  _stats.total_tokens += chat["usage"]["total_tokens"].get<unsigned int>();
	  response_json = json::parse(_result);
	  try {
	    bool valid = _validator(response_json);
	    if (valid) {
	      break;
	    }
	  } catch (ai::exception& e) {
	    if (_debug) {
	      std::cerr << fmt::format("Validator caught exception {}\n", e.what()) << std::endl;
	    }
#if 0
	    // Feedback loop.
	    _messages.push_back(json({
		  {"role", "user"},
		  {"content", e.what() }
		}));
	    j["messages"] = _messages;
#endif
	  }
	}
	catch (nlohmann::json_abi_v3_11_2::detail::parse_error& pe) {
	  // Retry if there were JSON parse errors.
	  if (_debug) {
	    std::cerr << "JSON parse error." << std::endl;
	  }
	}
	catch (nlohmann::json_abi_v3_11_2::detail::type_error& te) {
	  // Retry if there were JSON parse errors.
	  if (_debug) {
	    std::cerr << "JSON parse error." << std::endl;
	  }
	}
	catch (std::runtime_error& e) {
	  std::string msg(e.what());
	  // If we find "API key" in the message, we assume we had an invalid key.
	  if (msg.find("API key") != std::string::npos) {
	    std::cerr << "Missing API key?" << std::endl;
	    throw ai::exception(ai::exception_value::INVALID_KEY,
			       fmt::format("The API key ({}) was invalid.", _key.c_str()));
	  } else {
	    // Otherwise, pass up the exception.
	    throw e;
	  }
	}
	retries -= 1;
	if (_debug) {
	  std::cerr << "Retrying. Retries remaining: " << retries << std::endl;
	}
      }
      return *this;
    }

    void reset() {
      // Clears chat history and validator.
      _result = "";
      _messages.clear();
      _validator = [](const json&) { return true; };
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
    const bool _debug;
    stats _stats;
    std::function<bool(const json&)> _validator = [](const json&){ return true; };
  };

}

