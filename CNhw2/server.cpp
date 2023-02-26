#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <string>
#include <signal.h>
#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include "opencv2/opencv.hpp"
using namespace std;
using namespace cv;

//define
#define ERR_EXIT(a){ perror(a); exit(1); }
#define OSARGMAX 2097153
//#define OSARGMAX 30000
#define clientnamelen 30000
#define BUFSIZE 1024 //must exceed 10, because of overflow

//char cmdget [OSARGMAX];
char CMDcast[OSARGMAX];
char *tempbuf;
fd_set all_fds, read_fds, write_fds;
int init_server(int portnumber); //return socket file descriptor
int handleclient(int clientfd); //handle request and send response to client
int clientexithandle(int clientfd);
int handlerecv(int clientfd);
void handlesend(int clientfd,char *output);
void dealingFILE(string currentuser,string targetfile,bool isPUT,int clientfd);
void listfile(string currentuser,int clientfd);
void dealingPUT(string currentuser,string targetfile,int clientfd);
void dealingGET(string currentuser,string targetfile,int clientfd);
void dealingPLAY(string currentuser,string targetfile,int clientfd);
void keepPUT(int clientfd);
void keepGET(int clientfd);
void keepPLAY(int clientfd);
int exitFD;
//special structure
VideoCapture cap[1024];
Mat server_img[1024];
map <int,string> FDtoUSER;
set <string> bannedlist;
FILE *readfp[1024];
FILE *writefp[1024];
int PUTflag[1024];
int GETflag[1024];
int PLAYflag[1024];
//signal handle
void sigPIPE(int signo){
    clientexithandle(exitFD);
}
int main(int argc, char* argv[]) {
    for (int i = 0; i < 1024; i++){
        PUTflag[i] = 0;
        GETflag[i] = 0;
        PLAYflag[i] = 0;
        readfp[i] = NULL;
        writefp[i] = NULL;
    }
    if (argc != 2) ERR_EXIT("SERVER input arguments error!\n");
    if (opendir("./server_dir") == NULL){
        if (mkdir("./server_dir",0777/*mode:777*/) < 0){
            ERR_EXIT("MKDIR failed\n");
        }
        //chdir("./server_dir");
    }
    signal(SIGPIPE, sigPIPE);
    int portnumber = atoi(argv[1]);
    int server_socket_fd = init_server(portnumber); //listening fd
    //using select
    int MAXfd = 1023;
    FD_ZERO(&all_fds);
    FD_ZERO(&write_fds);
    FD_SET(server_socket_fd,&all_fds);
    while(1){
        read_fds = all_fds;
        //printf("startselect\n");
        if (select(MAXfd+1,&read_fds,&write_fds,NULL,NULL) < 0)
            ERR_EXIT("SELECT failed\n");
        //printf("finishselect\n");    
        for (int currentFD = 0; currentFD < MAXfd; currentFD++){
            exitFD = currentFD;
            if (FD_ISSET(currentFD,&read_fds)){
                //new connection
                if (server_socket_fd == currentFD){
                    struct sockaddr_in client_sockaddr;
                    int socklen = sizeof(client_sockaddr);
                    int client_conn_fd = accept(server_socket_fd,(sockaddr*)&client_sockaddr,(socklen_t*)&socklen);
                    //fflush(stdout);//fflush stdout
                    if (client_conn_fd < 0){
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        ERR_EXIT("Client ACCEPT error!\n");
                    } 
                    else{
                        //receive USERNAME
                        if(handlerecv(client_conn_fd) < 0){
                            clientexithandle(client_conn_fd);
                            continue;
                        }
                        char *buf = CMDcast;
                        //printf("buf:%s",buf);
                        printf("Accept a new connection on socket [%d]. Login as %s.\n",client_conn_fd,buf);
                        //Map function
                        string Userbuf = buf;
                        FDtoUSER[client_conn_fd] = Userbuf;
                        //create folder
                        string startadd = "./server_dir/";
                        char *newfilecreate = (char*)startadd.data();
                        strcat(newfilecreate,buf);
                        //printf("files:%s\n",newfilecreate);
                        if (!opendir(newfilecreate)){
                            if (mkdir(newfilecreate,0777) < 0){
                                ERR_EXIT("MKDIR failed\n");
                            }
                        }
                    }
                    if (client_conn_fd > MAXfd) MAXfd = client_conn_fd;
                    FD_SET(client_conn_fd,&all_fds);
                }
                //handling request
                else{
                    //printf("clientfd:%d\n",currentFD);
                    if(handleclient(currentFD) < 0) ERR_EXIT("HANDLE CLIENT failed\n");
                }
            }else if (FD_ISSET(currentFD,&write_fds)){
                if(handleclient(currentFD) < 0) ERR_EXIT("HANDLE CLIENT failed\n");
            }
        }
    }
    close(server_socket_fd);
    return 0;
}

int init_server(int portnumber){
    int socket_fd = -1;
    struct sockaddr_in serveraddr; //represent bind addr 
    if ((socket_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        ERR_EXIT("SOCKET failed\n");
    }
    //set server addr info
    bzero(&serveraddr,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(portnumber);
    if (bind(socket_fd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0){
        ERR_EXIT("BIND failed\n");
    }
    // Listen on socket_fd, MAXaccept = 80000
    if (listen(socket_fd,80000) < 0){
        ERR_EXIT("LISTEN failed\n");
    }
    //accept in loop(check accept in every loop)
    return socket_fd;
}
int clientexithandle(int clientfd){
    if (readfp[clientfd] != NULL) fclose(readfp[clientfd]);
    if (writefp[clientfd] != NULL) fclose(writefp[clientfd]);
    PUTflag[clientfd] = 0;
    GETflag[clientfd] = 0;
    PLAYflag[clientfd] = 0;
    FD_CLR(clientfd,&all_fds);
    if (FD_ISSET(clientfd,&write_fds)){
        FD_CLR(clientfd,&write_fds);
    }
    close(clientfd);
    return 0;
}
int handleclient(int clientfd){
    //printf("dealing with:%d\n",clientfd);
    //deal with flags
    if (PUTflag[clientfd] > 0){
        //printf("lalala\n");
        keepPUT(clientfd);
        return 0;
    }
    if (GETflag[clientfd] > 0){
        keepGET(clientfd);
        return 0;
    }
    if (PLAYflag[clientfd] > 0){
        keepPLAY(clientfd);
        return 0;
    }
    //printf("clientharec:%d\n",clientfd);
    if(handlerecv(clientfd) < 0){
        clientexithandle(clientfd);
        return 0;
    }
    //printf("cliearec:%d\n",clientfd);
    //int start = -1, end = -2;
    //string originstr(CMDcast);
    //char* CMD = CMDcast;
    char CMD[OSARGMAX];
    //cmd works good!
    strcpy(CMD,CMDcast);
    vector<string> cmdlist;
    const char knife[3] = " \n";
    //char *P_to_cut;
    char *P_to_cut;
    P_to_cut = strtok(CMDcast,knife);
    while(P_to_cut != NULL){
        //printf("cmd:%s\n",P_to_cut);
        string input = P_to_cut;
        cmdlist.push_back(input);
        P_to_cut = strtok(NULL,knife);
    }
    //use "cmdlist" to operate command
    string currentuser = FDtoUSER[clientfd];
    string outputstring;
    if ((cmdlist[0] == "")){
        outputstring = "Command not found.\n";
        tempbuf = (char*)outputstring.data();
        handlesend(clientfd,tempbuf);
        strcpy(CMDcast,"\0");
        strcpy(tempbuf,"\0");
        return 0;
    }
    if ((cmdlist[0] == "ban")||(cmdlist[0] == "unban")||(cmdlist[0] == "blocklist")){
        if (currentuser != "admin"){
            outputstring = "Permission denied.\n";
        }else{
            if (cmdlist[0] == "ban"){
                outputstring = "";
                int len = cmdlist.size();
                for (int i = 1; i < len; i++){
                    if (cmdlist[i] == "admin"){
                        outputstring += "You cannot ban yourself!\n";
                    }
                    else{
                        if (bannedlist.count(cmdlist[i])==1){ //already in banned list
                            string temp = "User "+cmdlist[i]+" is already on the blocklist!\n";
                            outputstring += temp;
                        }
                        else{
                            bannedlist.insert(cmdlist[i]);
                            string temp = "Ban "+cmdlist[i]+" successfully!\n";
                            outputstring += temp;
                        }
                    }
                }
                if (len == 1) outputstring+="\0";
            }
            else if (cmdlist[0] == "unban"){
                outputstring = "";
                int len = cmdlist.size();
                for (int i = 1; i < len; i++){
                    if (bannedlist.count(cmdlist[i]) == 1){ //In banned list
                        bannedlist.erase(cmdlist[i]);
                        string temp = "Successfully removed "+cmdlist[i]+" from the blocklist!\n";
                        outputstring += temp;
                    }
                    else{
                        string temp = "User "+cmdlist[i]+" is not on the blocklist!\n";
                        outputstring += temp;
                    }
                }
                if (len == 1) outputstring+="\0";
            }
            else if (cmdlist[0] == "blocklist"){
                outputstring = "";
                for (const auto &iter: bannedlist){
                    string temp = iter+"\n";
                    outputstring+= temp;
                }
                if (outputstring == "") outputstring += "\0";
            }
        }
        tempbuf = (char*)outputstring.data();
        handlesend(clientfd,tempbuf);
        strcpy(CMDcast,"\0");
        strcpy(tempbuf,"\0");
        return 0;
    }
    else if ((cmdlist[0] == "ls")||(cmdlist[0] == "play")){
        if (bannedlist.count(currentuser)){
            outputstring = "Permission denied.\n";
            tempbuf = (char*)outputstring.data();
            handlesend(clientfd,tempbuf);
            strcpy(CMDcast,"\0");
            strcpy(tempbuf,"\0");
            return 0;
        }
        if (cmdlist[0] == "ls"){
            listfile(currentuser,clientfd);
            strcpy(CMDcast,"\0");
            strcpy(tempbuf,"\0");
            return 0;
        }else if (cmdlist[0] == "play"){
            outputstring = "Ok.\n";
            tempbuf = (char*)outputstring.data();
            handlesend(clientfd,tempbuf);
            strcpy(CMDcast,"\0");
            strcpy(tempbuf,"\0");
            string targetfile = cmdlist[1]+"\0";
            dealingPLAY(currentuser,targetfile,clientfd);
            strcpy(CMDcast,"\0");
            strcpy(tempbuf,"\0");
            return 0;
        }
    }
    else if ((cmdlist[0] == "put")||(cmdlist[0] == "get")){
        //check permission
        if (bannedlist.count(currentuser)){
            outputstring = "Permission denied.\n";
            tempbuf = (char*)outputstring.data();
            handlesend(clientfd,tempbuf);
            strcpy(CMDcast,"\0");
            strcpy(tempbuf,"\0");
            return 0;
        } else{
            outputstring = "Ok.\n";
            tempbuf = (char*)outputstring.data();
            handlesend(clientfd,tempbuf);
            strcpy(CMDcast,"\0");
            strcpy(tempbuf,"\0");
        }
        //file existence+transmission
        string targetfile = cmdlist[1]+"\0";
        if (cmdlist[0] == "put"){
            dealingPUT(currentuser,targetfile,clientfd);
        }
        if (cmdlist[0] == "get"){
            dealingGET(currentuser,targetfile,clientfd);
        }
        strcpy(CMDcast,"\0");
        strcpy(tempbuf,"\0");
        return 0;
    }
    else{
        if (bannedlist.count(currentuser)){
            outputstring = "Permission denied.\n";
            tempbuf = (char*)outputstring.data();
            handlesend(clientfd,tempbuf);
            strcpy(CMDcast,"\0");
            strcpy(tempbuf,"\0");
            return 0;
        }
        outputstring = "Command not found.\n";
        tempbuf = (char*)outputstring.data();
        handlesend(clientfd,tempbuf);
    }
    strcpy(CMDcast,"\0");
    strcpy(tempbuf,"\0");
    return 0;
}

int handlerecv(int clientfd){
    bzero(&CMDcast,sizeof(CMDcast));
    char buf[BUFSIZE];
    bzero(&buf,sizeof(buf));
    int recvno = recv(clientfd,buf,sizeof(buf),0);
    if(recvno < 0) {ERR_EXIT("SERVER handle RECV error!\n");}
    else if (recvno == 0) return -1;
    int length = atoi(buf);
    int transfered = 0;
    while (transfered < length){
        bzero(&buf,sizeof(buf));
        if(recv(clientfd,buf,sizeof(buf),0) < 0) ERR_EXIT("SERVER handle RECV error!\n");
        strncpy(CMDcast+transfered,buf,BUFSIZE);
        transfered += BUFSIZE;
    }
    //printf("casting:%s\n",CMDcast);
    return 0;
}
void handlesend(int clientfd,char *output){
    char transbuff[BUFSIZE];
    int translen = int(strlen(output));
    sprintf(transbuff,"%d",translen);
    if (send(clientfd,transbuff,sizeof(transbuff),0) < 0) ERR_EXIT("Handlesend failed!\n");
    int transfered = 0;
    while (translen > 0){
        bzero(&transbuff,sizeof(transbuff));
        strncpy(transbuff,output+transfered,BUFSIZE);
        if(send(clientfd,transbuff,sizeof(transbuff),0) < 0) ERR_EXIT("Client SEND REQUEST error!\n");
        translen -= BUFSIZE;
        transfered += BUFSIZE;
    }
    
}

void listfile(string currentuser,int clientfd){
    DIR *current_dir;
    struct dirent *ptr_file;
    string userdirstr = "./server_dir/"+currentuser;
    char *userdirpoint = (char*)userdirstr.data();
    current_dir = opendir(userdirpoint);
    int filecount = 0;
    string outputstr = "";
    while ((ptr_file = readdir(current_dir)) != NULL){
        string name = ptr_file->d_name;
        if (name != ".." && name != ".") outputstr += (name +"\n");
        filecount++;
    }
    if (filecount == 2) outputstr += "\0";
    tempbuf = (char*)outputstr.data();
    //printf("the string line:%s\n",outputstr.c_str());
    //printf("lscmd:%s\n",tempbuf);
    handlesend(clientfd,tempbuf);
}

void dealingPUT(string currentuser,string targetfile,int clientfd){
    //check file existence
    strcpy(tempbuf,"\0");
    tempbuf = (char*)targetfile.data();
    handlesend(clientfd,tempbuf);
    //client response There is file or not
    handlerecv(clientfd);
    char *res = CMDcast;
    if (res[0] == 'N'){ //No file
        return;
    }
    //open write fp & Flag to 1
    string filepathstr = "./server_dir/"+currentuser+"/"+targetfile;
    writefp[clientfd] = fopen(((char*)filepathstr.c_str()),"wb");
    //receive file len
    char lenbuf[BUFSIZE];
    recv(clientfd,lenbuf,sizeof(lenbuf),0);
    int filelen = atoi(lenbuf);
    PUTflag[clientfd] = filelen; 
    strcpy(CMDcast,"\0");
    strcpy(tempbuf,"\0");
}
void dealingGET(string currentuser,string targetfile,int clientfd){
    //check file existence
    string filepathname = "./server_dir/\0"+currentuser+"/\0"+targetfile;
    if (access(filepathname.c_str(),F_OK) < 0){  //file non existence
        string msg = "No.\0";
        tempbuf = (char*)msg.data();
        //strcpy(tempbuf,"No.\0");
        handlesend(clientfd,tempbuf);
        string denymsg = targetfile+" doesn't exist.\n";
        tempbuf = (char*)denymsg.data();
        //strcpy(tempbuf,denymsg.c_str());
        handlesend(clientfd,tempbuf);
        strcpy(tempbuf,"\0");
        return;
    } else{
        string msg = "Yes.\0";
        tempbuf = (char*)msg.data();
        handlesend(clientfd,tempbuf);
        string accmsg = "getting "+targetfile+"...\n";
        tempbuf = (char*)accmsg.data();
        handlesend(clientfd,tempbuf);
        strcpy(tempbuf,"\0");
    }
    //printf("filepath:%s\n",filepathname.c_str());
    //send file length
    FILE *lenfp = fopen(filepathname.c_str(),"rb");
    fseek(lenfp,0,SEEK_END);
    int filelen = ftell(lenfp);
    fclose(lenfp);
    char lenbuf[BUFSIZE];
    strcpy(lenbuf,to_string(filelen).c_str());
    send(clientfd,lenbuf,sizeof(lenbuf),0);

    //open read fp & Flag to 1
    tempbuf = (char*)targetfile.data();
    handlesend(clientfd,tempbuf);
    readfp[clientfd] = fopen((char*)filepathname.c_str(),"rb");
    GETflag[clientfd] = filelen;
    FD_SET(clientfd,&write_fds);
    strcpy(CMDcast,"\0");
    strcpy(tempbuf,"\0");
    //printf("endfunction\n");
    return;
}
void dealingPLAY(string currentuser,string targetfile,int clientfd){
    string filepathname = "./server_dir/\0"+currentuser+"/\0"+targetfile;
    if (access(filepathname.c_str(),F_OK) < 0){  //file non existence
        string msg = "No.\0";
        tempbuf = (char*)msg.data();
        handlesend(clientfd,tempbuf);
        string denymsg = targetfile+" doesn't exist.\n";
        tempbuf = (char*)denymsg.data();
        handlesend(clientfd,tempbuf);
        strcpy(tempbuf,"\0");
        return;
    } else{
        //is .mpg or not
        int len = targetfile.size();
        string filetype = targetfile.substr(len-4,len);
        if (filetype != ".mpg"){
            string msg = "No.\0";
            tempbuf = (char*)msg.data();
            handlesend(clientfd,tempbuf);
            string denymsg = targetfile+" is not an mpg file.\n";
            tempbuf = (char*)denymsg.data();
            handlesend(clientfd,tempbuf);
            strcpy(tempbuf,"\0");
            return;
        }
        string msg = "Yes.\0";
        tempbuf = (char*)msg.data();
        handlesend(clientfd,tempbuf);
        string accmsg = "playing the video...\n";
        tempbuf = (char*)accmsg.data();
        handlesend(clientfd,tempbuf);
        strcpy(tempbuf,"\0");
    }
    //Dealing with video
    //Mat server_img,client_img;
    //printf("filepath:%s\n",filepathname.c_str());
    //printf("ready to cap open\n");
    cap[clientfd].open(filepathname.c_str());
    //printf("ready to cap open\n");
    int width = cap[clientfd].get(CAP_PROP_FRAME_WIDTH);
    int height = cap[clientfd].get(CAP_PROP_FRAME_HEIGHT);
    server_img[clientfd] = Mat::zeros(height, width, CV_8UC3);
    //server_img = Mat::zeros(540, 960, CV_8UC3);
    //printf("width height:%d %d\n",width,height);
    //send width and height to client
    char LEN[BUFSIZE];
    sprintf(LEN,"%d%c",width,'\0');
    tempbuf = LEN;
    handlesend(clientfd,tempbuf);
    strcpy(tempbuf,"\0");
    sprintf(LEN,"%d%c",height,'\0');
    tempbuf = LEN;
    handlesend(clientfd,tempbuf);
    strcpy(tempbuf,"\0");
    //printf("finish width height\n");
    if(!server_img[clientfd].isContinuous()){
         server_img[clientfd] = server_img[clientfd].clone();
    }
    PLAYflag[clientfd] = 1;
    //FD_SET(clientfd,&write_fds);
}
void keepPUT(int clientfd){
    if (PUTflag[clientfd] > 0){
        

        int filelen = PUTflag[clientfd];
        int readsize;
        if (filelen > BUFSIZE) readsize = BUFSIZE;
        else readsize = filelen;
        char rwbuffer[readsize];
        if(recv(clientfd,rwbuffer,sizeof(rwbuffer),0) < 0) ERR_EXIT("PUT send failed.\n");
        if(fwrite(rwbuffer,readsize,1,writefp[clientfd]) < 0) ERR_EXIT("Keep PUT Fwrite failed.\n");
        fflush(writefp[clientfd]);
        PUTflag[clientfd] -= BUFSIZE;

        char temp[BUFSIZE];
        strcpy(temp,"Goingtosend\0");
        send(clientfd,temp,sizeof(temp),0);
    }
    if (PUTflag[clientfd] <= 0){
        //printf("finishput1_clientfd:%d\n",clientfd);
        fclose(writefp[clientfd]);
        //printf("finishput_clientfd:%d\n",clientfd);
        PUTflag[clientfd] = 0;
    }
}
void keepGET(int clientfd){
    if(GETflag[clientfd] > 0){
        int filelen = GETflag[clientfd];
        int readsize;
        if (filelen > BUFSIZE) readsize = BUFSIZE;
        else readsize = filelen;
        char rwbuffer[readsize];
        int getsize = fread(rwbuffer,1,readsize,readfp[clientfd]);
        //printf("getsize:%d\n",getsize);
        if(send(clientfd,rwbuffer,sizeof(rwbuffer),0) < 0) ERR_EXIT("PUT send failed.\n");
        GETflag[clientfd] -= BUFSIZE;

        char temp[BUFSIZE];
        recv(clientfd,temp,sizeof(temp),0);
    }
    if (GETflag[clientfd] <= 0){
        FD_CLR(clientfd,&write_fds);
        fclose(readfp[clientfd]);
        GETflag[clientfd] = 0;
    }
}
void keepPLAY(int clientfd){
    // if (FD_ISSET(clientfd,&read_fds)){
    //     char endBB[BUFSIZE];
    //     recv(clientfd,endBB,sizeof(endBB),0);
    //     //handlerecv(clientfd); //CMDcast
    //     if (strcmp(endBB,"ENDLA") == 0){
    //         cap.release();
    //         PLAYflag[clientfd] = 0;
    //         FD_CLR(clientfd,&write_fds);
    //         printf("finishplaying\n");
    //         bzero(&CMDcast,sizeof(CMDcast));
    //         tempbuf = CMDcast;
    //         return;
    //     }
    //     else ERR_EXIT("Something goes wrong!\n");
    // }
    //receive client msg
    char startBUF[BUFSIZE];
    recv(clientfd,startBUF,sizeof(startBUF),0);

    if(!server_img[clientfd].isContinuous()){
         server_img[clientfd] = server_img[clientfd].clone();
    }
    cap[clientfd] >> server_img[clientfd];
    int imgSize = server_img[clientfd].total() * server_img[clientfd].elemSize();
    if (imgSize == 0){
        uchar imgbuffer[BUFSIZE];
        for (int i = 0;i < BUFSIZE; i++){
            if (i == 0) imgbuffer[i] = '-';
            else if (i==1)imgbuffer[i] = '1';
            else imgbuffer[i] = '2';
        }
        send(clientfd,imgbuffer,sizeof(imgbuffer),0);
        bzero(&imgbuffer,sizeof(imgbuffer));
        cap[clientfd].release();
        PLAYflag[clientfd] = 0;
        //FD_CLR(clientfd,&write_fds);
        strcpy(CMDcast,"\0");
        strcpy(tempbuf,"\0");
        return;
    }
    char SizeBUF[BUFSIZE];
    for (int i = 0; i < BUFSIZE; i++) SizeBUF[i] = 'x';
    sprintf(SizeBUF,"%d%c",imgSize,'\0');
    send(clientfd,SizeBUF,sizeof(SizeBUF),0);
    bzero(&SizeBUF,sizeof(SizeBUF));
    //printf("Already send frame size:%s\n.",SizeBUF);
    int sizecounter = imgSize,alreadycount = 0;
    while (sizecounter > 0){
        int sizela;
        if (sizecounter >= BUFSIZE) sizela = BUFSIZE;
        else sizela = sizecounter;
        uchar imgbuffer[sizela];
        memcpy(imgbuffer, server_img[clientfd].data+alreadycount, sizela);
        send(clientfd,imgbuffer,sizeof(imgbuffer),0);
        sizecounter -= sizela;
        alreadycount += sizela;
    }
    //receive client
    char recbuf[BUFSIZE];
    recv(clientfd,recbuf,sizeof(recbuf),0);
    //printf("recbuf:%s\n",recbuf);
    if (recbuf[0]=='N'){
        cap[clientfd].release();
        PLAYflag[clientfd] = 0;
        //FD_CLR(clientfd,&write_fds);
        //printf("finishplaying\n");
        //bzero(&CMDcast,sizeof(CMDcast));
        //tempbuf = CMDcast;
        return;
    }
}
