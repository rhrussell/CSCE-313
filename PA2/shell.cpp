#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <vector>
#include <iterator>
#include <sstream>
#include <ctime>
using namespace std;

string trim(const string &str)
{
    const string wspace = " \t";
    const auto begin = str.find_first_not_of(wspace);
    const auto end = str.find_last_not_of(wspace);

    if(begin == string::npos)
    {
        return "";
    }

    const auto diff = end - begin + 1;

    return str.substr(begin, diff);
}

char** vec_to_char_array(vector<string>& parts)
{
    char** result = new char *[parts.size() + 1];

    for(int i = 0; i < parts.size(); i++)
    {
        result[i] = (char*) parts[i].c_str();
    }

    result[parts.size()] = NULL;

    return result;
}


vector<string> split(string str, char delimiter)
{
    vector<string> vec;
    int begin = 0;
    char ch;
    string substr;

    for(size_t i = 0; i < str.length(); i++)
    {
        ch = str[i];

        if(ch == delimiter)
        {
            substr = str.substr(begin, i - begin);
            vec.push_back(substr);
            begin = i + 1;
        }
        else if(ch == '\"')
        {
            do
            {
                i++;
            } while (str[i] != '\"');
        }
        else if(ch == '\'')
        {
            do
            {
                i++;
            } while (str[i] != '\'');
        }
    }
    if(str.length() - begin >= 1)
    {
        vec.push_back(str.substr(begin, str.length() - begin));
    }

    return vec;
}

int main()
{
    int kb = dup2(0,3);
    int terminal = dup2(1,4);

    while(true)
    {
        vector<int> bgs;
        vector<string> parts;
        vector<string> pparts;

        for(int i = 0; i < bgs.size(); i++)
        {
            if(waitpid(bgs[i], 0, WNOHANG) == bgs[i])
            {
                bgs.erase(bgs.begin() + i);
                i++;
            }
        }

        time_t current;
        time(&current);
        cout << asctime(localtime(&current)) << "~rhrussell@tamu.edu$ "; //print a prompt

        string inputline;
        dup2(kb,0);
        dup2(terminal,1);
        getline(cin, inputline); //get a line from standard inputline

        if(inputline == string("exit"))
        {
            cout << "Bye!! End of shell" << endl;
            break;
        }
        bool result = false;
        bool inresult = false;
        bool outresult = false;
        string backup;
        if(trim(inputline).find("echo") == 0)
        {
            result = true;
            int pos_begin = inputline.find('\"');
            int pos_end = inputline.find('\"', pos_begin + 1);
            int pipe_pos = inputline.find('|');

            if(pos_begin < pipe_pos && pipe_pos < pos_end)
            {
                backup = inputline;
                inputline = inputline.erase(pipe_pos, 1);
            }

            pos_begin = inputline.find('\'');
            pos_end = inputline.find('\'', pos_begin + 1);
            pipe_pos = inputline.find('|');
          
            if(pos_begin < pipe_pos && pipe_pos < pos_end)
            {
                backup = inputline;
                inputline = inputline.erase(pipe_pos, 1);
            }

            inputline.erase(remove(inputline.begin(), inputline.end(), '\"'), inputline.end());
            inputline.erase(remove(inputline.begin(), inputline.end(), '\''), inputline.end());
        }

        pparts = split(inputline, '|');
        for(int i = 0; i < pparts.size(); i++)
        {
            int fds[2];
            pipe(fds);
            
            int pid = fork();

            bool bg = false;
            int pos = trim(pparts[i]).find('&');
            if(pos != -1 && result == false)
            {
                pparts[i] = pparts[i].substr(0, pos - 1);
                bg = true;
            }
            if(trim(inputline).find("cd") == 0 && result == false)
            {
                string dirname = trim(split(inputline, ' ')[1]);
                chdir(dirname.c_str());
                continue;
            } 
            if(trim(inputline).find("pwd") == 0 && result == false)
            {
                char cwd[100];
                getcwd(cwd,sizeof(cwd));
                printf("%s", cwd);
            }
            if((trim(inputline).find("sleep") == 0) && (trim(inputline).find("&") == -1) && result == false)
            {
                int sleepnum = stoi(trim(split(inputline, ' ')[1]));
                sleep(sleepnum);
            }

            int fd = 0;
            if(pid == 0)
            {
                int pos = inputline.find('>');
                if(pos >= 0 && result == false)
                {
                    inresult = true;
                    inputline = trim(inputline);
                    string command = trim(inputline.substr(0, pos));
                    string filename = trim(inputline.substr(pos+1));
                    inputline = command;
                    fd = open(filename.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
                    dup2(fd, 1);
                    close(fd);
                }

                pos = inputline.find('<');
                if(pos >= 0 && result == false)
                {
                    outresult = true;
                    inputline = trim(inputline);
                    string command = trim(inputline.substr(0, pos));
                    string filename = trim(inputline.substr(pos+1));
                    inputline = command;
                    fd = open(filename.c_str(), O_RDONLY, S_IWUSR | S_IRUSR);
                    dup2(fd, 0);
                    close(fd);
                }

                if(i < pparts.size() - 1)
                {
                    dup2(fds[1], 1);
                }

                if(result == true)
                {
                    backup.erase(remove(backup.begin(), backup.end(), '\"'), backup.end());
                    backup.erase(remove(backup.begin(), backup.end(), '\''), backup.end());
                    parts = split(backup, ' ');
                    char** args = vec_to_char_array(parts);
                    execvp(args[0], args);
                }
                else if(inresult == true || outresult == true)
                {
                    inputline.erase(remove(inputline.begin(), inputline.end(), '\"'), inputline.end());
                    inputline.erase(remove(inputline.begin(), inputline.end(), '\''), inputline.end());
                    parts = split(inputline, ' ');
                    char** args = vec_to_char_array(parts);
                    execvp(args[0], args);
                }
                else
                {
                    inputline = trim(pparts[i]);
                    inputline.erase(remove(inputline.begin(), inputline.end(), '\"'), inputline.end());
                    inputline.erase(remove(inputline.begin(), inputline.end(), '\''), inputline.end());
                    parts = split(inputline, ' ');
                    char** args = vec_to_char_array(parts);
                    execvp(args[0], args);
                }
            }
            else
            {
                if(!bg)
                {
                    if(i == pparts[i].size()-1)
                    {
                        waitpid(pid, 0, 0);
                    }
                    else
                    {
                        bgs.push_back(pid);
                    }
                }
                else
                {
                    bgs.push_back(pid);
                }
                dup2(fds[0], 0);
                close(fds[1]);
            }
        }
    } 
}