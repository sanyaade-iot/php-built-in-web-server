/* request.h */
#ifndef REQUEST_H
#define REQUEST_H

struct server_request {
    struct http_client *client;
    struct http_response *resp;
};

#endif /* REQUEST_H */
