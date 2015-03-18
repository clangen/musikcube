# Introduction #

mC 2 includes a big new feature. There is now a client/server model of the whole application that works exactly as a local library. This page will try to explain how the client/server works.

# Communication #

The client/server is communicating using what we would call a xmlsocket. A xmlsocket is a normal socket connection that send and receives all queries and queryresults using xml.
All binary data like mp3s, thumbnails etc, are send using a standard http server that is integrated in the musikServer. Currently the ports used by the servers are fixed (but that will change in the future) to 10543 for the xmlsocket and 10544 for the http server.

# Libraries and queries #

When we designed the library and queries that could be send to the library, we kept in mind that we sometime in the future would add the remote functionality. The library is designed in a way that when you add a query, it will add a copy of that query to a queryqueue, and then the library parser thread will execute the query when it has the time. What we did in the remote library was to instead of parsing the query, it will instead send the query through a xmlsocket and wait for reply on the same socket.
Looking at the [musik::core::Query::Base](http://code.google.com/p/musikcube/source/browse/trunk/src/core/Query/Base.h) you'll see that there are 5 important methods that needs to be implemented for each query to work both locally and remote:
  * ParseQuery : That does the actual parsing against the db.
  * SendQuery : Send the query to a musikServer.
  * ReceiveQuery : Used by the musikServer to receive the query.
  * SendResults : Send the results from the server to the cube.
  * ReceiveResults : Receives the results on the client.

# Security #

User authorization:
  * When a user is added on the musikServer, the password will be encrypted using a static salt and a one way md5 encryption before it's saved to the database. The exact same encryption is done on the client when adding a new remote library, so the passwords are never saved in clear text, and can never be recovered.
  * When a user connects to the server, the server will initialize by sending a authorization request with a random generated string. The client will use that string and encrypt the already encrypted password using the string as a salt. The resulting authorization code is then send back to the server where the same procedure to generate the authorization code will be done and compared to the one send from the client.
  * Using this procedure, the password (or even the encrypted password) will never be send over the connection.

Queries and query results:
  * The xmlsockets used to send queries and query results are not encrypted.
  * Since the authorization is made when the socket is initialized, we feel that there is no big reason to deal with the overhead of encrypting the whole query stream. You could compare this to logging in to a website using a login form on a https connection and then viewing the content through a normal http connection. It's actually easier to hack that kind of security than hacking the mC2 xmlsockets since the http protocol makes a new connection for each call.
  * Having unencrypted xmlsockets will allow us to make a flash client in the future.

Streaming:
  * The streaming of the tracks are not encrypted, but is authorized with the same authorizationcode generated when initializing the xmlsocket.
  * The streaming will not work unless the user has an open xmlsocket connection coming from the same host as the http connection.