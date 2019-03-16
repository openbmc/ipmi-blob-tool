#pragma once

#include <exception>
#include <map>
#include <sstream>
#include <string>

namespace ipmiblob
{

class IpmiException : public std::exception
{
  public:
    explicit IpmiException(const std::string& message) : _message(message){};

    static std::string messageFromIpmi(int cc)
    {
        const std::map<int, std::string> commonFailures = {
            {0xc0, "busy"},
            {0xc1, "invalid"},
            {0xc3, "timeout"},
        };

        std::ostringstream smessage;

        auto search = commonFailures.find(cc);
        if (search != commonFailures.end())
        {
            smessage << "Received IPMI_CC: " << search->second;
        }
        else
        {
            smessage << "Received IPMI_CC: " << cc;
        }

        return smessage.str();
    }

    explicit IpmiException(int cc) : IpmiException(messageFromIpmi(cc))
    {
    }

    virtual const char* what() const noexcept override
    {
        return _message.c_str();
    }

  private:
    std::string _message;
};

} // namespace ipmiblob
