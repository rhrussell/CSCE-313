/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/20
 */
#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>


using namespace std;


int main(int argc, char *argv[]){

    //TASK FOUR: RUN THE SERVER AS A CHILD PROCESS
    /*int pid = fork();
    if(pid == 0)
    {
        char* arg[] = {"p", "t", "e", "m", "f", "c"};
        execvp("./server", arg);
    }*/

    //FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);

    //TASK ONE: REQUESTING DATA POINTS

    int opt;
    int person = 0;
    double time = -1.0;
    int ecg = 0;
    int capacity = MAX_MESSAGE;
    bool channel = false;
    string filename;

    while((opt = getopt(argc, argv, "p:t:e:m:f:c")) != -1)
    {
        switch(opt)
        {
            case 'p':
                person = atoi(optarg);
                break;
            case 't':
                time = atof(optarg);
                break;
            case 'e':
                ecg = atoi(optarg);
                break;
            case 'm':
                capacity = atoi(optarg);
            case 'f':
                filename = optarg;
                break;
            case 'c':
                channel = true;
                break;
        }
    }

    int pid = fork();
    if(pid == 0)
    {
        if(capacity != 256)
        {
            char* arg[] = {"server", "-m", (char*)(to_string(capacity).c_str()), NULL};
            execvp("./server", arg);
        }
        else
        {
            char* arg[] = {"server",NULL};
            execvp("./server", arg);
        }
    }

    FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);

    //TASK THREE: REQUESTING A NEW CHANNEL

    if(channel)
    {
        MESSAGE_TYPE newrequest = NEWCHANNEL_MSG;
        chan.cwrite(&newrequest, sizeof(MESSAGE_TYPE));
        char newchannelname[100];
        chan.cread(newchannelname, sizeof(char[100]));
        cout << "Channel Name: " << newchannelname << endl;
        FIFORequestChannel newchan(newchannelname, FIFORequestChannel::CLIENT_SIDE);

        datamsg d1(1, 0.00, 2);
        newchan.cwrite(&d1, sizeof (d1));
        double result1;
        newchan.cread(&result1, sizeof (double));
        cout << "Result is: " << result1 << endl;

        datamsg d2(1, 0.004, 2);
        newchan.cwrite(&d2, sizeof (d2));
        double result2;
        newchan.cread(&result2, sizeof (double));
        cout << "Result is: " << result2 << endl;

        datamsg d3(1, 0.008, 2);
        newchan.cwrite(&d3, sizeof (d3));
        double result3;
        newchan.cread(&result3, sizeof (double));
        cout << "Result is: " << result3 << endl;

        MESSAGE_TYPE m = QUIT_MSG;
        newchan.cwrite (&m, sizeof (MESSAGE_TYPE));
    }

    struct timeval intial_time;
    struct timeval final_time;
    double totaltime;

    if(((person > 0) && (person < 16)) && ((time >= 0.000) && (time <= 59.996)) && ((ecg >= 1) && (ecg <= 2)))
    {
        gettimeofday(&intial_time, NULL);
        cout << "Channel Name: control" << endl;
        datamsg d(person, time, ecg);
        chan.cwrite(&d, sizeof (d));
        double result;
        chan.cread(&result, sizeof (double));
        gettimeofday(&final_time,NULL);
        totaltime = ((final_time.tv_sec - intial_time.tv_sec) * 1000000.0) + (final_time.tv_usec - intial_time.tv_usec);
        cout << "Time Elapsed: " << totaltime << endl;
        cout << "Result is: " << result << endl; 
    }

    if(((person > 0) && (person < 16)) && ((time == -1.0)))
    {
        cout << "Channel Name: control" << endl;
        int count = 0;
        double dtime = 0.000;
        fstream file;
        file.open("x1.csv", ios::out | ios::app);

        
        if(ecg == 0)
        {
            gettimeofday(&intial_time, NULL);
            while(count < 1000)
            {
                datamsg d1(person, dtime, 1);
                chan.cwrite(&d1, sizeof (d1));
                double result1;
                chan.cread(&result1, sizeof (double));

                datamsg d2(person, dtime, 2);
                chan.cwrite(&d2, sizeof (d2));
                double result2;
                chan.cread(&result2, sizeof (double));

                file << dtime << "," << result1 << "," << result2 << "\n";

                count += 1;
                dtime += 0.004;
            }
            gettimeofday(&final_time,NULL);
            totaltime = ((final_time.tv_sec - intial_time.tv_sec) * 1000000.0) + (final_time.tv_usec - intial_time.tv_usec);
            cout << "Time Elapsed: " << totaltime << endl;
            file.close();
        }
        else if(ecg == 1)
        {
            gettimeofday(&intial_time, NULL);
            while(count < 1000)
            {
                datamsg d1(person, dtime, 1);
                chan.cwrite(&d1, sizeof (d1));
                double result1;
                chan.cread(&result1, sizeof (double));

                file << dtime << ", " << result1 << "\n";

                count += 1;
                dtime += 0.004;
            }
            gettimeofday(&final_time,NULL);
            totaltime = ((final_time.tv_sec - intial_time.tv_sec) * 1000000.0) + (final_time.tv_usec - intial_time.tv_usec);
            cout << "Time Elapsed: " << totaltime << endl;
            file.close();
        }
        else if(ecg == 2)
        {
            gettimeofday(&intial_time, NULL);
            while(count < 1000)
            {
                datamsg d2(person, dtime, 2);
                chan.cwrite(&d2, sizeof (d2));
                double result2;
                chan.cread(&result2, sizeof (double));

                file << dtime << ", " << result2 << "\n";

                count += 1;
                dtime += 0.004;
            }
            gettimeofday(&final_time,NULL);
            totaltime = ((final_time.tv_sec - intial_time.tv_sec) * 1000000.0) + (final_time.tv_usec - intial_time.tv_usec);
            cout << "Time Elapsed: " << totaltime << endl;
            file.close();
        }
    }
    
    
    //TASK TWO: REQUESTING FILES

    if(filename != "")
    {
        cout << "Channel Name: control" << endl;
        fstream rfile;
        string outputfilepath = "received/" + filename;
        rfile.open(outputfilepath, ios::out | ios::binary);
        filemsg f(0,0);
        int size_total = sizeof(filemsg) + filename.length() + 1;
        char* buffer = new char[size_total];
        memcpy(buffer, (char*)&f, sizeof(filemsg));
        memcpy(buffer + sizeof(filemsg), filename.c_str(), filename.size() + 1);
        chan.cwrite(buffer, size_total);
        __int64_t filesize;
        chan.cread(&filesize, sizeof(__int64_t));
        delete buffer;

        int requests = filesize / capacity;
        int count = 0;
        int bytes = 0;
        struct timeval itime;
        struct timeval ftime;

        gettimeofday(&itime,NULL);
        while(count < requests)
        {
            filemsg msg (bytes, capacity);
            char* ret_buffer = new char[size_total];
            memcpy(ret_buffer, (char*)&msg, sizeof(filemsg));
            strcpy(ret_buffer + sizeof(filemsg), filename.c_str());
            chan.cwrite(ret_buffer, size_total);
            char* temp = (char*)malloc(capacity);
            chan.cread(temp, capacity);
            count++;
            bytes += capacity;
            rfile.write(temp, capacity);
            delete ret_buffer;
            delete temp;
        }

        filemsg remainder (bytes, (filesize - bytes));
        char* ret_buffer_final = new char[size_total];
        memcpy(ret_buffer_final, (char*)&remainder, sizeof(filemsg));
        strcpy(ret_buffer_final + sizeof(filemsg), filename.c_str());
        chan.cwrite(ret_buffer_final, size_total);
        char* temp2 = (char*)malloc(filesize-bytes);
        chan.cread(temp2, (filesize-bytes));
        rfile.write(temp2, (filesize-bytes));
        gettimeofday(&ftime,NULL);
        delete ret_buffer_final;
        delete temp2;
        rfile.close();
        totaltime = ((ftime.tv_sec - itime.tv_sec) * 1000000.0) + (ftime.tv_usec - itime.tv_usec);
        cout << "Time Elapsed: " << totaltime << endl;
    }

    //TASK FIVE: CLOSING CHANNELS

    // closing the channel    
    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite (&m, sizeof (MESSAGE_TYPE));
    wait(NULL);
}
