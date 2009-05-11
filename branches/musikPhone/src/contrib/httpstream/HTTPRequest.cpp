#include "stdafx.h"

#include "HTTPRequest.h"
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <boost/algorithm/string.hpp>


HTTPRequest::HTTPRequest(void)
 :socket(IOService)
 ,pcBuffer(NULL)
 ,pcBufferMaxSize(256*1024)
 ,pcBufferSize(0)
 ,pcBufferPosition(0)
{
    this->pcBuffer  = new char[256*1024];
}

HTTPRequest::~HTTPRequest(void)
{
    delete this->pcBuffer;
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
    std::size_t bytesRead(0);
    // lets check if ther is more that we can precache
    if(this->pcBufferSize==0){
        // if there is no available buffer, lets read directly to the buffer once there is something to read
        boost::system::error_code error;        
        bytesRead   = this->socket.read_some(boost::asio::buffer(buffer,getLength),error);
        if(error){
            return 0;
        }
    }

    // lets try to precache whatever is left 
    size_t availableBuffer  = this->socket.available();
    bool readError(false);
    while(availableBuffer && this->pcBufferSize<this->pcBufferMaxSize && !readError){
        // read to the internal precache buffer
        std::size_t pcEndPosition       = (this->pcBufferPosition+this->pcBufferSize)%this->pcBufferMaxSize;

        std::size_t rotationSizeLeft    = this->pcBufferMaxSize-pcEndPosition;
        std::size_t pcBufferLeft        = this->pcBufferMaxSize-this->pcBufferSize;
        std::size_t readMax             = pcBufferLeft<availableBuffer?pcBufferLeft:availableBuffer;
        readMax                         = rotationSizeLeft<readMax?rotationSizeLeft:readMax;

        boost::system::error_code error;        
        std::size_t addedToPrecache     = this->socket.read_some(boost::asio::buffer(&this->pcBuffer[pcEndPosition],readMax),error);
        if(error){
            readError   = true;
        }
        this->pcBufferSize              += addedToPrecache;
    
        availableBuffer  = this->socket.available();
    }

    // if we havn't read anything yet and we have some cached buffer, lets copy from the precache buffer
    if(!bytesRead && this->pcBufferSize){
        while(bytesRead<getLength && this->pcBufferSize){
            std::size_t rotationSizeLeft    = this->pcBufferMaxSize-this->pcBufferPosition;
            std::size_t requestedLeft       = getLength-bytesRead;
            std::size_t readMax             = rotationSizeLeft<this->pcBufferSize?rotationSizeLeft:this->pcBufferSize;
            readMax                         = requestedLeft<readMax?requestedLeft:readMax;
            memcpy((char*)buffer+bytesRead,this->pcBuffer+this->pcBufferPosition,readMax);

            bytesRead           += readMax;
            this->pcBufferSize  -= readMax;
            this->pcBufferPosition  = (this->pcBufferPosition+readMax)%this->pcBufferMaxSize;

        }
    }

    return bytesRead;

}

