#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
#include <thread>
#include <signal.h>
#include <time.h>
#include <sys/epoll.h>
using namespace std;

bool filetransfer = false;
__int64_t filelength;
__int64_t remlen;
HistogramCollection hc;

FIFORequestChannel* create_new_channel (FIFORequestChannel* mainchan) 
{
    char name [1024];
    MESSAGE_TYPE m = NEWCHANNEL_MSG;
    mainchan->cwrite (&m, sizeof(m));
    mainchan->cread (name, 1024);
    FIFORequestChannel* newchan = new FIFORequestChannel (name, FIFORequestChannel::CLIENT_SIDE);
    return newchan;
}

void patient_thread_function(int n, int pno, BoundedBuffer* request_buffer) 
{
    /* What will the patient threads do? */
    datamsg d(pno, 0.0, 1);
    double resp = 0;

    for ( int i = 0; i < n; ++i ) 
    {
        /*chan->cwrite (&d, sizeof (datamsg));
        chan->cread (&resp, sizeof(double));
        hc->update (pno, resp);*/
        request_buffer->push ((char*) &d,sizeof(datamsg));
        d.seconds += 0.004;
    }
}

void file_thread_function (string fname, BoundedBuffer* request_buffer, FIFORequestChannel* chan, int mb) 
{
    //1. Create the file
    string recvfname = "recv/" + fname;
    //make it as long as the original
    char buf [1024];
    filemsg f(0, 0);
    memcpy(buf, &f, sizeof(f));
    strcpy (buf + sizeof(f), fname.c_str());
    chan->cwrite(buf, sizeof(f) + fname.size() + 1);
    __int64_t filelength;
    chan->cread(&filelength, sizeof(filelength));
    FILE* fp = fopen(recvfname.c_str(), "w");
    fseek (fp, filelength, SEEK_SET);
    fclose(fp);

    //2. Generate all the file messages
    filemsg* fm = (filemsg*) buf;
    //__int64_t remlen = filelength;
    remlen = filelength;

    while (remlen > 0) 
    {
        fm->length = min(remlen, (__int64_t) mb);
        request_buffer->push(buf, sizeof(filemsg) + fname.size() + 1);
        fm->offset += fm->length;
        remlen -= fm->length;
    }
}

void worker_thread_function(FIFORequestChannel* chan, BoundedBuffer* request_buffer, HistogramCollection* hc, int mb)
{
    /*
		Functionality of the worker threads
    */
    char buf[1024];
    double resp = 0;
    char recvbuf[mb];

    while (true) 
    {
        request_buffer->pop(buf, 1024);
        MESSAGE_TYPE* m = (MESSAGE_TYPE*) buf;

        if (*m == DATA_MSG) 
        {
            chan->cwrite(buf, sizeof(datamsg));
            chan->cread (&resp, sizeof(double));
            hc->update(((datamsg*) buf)->person, resp);
        }
        else if (*m == QUIT_MSG) 
        {
            chan->cwrite(m, sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }
        else if (*m == FILE_MSG) 
        {
            filemsg* fm = (filemsg*) buf;
            string fname = (char*)(fm + 1);
            int sz = sizeof(filemsg) + fname.size() + 1;
            chan->cwrite(buf, sz);
            chan->cread(recvbuf, mb);

            string recvfname = "recv/" + fname;
            FILE* fp = fopen(recvfname.c_str(), "r+");
            fseek (fp, fm->offset, SEEK_SET);
            fwrite (recvbuf, 1, fm->length, fp);
            fclose(fp);
        }
    }
}

void event_polling_thread(int w, int mb, FIFORequestChannel** wchans, BoundedBuffer* request_buffer, HistogramCollection* hc)
{
    char buf[1024];
    double resp = 0;
    char recvbuf[mb];

    struct epoll_event ev;
    struct epoll_event events[w];
    
    //create an empty epoll list
    int epollfd = epoll_create1 (0);
    if(epollfd == -1)
    {
        EXITONERROR("epoll_create1");
    }

    unordered_map<int, int> fd_to_index;
    vector<vector<char>> state(w);

    //priming + adding each fd to the list
    int nsent = 0, nrecv = 0;
    bool quit_recv = false;
    for(int i = 0; i < w; i++)
    {
        int sz = request_buffer->pop(buf, 1024);
        if(*(MESSAGE_TYPE*) buf == QUIT_MSG)
        {
            quit_recv = true;
            break;
        }
        wchans[i]->cwrite(buf, sz);

        state[i] = vector<char>(buf, buf + sz); // record the state [i]
        nsent++;

        int rfd = wchans[i]->getrfd();
        fcntl(rfd, F_SETFL, O_NONBLOCK);
        //cout << rfd << endl;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = rfd;
        fd_to_index[rfd] = i;

        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, rfd, &ev) == -1)
        {
            EXITONERROR("epoll_ctl: listen_sock");
        }
    }

    while(true)
    {
        if(quit_recv && (nsent == nrecv))
        {
            break;
        }

        int nfds = epoll_wait(epollfd, events, w, -1);
        if(nfds == -1)
        {
            EXITONERROR("epoll_wait");
        }
        for(int i = 0; i < nfds; i++)
        {
            int rfd = events[i].data.fd;
            int index = fd_to_index[rfd];
            int resp_sz = wchans[index]->cread(recvbuf, mb);
            nrecv++;

            //process (recvbuf)
            vector<char> req = state[index];
            char* request = req.data();
            //processing the response
            MESSAGE_TYPE* m = (MESSAGE_TYPE *) buf;
            if(*m == DATA_MSG)
            {
                //cout << "recvd: " << *(double*)recvbuf << endl;
                hc->update(((datamsg *)request)->person, *(double*) recvbuf);
            }
            else if(*m == FILE_MSG)
            {
                filemsg* fm = (filemsg*) m;
                string fname = (char*)(fm + 1);

                string recvfname = "recv/" + fname;
                FILE* fp = fopen(recvfname.c_str(), "r+");
                fseek (fp, fm->offset, SEEK_SET);
                fwrite (recvbuf, 1, fm->length, fp);
                fclose(fp);
            }
            
            //reuse 
            if(!quit_recv)
            {
                int req_sz = request_buffer->pop(buf, sizeof(buf));
                if(*(MESSAGE_TYPE*) buf == QUIT_MSG)
                {
                    quit_recv = true;
                }
                else
                {
                    wchans[index]->cwrite(buf, req_sz);
                    state[index] = vector<char> (buf, buf+req_sz); 
                    nsent++;
                }
            }
        }
    }
}


int main(int argc, char *argv[]) 
{
    int n = 1;    //default number of requests per "patient"
    int p = 1;     // number of patients [1,15]
    int w = 5;    //default number of worker threads
    int b = 100;    // default capacity of the request buffer, you should change this default
    int m = MAX_MESSAGE;    // default capacity of the message buffer
    char* _m = nullptr;
    srand(time_t(NULL));
    string fname = "10.csv"; // default file
    bool filetransfer = false;

    int opt = -1;
    while ((opt = getopt(argc, argv, "m:n:b:w:p:f:")) != -1) 
    {
        switch (opt) {
            case 'm':
                m = atoi(optarg);
                _m = optarg;
                break;
            case 'n':
                n = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                break;
            case 'f':
                filetransfer = true;
                fname = optarg;
            default:
                break;
        }
    }

    if(w > n*p)
    {
        w = n*p;
    }

    int pid = fork();
    if (pid == 0) 
    {
        if (_m) 
        {
            execl("server", "server", "-m", _m, (char*) NULL);
        }
        else 
        {
            execl("server", "server", (char *) NULL);
        }
    }

    FIFORequestChannel *chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    HistogramCollection hc;

    //making histograms and adding to the histogram collection hc
    if(!filetransfer)
    {
        for (int i = 0; i < p; ++i) 
        {
            Histogram *h = new Histogram(10, -2.0, 2.0);
            hc.add(h);
            w = min(n * p, w);
        }
    }

    // make worker channels
    FIFORequestChannel** wchans = new FIFORequestChannel*[w];
    for (int i = 0; i < w; ++i) 
    {
        wchans[i] = create_new_channel(chan);
    }

    struct timeval start, end;
    gettimeofday(&start, 0);

    /* Start all threads here */
    thread patient[p];
    thread* filethread;
    if (!filetransfer) 
    {
        for (int i = 0; i < p; ++i) 
        {
            patient[i] = thread(patient_thread_function, n, i + 1, &request_buffer);
        }
    }
    else
    {
        filethread = new thread(file_thread_function, fname, &request_buffer, chan, m);
    }

    thread evp(event_polling_thread, w, m, wchans, &request_buffer, &hc);

    if(!filetransfer)
    {
        for(int i = 0; i < p; i++)
        {
            patient[i].join();
        }
    }
    else
    {
        filethread->join();
    }
    
    MESSAGE_TYPE q = QUIT_MSG;
    request_buffer.push((char *) &q, sizeof(q));
    evp.join();

    for(int i = 0; i < w; ++i)
    {
        wchans[i]->cwrite(&q, sizeof(MESSAGE_TYPE));
        delete wchans[i];
    }

    delete[] wchans;

    gettimeofday(&end, 0);

    // print the results
    hc.print();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int) 1e6;
    int usecs = (int) (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    /*for (int i = 0; i < p; ++i) 
    {
        MESSAGE_TYPE q = QUIT_MSG;
        wchans[i]->cwrite((char *) &q, sizeof(MESSAGE_TYPE));
        delete wchans[i];
    }*/

    //MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan; 
    exit(EXIT_SUCCESS);
}
