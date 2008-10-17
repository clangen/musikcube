#include "stdafx.h"

#include "HTTPRequest.h"
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/algorithm/string.hpp>


HTTPRequest::HTTPRequest(void)
 :socket(IOService)
{
}

HTTPRequest::~HTTPRequest(void)
{
    this->socket.close();
}

bool HTTPRequest::Request(const char *url){
    using namespace boost::asio::ip;

    // Start by splitting the url
    std::string uri(url);
    std::string location;
    std::string server;
    std::string port("80");

    std::string::size_type protocolSplitter = uri.find("://");
    if(protocolSplitter!=std::string::npos){
        // Remove from uri
        uri = uri.substr(protocolSplitter+3);
    }

    // Find location
    std::string::size_type locationSplitter = uri.find("/");
    if(locationSplitter!=std::string::npos){
        // Remove from uri
        location    = uri.substr(locationSplitter);
        uri = uri.substr(0,locationSplitter);
    }

    // Find port
    std::string::size_type portSplitter = uri.find(":");
    if(portSplitter!=std::string::npos){
        port    = uri.substr(portSplitter+1);
        uri = uri.substr(0,portSplitter);
    }

    // Set server to what is left
    server  = uri;    
    

    tcp::resolver resolver(this->IOService);
    tcp::resolver::query query(server, port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;

    // Try each endpoint until we successfully establish a connection.
    boost::system::error_code error = boost::asio::error::host_not_found;
    while(error && endpoint_iterator!= end){
        this->socket.close();
        this->socket.connect(*endpoint_iterator++, error);
    }

    if(error){
        // Unable to connect to server
        return false;
    }

    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    boost::asio::streambuf request;
    std::ostream request_stream(&request);
    request_stream << "GET " << location << " HTTP/1.0\r\n";
    request_stream << "Host: " << server << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::asio::write(this->socket, request);

    // Read the response status line.
    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/"){
        // Invalid response
        return false;
    }
    if (status_code != 200){
        // std::cout << "Response returned with status code " << status_code << "\n";
        return false;
    }

    // Lets get the rest of the headers send
    this->attributes.clear();

    do{
        // Read a line
        std::string line;
        std::getline(response_stream, line);

        std::size_t pos = line.find(":");
        if(pos!=std::string::npos){
            std::string key = line.substr(0,pos);
            boost::algorithm::trim(key);
            std::string value   = line.substr(pos+1);
            boost::algorithm::trim(value);
            this->attributes[key]    = value;
        }
    }while(response_stream);


    return true;
}

long HTTPRequest::GetContent(void *buffer,long getLength){
    // First just try to read_some so that we do not hold if not nessesary
    boost::system::error_code error;
    std::size_t bytesRead   = this->socket.read_some(boost::asio::buffer(buffer,getLength),error);

    if(error){
        return 0;
    }
    return bytesRead;
}

