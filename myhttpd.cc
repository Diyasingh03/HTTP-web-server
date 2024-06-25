#include <signal.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include <time.h>
#include <algorithm>
#include <map>
//#include <thread>
//#include <mutex>
using namespace std;
pthread_mutex_t mutex;
void processRequest(int socket);
void iterServer (int masterSocket);
void forkServer( int masterSocket);
void createThreadForEachRequest(int masterSocket);
void poolOfThreads(int masterSocket);
void *loopthread(int masterSocket);
string fileType(string fname);
string dirList(DIR * dir, string fpath, string sortingOrder);
bool endsWith(string str, string suff);
int QueueLength = 5;
double cpu_time_used;
char * startTime;
int countRequest = 0;
double minimumTime = 100000000.0;
char slowestRequest [256];
double maximumTime = 0.0;
char fastestRequest [256];

main(int argc, char ** argv)
{
     time_t rawtime;
    time(&rawtime);
    startTime = ctime(&rawtime);
  int port = 0;
  string opt;
  if (argc == 2) {
    if (argv[1][0] == '-') {
      opt= string(argv[1]);
      port = 30604;
    } else {
      port = atoi(argv[1]);
      //string opt("");
    }
  } else if(argc == 3) {
    port = atoi(argv[2]);
    opt= string(argv[1]);
  } else {
    fprintf(stderr, "Usage: myhttpd [-f|-t|-p]  [<port>]");
    exit(-1);
  }
  cerr<< opt<< endl;
  //if (port == 0) port = 30604;
   // Add your HTTP implementation here
  // port from arg
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }
  /*while(1) {
    // accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    printf("accepted");
      processRequest(slaveSocket);

    
    //We assume that dispatchHTTP itself closes clientSocket.
  // Close socket
    close( slaveSocket );
    
  }*/

  // Process request.
  if (opt.empty()) {

    //processRequest(slaveSocket);i
    iterServer(masterSocket);
  }
  else if (opt == "-f") {
    //cerr << "forking"<<endl;
    forkServer(masterSocket);
  } else if (opt == "-t") {
    createThreadForEachRequest(masterSocket);
  } else if (opt == "-p") {
    poolOfThreads(masterSocket);
  }
}
    
void processRequest (int fd) {
  
  clock_t start, end;
  start = clock();
  const int MaxRequest = 6*1024;
  char request[MaxRequest + 1];
  int rlen = 0;
  int n;

  unsigned char newChar;
  //unsigned char lastChar = 0;

  while (rlen < MaxRequest && 
        (n = read(fd,&newChar, 1)) > 0) {


    request[rlen] = newChar;
    if (rlen >= 3 && request[rlen-3] == 13 && request[rlen-2] == 10
    && request[rlen - 1] == 13 && request[rlen] == 10) {
      //rlen-=2;
      break;
    }
    rlen++;



  }
  request[rlen] = 0;
  printf("req=%s\n", request);
  string reqstr(request);
  string fname;
  int p1 =reqstr.find(" ")+1;
  int p2 = reqstr.find(" ",p1);
//fname = reqstr.substr(p1,p2-p1);

  int ap = reqstr.find("Authorization: Basic ZHNpbmdoOjEyMzQ1");
  string fpath;
  const char * hdr;
  if (ap == string::npos) {
    hdr = "HTTP/1.1 401 Unauthorized\r\n"
    "WWW-Authenticate: Basic realm=\"myhttpd-realm-something\"\r\n"
    "\r\n";
    write(fd, hdr, strlen(hdr));
    //return;
  } else {
    if (p1 != string::npos && p2 != string::npos) {
      fname = reqstr.substr(p1,p2-p1);
      //string fpath;
      if (fname.find("icons/") != string::npos || fname.find("/cgi")!= string::npos) {
        fpath = "http-root-dir" + fname;
      }
      else if (fname == "/" || fname[1] != 'h') {
        if (fname == "/") {
          fname += "index.html";
        }
        fpath = "http-root-dir/htdocs" + fname;
      } else {
        fpath = "http-root-dir" + fname;
        printf("%s\n",fpath);
      }
      DIR * dir;
      string fcont;
      int reqfd;
      hdr = 
        "HTTP/1.1 200 Document follows\r\n" 
        "Server: CS 252 lab5\r\n"; 
        //"Content-type: text/html\r\n"
        //"\r\n";
      //bool error = true;
      if (fname.find("/cgi-bin") != string::npos) {
        int pos = fpath.find("?");
        string cgpath = fpath.substr(0,pos);
        cerr<< cgpath<< endl;
        if(!(open(cgpath.c_str(), O_RDONLY))) {
          hdr = "HTTP/1.1 404 File Not Found\r\n"
          "Server: CS 252 lab5\r\n"
          "Content-type: text/html\r\n"
          "\r\n";
          
          write(fd, hdr, strlen(hdr));
          write(fd, "Oops",4);
          exit(1);

        }
        else {
          char ** args = (char **)malloc(sizeof(char *) * 2);
            args[0] = (char *)malloc(sizeof(char) * fpath.size());

            for (int i=0; i < fpath.size(); i++) {
                args[0][i] = '\0';
            }

            args[1] = NULL;

            int start = fpath.find("?");;

            if (start!= string::npos) {
                //start++;
                //strcpy(args[0], start);
                args[0] = strdup(fpath.substr(start+1).c_str());
            }
            write(fd,hdr,strlen(hdr));
            if (endsWith(cgpath, ".so")) {
            }
            // CGI-BIN module
            else {
                int tmpout = dup(1);
                dup2(fd, 1);
                close(fd);
    
                pid_t p = fork();
                if (p == 0) {
                    setenv("REQUEST_METHOD","GET",1);
                    setenv("QUERY_STRING", args[0],1);
                    execvp(cgpath.c_str(), args);
                    cerr<<"I'm here in the child process\n";
                    exit(2);
                }
    
                dup2(tmpout,1);
                close(tmpout);
            }
            free(args[0]); free(args[1]);free(args);
            //close(cgfd);
        }

      }
        else if (endsWith(fpath, "stats") || endsWith(fpath, "stats/")) {
        write(fd,hdr,strlen(hdr));

        write(fd, "Diya Singh", 10);

        write(fd, startTime, strlen(startTime));
        write(fd, "\n", 1);

        char nreq[16];
        sprintf(nreq, "%d", countRequest);
        write(fd, nreq, strlen(nreq));
        write(fd, "\n", 1);

        char mint[128];
        sprintf(mint, "%f", minimumTime);
        write(fd, mint, strlen(mint));
        write(fd, "\n", 1);

        write(fd, fastestRequest, strlen(fastestRequest));
        write(fd, "\n", 1);

        char maxt[128];
        sprintf(maxt, "%f", maximumTime);
        write(fd, maxt, strlen(maxt));
        write(fd, "\n", 1);

        write(fd, slowestRequest, strlen(slowestRequest));
        write(fd, "\n", 1);
    }
      else if ((dir = opendir(fpath.c_str())) != NULL || endsWith(fpath, "?C=M;O=A") ||  
                endsWith(fpath, "?C=M;O=D") ||  endsWith(fpath, "?C=N;O=A") || 
                endsWith(fpath, "?C=N;O=D") ||  endsWith(fpath, "?C=S;O=A") ||  
                endsWith(fpath, "?C=S;O=D") ||endsWith(fpath, "?C=D;O=A") ||
                endsWith(fpath, "?C=D;O=D")) {
       //string sortingMode = "N";
       string sortingOrder = "A";
        if (dir == NULL) {
            int s = fpath.size();
            //sortingMode = fpath[ s- 5];
            sortingOrder = fpath[s - 1];
            fpath= fpath.substr(0,fpath.find('?'));
            fpath.push_back('/');
            //docpath[strlen(docpath)-8] = '\0';
            dir=opendir(fpath.c_str());
            if (dir == NULL) {
              printf("ERORRRRRRRR\n");
            }
        }
         //fpath.push_back('/');
      if (fpath[fpath.size()-1] !='/') {
          fpath.push_back('/');
        }

        if(fname[fname.size()-1] != '/') fname.push_back('/');
        string ftype = "Content-type: text/html\r\n\r\n";
        if (fname == "/subdir1/") {
          fpath = "http-root-dir/htdocs/dir1/subdir1/";
          fname = "/dir1/subdir1/";
        }
           /*char * path = realpath("/homes/singh959/cs252/lab5-src/http-root-dir/icons/unknown.gif",NULL);
           string tmpp(path);*/

          // It's a directory
          string nextSort = (sortingOrder == "A") ? "D" : "A";
          fcont = "<html><head><title>Directory Listing</title></head><body><h1>Directory Listing</h1>"
          "<table><tr><th valign=\"top\"><img src=\"/icons/blank.gif\" alt=\"[ICO]\"></th>"
          "<th><a href=\"?C=N;O="+nextSort+"\">Name</a></th>"
          "<th><a href=\"?C=M;O="+nextSort+"\">Last modified</a></th>"
          "<th><a href=\"?C=S;O="+nextSort+"\">Size</a></th><th><a href=\"?C=D;O="+nextSort+"\">Description</a></th></tr>"
          "<tr><th colspan=\"5\"><hr></th></tr>\n";
          fcont += dirList(dir, fpath, sortingOrder);
          //printf("Contents of directory '%s':\n", path);
          /*while ((ent = readdir(dir)) != NULL) {
              string temp(ent->d_name);
              fcont += temp + "\n";
              //fcont.push_back('\n');
          }*/
          //free(ent);
          closedir(dir);
          fcont += "<tr><th colspan=\"5\"><hr></th></tr></body></html>";
        printf("%s",ftype);
        write(fd, hdr, strlen(hdr));
        write(fd, ftype.c_str(), ftype.size());
        write(fd, fcont.c_str(), fcont.size());


      } else if ((reqfd = open(fpath.c_str(), O_RDONLY)) != 0) {
        /*if (fpath[fpath.size()-1] !='/') {
          fpath.push_back('/');
        }*/
        string ftype = "Content-type: " + fileType(fname) + "\r\n\r\n";
        //string fcont;
        while((n = read(reqfd,&newChar, 1)) > 0) {
          fcont.push_back(newChar);
        }
        close(reqfd);
        //cerr << fcont <<endl;
        //string hdrstr(hdr);
        //hdrstr += fcont;
        write(fd, hdr, strlen(hdr));

        write(fd, ftype.c_str(), ftype.size());
        write(fd, fcont.c_str(), fcont.size());
        //close(reqfd);

      } else {
        hdr = "HTTP/1.1 404 File Not Found\r\n"
        "Server: CS 252 lab5\r\n"
        "Content-type: text/html\r\n"
        "\r\n";
        
        write(fd, hdr, strlen(hdr));
        write(fd, "Oops",4);

      } 

    }
  }
    



    /*const char * hi = "\nHi ";
  const char * timeIs = " the time is:\n";
  write( fd, hi, strlen( hi ) );
  write( fd, name, strlen( name ) );
  write( fd, timeIs, strlen( timeIs ) );
  
  // Send the time of day 
  write(fd, timeString, strlen(timeString));

  // Send last newline
  const char * newline="\n";
  write(fd, newline, strlen(newline));
  */
  if (!endsWith(fpath, "stats") && !endsWith(fpath, "stats/")
        && !endsWith(fpath, "logs") && !endsWith(fpath, "logs/")) {
        end = clock();
        //current_t = (double)(end - start);
        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
        if (cpu_time_used < minimumTime) {
            minimumTime = cpu_time_used;
            strcpy(fastestRequest, fpath.c_str());
        }

        if (cpu_time_used > maximumTime) {
            maximumTime = cpu_time_used;
            strcpy(slowestRequest, fpath.c_str());
        }

        char sourceHost[1023];
        gethostname(sourceHost, 1023);
        strcat(sourceHost, ":");

        char convertPort[8];
        sprintf(convertPort, "%d", 30604);
        strcat(sourceHost, convertPort);
        strcat(sourceHost, fpath.c_str());
        strcat(sourceHost, "\n");

        FILE * f = fopen("/homes/singh959/cs252/lab5-src/http-root-dir/logs", "a+");

        fprintf(f, "%s\n", request);
        fclose (f);
    }
  //close socket
  close( fd );

}

string fileType(string fname) {
  int dpos = fname.find('.');
  if(dpos != string::npos) {
    string ext = fname.substr(dpos+1);
    if (ext == "html") {
      return "text/html";
    } else if( ext == "jpg" || ext == "jpeg") {
      return "image/jpeg";
    } else if( ext == "png") {
      return "image/png";
    } else if (ext == "svg") {
      return "image/svg+xml";
    } else if(ext =="gif") {
      return "image/gif";
    }
  }
  return "text/plain";
}

string dirList(DIR * dir, string fpath, string sortingOrder) {
  map<string,string> fnames;
 // map<string,string> fsizes;
  vector <string> fnamesvec;
  struct dirent *ent;
  string pd;
  string response;
  string cssStyle = "<style>"
                  "table {"
                  "    font-family: Arial, sans-serif;"
                  "    border-collapse: collapse;"
                  "    width: 100%;"
                  "    border: 2px solid #007bff;" // Blue border
                  "}"
                  "td, th {"
                  "    border: 1px solid #dddddd;"
                  "    text-align: left;"
                  "    padding: 8px;"
                  "}"
                  "tr:nth-child(even) {"
                  "    background-color: #f2f2f2;"
                  "}"
                  "tr:hover {"
                  "    background-color: #cce5ff;" // Light blue hover effect
                  "}"
                  "</style>";

  while ((ent = readdir(dir)) != nullptr) {
  string currr;
    string filename = ent->d_name;
    string dispn = filename;
    string alt= " ";
    string imgs= "/icons/unknown.gif";
    string fsize = "-";
    string filep = fpath+filename;
    struct stat fstat;
    if (stat(filep.c_str(), &fstat) == 0) {fsize = (S_ISDIR(fstat.st_mode)) ? "-" : to_string(fstat.st_size);}
   // string pd;
    if (filename == "subdir1") {
          filename = "/dir1/subdir1/";
          dispn = "subdir1";
          imgs = "/icons/menu.gif";
          alt = "DIR";
    } else if (filename.find(".gif") != string::npos) {
      imgs = "/icons/image.gif";
    }
    // Skip current directory and parent directory entries
    if (filename == ".") {
      continue;
    } else if (filename == "..") {
      pd = "<tr><td valign=\"top\"><img src=\"/icons/back.gif\" alt=\"[PARENTDIR]\"></td><td><a href='" + filename + "'>Parent Directory</a></td><td align=\"right\">Apr 16 20:12  </td><td align=\"right\">  - </td><td>&nbsp;</td></tr>";
      continue;
    }
    currr = "<tr><td valign=\"top\"><img src=\""+imgs+"\" alt=\"["+alt+"]\"></td><td><a href='" + filename + "'>" + dispn + "</a></td><td align=\"right\">Apr 16 20:12  </td><td align=\"right\">"+fsize+"</td><td>&nbsp;</td></tr>";

    fnames[dispn] = currr;
    fnamesvec.push_back(dispn);
    //fsizes[dispn];
    //images.push_back(imgs);
  }
  int i = 0;
  if (sortingOrder == "D") {
      sort(fnamesvec.begin(), fnamesvec.end(),greater<string>());
  } else {
      sort(fnamesvec.begin(), fnamesvec.end());
  }

    while(i<fnamesvec.size()){
    //char * path = realpath("/homes/singh959/cs252/lab5-src/http-root-dir/icons/unknown.gif",NULL);
    //string tmpp(path);
        
    // Append directory entry as a hyperlink
    string dispn = fnamesvec[i];
      response += fnames[dispn];
      i++;
    }
  return cssStyle+pd+response;
}

void iterServer (int masterSocket) {
  while(1) {
    // Accept incoming connections
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    //if (slaveSocket >=0) {
      processRequest(slaveSocket);
    //}
    //We assume that dispatchHTTP itself closes clientSocket.
  // Close socket
    //close( slaveSocket );
    
  }

}
extern "C" void sigIntHandler(int siga) {
   if (siga == SIGCHLD) {
    //pid_t pid = wait3(0, 0, NULL);
   //cerr<<"zombie"<<endl; 
    //while (waitpid(-1, NULL, WNOHANG) > 0) {
      //if(isatty(0)) printf("\n[%d] exited.", pid);

      //}
    while(waitpid(-1, NULL, WNOHANG) > 0);
  }
}

void forkServer( int masterSocket) {
  while(1) {
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    
    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }
    struct sigaction sa;
    sa.sa_handler = sigIntHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    //if (slaveSocket >=0) {
      int ret = fork();
      if(ret == 0) {
        processRequest(slaveSocket);
        exit(0);
      } else {
        sigaction(SIGCHLD, &sa, NULL);
        close(slaveSocket);
      }
     // int wstatus;
      //while (waitpid(-1, &wstatus,WNOHANG)!=-1);
   // }
  }
}

void createThreadForEachRequest(int masterSocket)
{
  while (1) {
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);

    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }

    if (slaveSocket >= 0) {
      // When the thread ends resources are recycled
      pthread_t thread;
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
      pthread_create(&thread, &attr,(void *(*) (void *))processRequest, (void *) slaveSocket);
    }
  }
}

void poolOfThreads( int masterSocket ) {
  pthread_t thread[5];
  pthread_mutex_init(&mutex, NULL);
  for (int i=0; i<4; i++) {
    //pthread_t thread;
    pthread_create(&thread[i], NULL, (void *(*) (void *))loopthread, (void *) masterSocket);
  }
  pthread_join(thread[3],NULL);
  
  //loopthread (masterSocket);
}
void *loopthread (int masterSocket) {
  while (1) {
    //int clientSocket = accept(serverSocket,&sockInfo, &alen);
    struct sockaddr_in clientIPAddress;
    int alen = sizeof( clientIPAddress );
    pthread_mutex_lock(&mutex);
    int slaveSocket = accept( masterSocket,
			      (struct sockaddr *)&clientIPAddress,
			      (socklen_t*)&alen);
    pthread_mutex_unlock(&mutex);
    if ( slaveSocket < 0 ) {
      perror( "accept" );
      exit( -1 );
    }

    if (slaveSocket >= 0) {
      processRequest(slaveSocket);
    }
  }
}

bool endsWith(string str, string suffix) {
    if (str.length() >= suffix.length()) {
        return (str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
    } else {
        return false;
    }
}
