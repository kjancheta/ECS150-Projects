#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

// readme summary if ur reading this hello :D
// make web server multi threaded and modify how web server is invoked to
  // handle new input parameters like the number of threads to create
// to run:
  // compile using "make", "make clean" to remove .o files and clean build
// simplest way to make multi threaded server:
  // spawn a new thread for every new http request, OS schedules threads accordingly.
  // short requests dont need to wait for long request to finish, and when one
  // thread is blocked the others can handle other requests.
  // but web server pays the overhead
// preferred approach:
  // create a fixed size pool of worker threads when the web server is first started.
  // each thread is blocked until there is an http request for it,
  // so if more worker threads than active requests, some threads blocked and waiting,
  // if more requests than worker threads, requests need to be buffered until ready thread

// IMPLEMENT:
// main thread that starts by making pool of worker threads, # specified on command line
// main thread accepts new http connections and placing descriptor into fixed size buffer
// main thread doesnt read from this connection, # elements in buffer specified in cl
// existing web server has a single thread that accepts and handles the connection
// in this web server, this thread places a connection descriptor into a fixed size buffer,
  // then returns to accept more connections
// each worker thread able to handle requests
// worker thread wakes when http request in queue
// multiple http requests available, which is handled depends on scheduling policy
// when worker thread wakes, does read on network descriptor, obtains specified
  // content, (by reading static file or executing cgi process) then returns content
  // to client by writing to descriptor
// worker thread then waits for another http request
// main thread and worker threads in PRODUCER-CONSUMER relationship
  // requires that their accesses to the shared buffer are synchronized
// main thread must block and wait if buffer is full
// worker thread must wait if buffer is empty
// use condition variables, NO busy waiting

// scheduling policy: first in first out (fifo)
// when web server has multiple worker threads running, u have no control over 
  // which thread is actually scheduled at any given time by the os
// determine which http request should be handled by each of the waiting workers
// fifo: when worker thread wakes, handles first request (oldest) in the buffer
// http requests will not necessarily finish in fifo order
// order in which requests complete depends on how the os schedules active threads

// security
// make sure server is not running beyond testing
// constrain file constraints to stay within subtree of file system hierarchy,
  // rooted at the base working directory that the server starts in
// ensure pathnames that are passed in do not refer to files outside of this subtree
// can do this by rejecting pathnames with '..' in it, avoiding traversals up

// command line arguments should be invoked with:
//```bash
//$ ./gunrock_web [-p port] [-t threads] [-b buffers]
//```


// globals
int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

pthread_cond_t canTake = PTHREAD_COND_INITIALIZER; // conditional variable, can take client
pthread_cond_t canFill = PTHREAD_COND_INITIALIZER; // conditional variable, can add client
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // lock

vector<HttpService *> services;
deque<MySocket *> clients; // deque for clients

HttpService *find_service(HTTPRequest *request) {
   // find a service that is registered for this path prefix
  for (unsigned int idx = 0; idx < services.size(); idx++) {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}


void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
  stringstream payload;

  // invoke the service if we found one
  if (service == NULL) {
    // not found status
    response->setStatus(404);
  } else if (request->isHead()) {
    service->head(request, response);
  } else if (request->isGet()) {
    service->get(request, response);
  } else {
    // The server doesn't know about this method
    response->setStatus(501);
  }
}

void handle_request(MySocket *client) {
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;
  
  // read in the request
  bool readResult = false;
  try {
    payload << "client: " << (void *) client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  } catch (...) {
    // swallow it
  }    
    
  if (!readResult) {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }
  
  HttpService *service = find_service(request);
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str(""); payload.clear();
  payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());
    
  delete response;
  delete request;

  payload.str(""); payload.clear();
  payload << " client: " << (void *) client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
}

void *consumer(void *arg) { // consumer function
  while (1) {
    dthread_mutex_lock(&lock);
    while (clients.empty()) { // check if there are no clients
      dthread_cond_wait(&canTake, &lock); // wait until we can take one
    }
    MySocket* client = clients.front(); // copy client at front of deque
    clients.pop_front(); // remove front from deque
    dthread_cond_signal(&canFill); // signal that we can fill to the producer
    dthread_mutex_unlock(&lock);
    handle_request(client);
  }
  return NULL;
}

int main(int argc, char *argv[]) {

  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
    switch (option) {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr<< "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client;

  vector<pthread_t> threadIDs(THREAD_POOL_SIZE); // store thread ids

  for (int i = 0; i < THREAD_POOL_SIZE; i++) { // create as many as pool size
    dthread_create(&(threadIDs[i]), NULL, consumer, NULL); // creates thread
    dthread_detach(threadIDs[i]); // detach thread, run independently
  }
  // The order that you push services dictates the search order
  // for path prefix matching
  services.push_back(new FileService(BASEDIR));
  
  while(true) { 
    sync_print("waiting_to_accept", "");
    client = server->accept();
    sync_print("client_accepted", "");
    //handle_request(client);
    dthread_mutex_lock(&lock); 
    while (clients.size() >= static_cast<size_t>(BUFFER_SIZE)) { // check if buffer is full
      dthread_cond_wait(&canFill, &lock); // wait until can fill it
    }
    clients.push_back(client); // add client to deque
    dthread_cond_signal(&canTake); // wake a worker, signal that they can take 
    dthread_mutex_unlock(&lock);
  }

}
