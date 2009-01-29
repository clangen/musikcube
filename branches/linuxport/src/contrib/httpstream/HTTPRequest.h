#pragma once
#include <boost/asio.hpp>
#include <map>
#include <vector>


class HTTPRequest
{
public:
    HTTPRequest(void);
    ~HTTPRequest(void);

    bool Request(const char *url);
    long GetContent(void *buffer,long getLength);

    typedef std::vector<std::string> StringVector;
    typedef std::map<std::string,std::string> AttributeMap;
    AttributeMap attributes;
private:
    boost::asio::io_service IOService;
    boost::asio::ip::tcp::socket socket;

    long pcBufferMaxSize;
    long pcBufferSize;
    long pcBufferPosition;
    char *pcBuffer;

};
