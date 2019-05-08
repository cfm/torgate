#include <iostream>
#include <curl/curl.h>
using namespace std;

#include "TorHttpRequest.hpp"
#include "Util.hpp"

/* rewrites the hostname for the request to route through Tor/Onion */
string TorHttpRequest::getTargetHostName(string sourceHost, string tld){
    string result = sourceHost;

    /* check if the domain ends with the tld */
    if(Util::stringEndsWith(sourceHost,tld)){
        result = sourceHost.substr(0,sourceHost.length()-tld.length());
    }

    return result;
}

/* writes the response headers */
size_t TorHttpRequest::writeResponseHeader(char* b, size_t size, size_t nitems, void* userdata){
    string headerString = b;

    /* vector with all valid headers to return */
    vector<string> vlist = {
        "Content-Type", "Content-Language"
    };
    
    if(headerString.size() > 0){
        if(headerString.find(":") != string::npos){
            vector<TorHttpResponseHeader>* headerList = (vector<TorHttpResponseHeader>*)userdata;
            TorHttpResponseHeader header;

            /* parse header name and value */
            header.name = headerString.substr(0, headerString.find(":"));
            header.value = headerString.substr(headerString.find(":")+1);

            /* trim the header name and value */
            Util::trim(header.name);
            Util::trim(header.value);

            /* check if header is allowed for response */
            if(std::find(vlist.begin(), vlist.end(), header.name) != vlist.end()) {
                headerList->push_back(header);
            }
        }

        /* check if new set of headers is coming up after a redirect */
        if(headerString.substr(0,5) == "HTTP/"){
            /* clear out all headers in the vector */
            vector<TorHttpResponseHeader>* headerList = (vector<TorHttpResponseHeader>*)userdata;
            headerList->clear();
        }
    }

    return nitems * size;
}

/* executes a GET requests against the defined Host and URL */
TorHttpResponse TorHttpRequest::get(){
    TorHttpResponse result;

    /* initialise curl handle */
    CURL *curl;

    /* define the full request url */
    string url = "http://" + this->requestHost + "/" + this->requestUrl;

    /* buffer to write result into */
    string readBuffer;
    vector<TorHttpResponseHeader> headerList;

    /* initialise curl */
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); /* 1L = verbose, 0L = off */
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, TorHttpRequest::writeResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true); 
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, TorHttpRequest::writeResponseHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerList);

    /* check if a proxy is defined */
    if(this->proxyHost.length() > 0 && this->proxyPort > 0){
        /* define the socks5 proxy with hostname resolve and pass to curl */
        string proxyUrl = "socks5h://" + this->proxyHost + ":" + std::to_string(this->proxyPort);
        curl_easy_setopt(curl, CURLOPT_PROXY, proxyUrl.c_str());
    }

    /* kill and clean up curl */
    result.status = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    /* build up the result struct */
    result.headerList = headerList;
    result.content = readBuffer;

    return result;
}