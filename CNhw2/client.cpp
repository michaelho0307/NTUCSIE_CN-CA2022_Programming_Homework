#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <string>
#include <vector>
#include "opencv2/opencv.hpp"

#include <iostream>
using namespace std;
using namespace cv;
#define clientnamelen 30000
#define OSARGMAX 2097153
//#define OSARGMAX 30000
#define BUFSIZE 1024 //must exceed 10, because of overflow
#define ERR_EXIT(a){ perror(a); exit(1); }

int initandconnect_client(char* arg1, char* arg2); // return socketfd
void userconfig(int portno,char* username); //send who i am
int clientrequest(int clientfd, char* username,bool isinit); //handle request
int clientresponse(int clientfd, char* username); //handle response
int clientkeepresponse(int clientfd,char* username); //handle put get play response
void getresponse(int clientfd);//get response from socket(use in put get play)
void getrequest(int clientfd);
int handleput(int clientfd);
int handleget(int clientfd);
int handleplay(int clientfd);
void handleexitflag(void);

char res_socket[OSARGMAX]; //use in put get play
char req_socket[OSARGMAX]; //use in put get play
bool isSENDING = false; //use when: put get play
bool isPUT = false;
bool isGET = false;
bool isPLAY = false;
int main(int argc,char* argv[]){
    if (argc != 3) ERR_EXIT("SERVER input arguments error!\n");
    if (opendir("./client_dir")== NULL){
        if (mkdir("./client_dir",0777/*mode:777*/) < 0){
            ERR_EXIT("MKDIR failed\n");
        }
        //chdir("./client_dir");
    }
    int clientfd = initandconnect_client(argv[1],argv[2]);
    if (clientfd < 0) ERR_EXIT("Client init ERROR at main!\n");
    clientrequest(clientfd,argv[1],true);
    printf("$ ");
    //Connection setup done, start to do things!
    while(1){
        //need to write request and response!
        int res;
        if ((res = clientrequest(clientfd,argv[1],false)) == -1) ERR_EXIT("CLIENT REQUEST failed\n");
        if (res!=-2){
            if (isSENDING){
                if (clientkeepresponse(clientfd,argv[1]) < 0) ERR_EXIT("CLIENT KEEPRESPONSE failed\n");
            } else{
                if (clientresponse(clientfd,argv[1]) < 0) ERR_EXIT("CLIENT RESPONSE failed\n");
            }
        }
    }
    close(clientfd);
    return 0;
}
int initandconnect_client(char* arg1, char* arg2){
    //processing IP and port
    string username = arg1;
    string ip_port = arg2;
    int splitplace = ip_port.find(":",0);
    string ip = ip_port.substr(0,splitplace);
    string port_str = ip_port.substr(splitplace+1);
    int portnum = stoi(port_str);
    //get socket fd
    struct sockaddr_in socketaddr;
    int client_socket_fd = -1;
    if ((client_socket_fd = socket(AF_INET, SOCK_STREAM,0)) < 0){
        ERR_EXIT("SOCKET failed\n");
    }
    //connect to server
    bzero(&socketaddr,sizeof(socketaddr));
    socketaddr.sin_family = AF_INET;
    char *ipaddr = (char*)ip.data();
    socketaddr.sin_addr.s_addr = inet_addr(ipaddr);
    socketaddr.sin_port = htons(portnum);
    if (connect(client_socket_fd,(struct sockaddr*)&socketaddr,sizeof(socketaddr)) < 0){
        ERR_EXIT("CONNECTION failed\n");
    }
    return client_socket_fd;
}

int clientrequest(int clientfd, char* username,bool isinit){
    char commandline[OSARGMAX];
    bzero(&commandline,sizeof(commandline));
    //2 type
    if (isinit) sprintf(commandline,"%s",username);
    else fgets(commandline,sizeof(commandline),stdin);
    //printf("client cmd:%s\n",commandline);
    //2 type
    //bool havethings = false;
    for (int i = 0; i < OSARGMAX; i++){
        if (commandline[i] == ' ' || commandline[i] == '\n'){
            if (commandline[i] == '\n'){
                printf("$ ");
                return -2;
            } 
        }else{
            break;
        }
    }
    //is get put play
    if (!isinit){
        char CMDstr[OSARGMAX];
        strcpy(CMDstr,commandline);
        vector<string> cmdlist;
        const char* knife = " \n";
        char *P_to_cut;
        P_to_cut = strtok(CMDstr,knife);
        while(P_to_cut != NULL){
            string input = P_to_cut;
            cmdlist.push_back(input);
            P_to_cut = strtok(NULL,knife);
        }
        if ((cmdlist[0] == "put" )|| (cmdlist[0] == "get" ) || (cmdlist[0] == "play" )){
            isSENDING = true;
        }
        if (cmdlist[0] == "put"){
            isPUT = true;
        }else if(cmdlist[0] == "get"){
            isGET = true;
        }else if(cmdlist[0] == "play"){
            isPLAY = true;
        }
    }
    //is get put play
    int translen = int(strlen(commandline));
    //if (translen == 2 && commandline[0] == '\r' && commandline[1] == '\n') return -2; //empty string
    char transbuff[BUFSIZE];
    bzero(&transbuff,sizeof(transbuff));
    sprintf(transbuff,"%d",translen);
    if(send(clientfd,transbuff,sizeof(transbuff),0) < 0) ERR_EXIT("Client SEND REQUEST error!\n");
    int transfered = 0;
    while (translen > 0){
        bzero(&transbuff,sizeof(transbuff));
        strncpy(transbuff,commandline+transfered,BUFSIZE);
        if(send(clientfd,transbuff,sizeof(transbuff),0) < 0) ERR_EXIT("Client SEND REQUEST error!\n");
        translen -= BUFSIZE;
        transfered += BUFSIZE;
    }
    return 0;
}
int clientresponse(int clientfd, char* username){
    char responseline[OSARGMAX];
    bzero(&responseline,sizeof(responseline));
    char buf[BUFSIZE];
    bzero(&buf,sizeof(buf));
    if (recv(clientfd,buf,sizeof(buf),0) < 0) ERR_EXIT("Client RECV RESPONSE error!\n");
    int length = atoi(buf);
    //printf("len:%d\n",length);
    int transfered = 0;
    while (transfered < length){
        bzero(&buf,sizeof(buf));
        if(recv(clientfd,buf,sizeof(buf),0) < 0) ERR_EXIT("Client RECV RESPONSE error!\n");
        strncpy(responseline+transfered,buf,BUFSIZE);
        transfered += BUFSIZE;
    }
    fprintf(stdout,"%s",responseline);
    printf("$ ");
    return 0;
}
int clientkeepresponse(int clientfd,char* username){
    if (isPUT) handleput(clientfd);
    else if (isGET) handleget(clientfd);
    else if (isPLAY) handleplay(clientfd);
    //remember to turn all booling down!!!
    isSENDING = false;
    return 0;
}
int handleput(int clientfd){
    //check permission
    getresponse(clientfd);
    if (res_socket[0] == 'P'){  //Permission denied.
        fprintf(stdout,"%s",res_socket);
        printf("$ ");
        isPUT = false;
        return 0;
    }
    //check file existence
    getresponse(clientfd);
    string targetfile = res_socket;
    string filepathstr = "./client_dir/"+targetfile;

    if (access(filepathstr.c_str(),F_OK) < 0){ //non exist
        string outputstring = targetfile+" doesn't exist.\n";
        printf("%s",(char*)outputstring.data());
        printf("$ ");
        bzero(&req_socket,sizeof(req_socket));
        strcpy(req_socket,"No.\0");
        getrequest(clientfd);
        isPUT = false;
        return 0;
    } else{
        string outputstring = "putting "+targetfile+"...\n";
        printf("%s",(char*)outputstring.data());
        bzero(&req_socket,sizeof(req_socket));
        strcpy(req_socket,"Yes.\0");
        getrequest(clientfd);
    }
    //tell file length
    FILE *lenfp = fopen(filepathstr.c_str(),"rb");
    fseek(lenfp,0,SEEK_END);
    int filelen = ftell(lenfp);
    fclose(lenfp);
    char lenbuf[BUFSIZE];
    strcpy(lenbuf,to_string(filelen).c_str());
    send(clientfd,lenbuf,sizeof(lenbuf),0);
    //start putting file
    FILE *readfp = fopen(filepathstr.c_str(),"rb");
    //printf("filelen:%d\n",filelen);
    while(filelen > 0){
        //printf("clientid is reading: %d\nfilelen: %d\n",clientfd,filelen);
        int readsize;
        if (filelen > BUFSIZE) readsize = BUFSIZE;
        else readsize = filelen;
        char rwbuffer[readsize];
        int getsize = fread(rwbuffer,1,readsize,readfp);

       

        if(send(clientfd,rwbuffer,sizeof(rwbuffer),0) < 0) ERR_EXIT("PUT send failed.\n");

        filelen -= BUFSIZE;


        char temp[BUFSIZE];
        recv(clientfd,temp,sizeof(temp),0);
    }
    //notify server EOF
    // char endbuffer[BUFSIZE];
    // strcpy(endbuffer,"-1\0");
    // if(send(clientfd,endbuffer,sizeof(endbuffer),0) < 0) ERR_EXIT("PUT EOF failed.\n");
    fclose(readfp);
    isPUT = false;
    printf("$ ");
    return 0;
}
//remember to set isGET and print $!
int handleget(int clientfd){
    //check permission
    getresponse(clientfd);
    if (res_socket[0] == 'P'){  //Permission denied.
        fprintf(stdout,"%s",res_socket);
        printf("$ ");
        isGET = false;
        return 0;
    }
    //check file existence
    getresponse(clientfd);
    if (res_socket[0] == 'N'){  //No file.
        getresponse(clientfd);
        fprintf(stdout,"%s",res_socket);
        printf("$ ");
        isGET = false;
        return 0;
    }else{
        getresponse(clientfd);
        fprintf(stdout,"%s",res_socket);
    }
    //recv file length
    char lenbuf[BUFSIZE];
    recv(clientfd,lenbuf,sizeof(lenbuf),0);
    int filelen = atoi(lenbuf);
    //printf("filelen:%d\n",filelen);
    //Getting file name and open
    getresponse(clientfd);
    //char *writepath;
    string writefilepath = "./client_dir/";
    //strcpy(writepath,writefilepath.c_str());
    int len = strlen(res_socket);
    //printf("res_scok:%s\n",res_socket);
    char temp[len];
    strcpy(temp,res_socket);
    writefilepath += temp; writefilepath += "\0";
    //printf("writefilepath:%s\n",writefilepath.c_str());
    FILE *writefp = fopen(writefilepath.c_str(),"wb");
    //get file
    while (filelen > 0){
        int readsize;
        if (filelen > BUFSIZE) readsize = BUFSIZE;
        else readsize = filelen;
        char rwbuffer[readsize];
        if(recv(clientfd,rwbuffer,sizeof(rwbuffer),0) < 0) ERR_EXIT("PUT send failed.\n");
        if(fwrite(rwbuffer,readsize,1,writefp) < 0) ERR_EXIT("Keep PUT Fwrite failed.\n");
        fflush(writefp);
        filelen -= BUFSIZE;

        char temp[BUFSIZE];
        strcpy(temp,"Goingtosend\0");
        send(clientfd,temp,sizeof(temp),0);
    }
    fclose(writefp);
    isGET = false;   
    printf("$ ");
    return 0;
}
//remember to set isPLAY and print $!
int handleplay(int clientfd){
    //check permission
    getresponse(clientfd);
    if (res_socket[0] == 'P'){  //Permission denied.
        fprintf(stdout,"%s",res_socket);
        printf("$ ");
        isPLAY = false;
        return 0;
    }
    //Check file is mpg or not exist
    getresponse(clientfd);
    if (res_socket[0] == 'N'){  //No file.
        getresponse(clientfd);
        fprintf(stdout,"%s",res_socket);
        printf("$ ");
        isPLAY = false;
        return 0;
    }else{
        getresponse(clientfd);
        fprintf(stdout,"%s",res_socket);
    }
    //Dealing with width and height
    getresponse(clientfd);
    int width = atoi(res_socket);
    getresponse(clientfd);
    int height = atoi(res_socket);
    //printf("width and height:%d %d\n",width,height);
    Mat client_img = Mat::zeros(height, width, CV_8UC3);
    if(!client_img.isContinuous()){
         client_img = client_img.clone();
    }

    while(1){
        // Press ESC on keyboard to exit
        // Notice: this part is necessary due to openCV's design.
        // waitKey function means a delay to get the next frame. You can change the value of delay to see what will happen
        // request a frame
        char startBUF[BUFSIZE];
        strcpy(startBUF,"Ask for frame.\0");
        send(clientfd,startBUF,sizeof(startBUF),0);

        bool exitflag = false;
        char c = (char)waitKey(33.3333);
        if (c==27) {
            char endBUF[BUFSIZE];
            strcpy(endBUF,"No need.\0");
            send(clientfd,endBUF,sizeof(endBUF),0);
            exitflag = true;
            //break;
        }
        //get image from server
        char SizeBUF[BUFSIZE];
        bzero(&SizeBUF,sizeof(SizeBUF));
        recv(clientfd,SizeBUF,sizeof(SizeBUF),0);
        if (SizeBUF[0] == '-'){
            break;
        }
        int imgsize = atoi(SizeBUF);
        //get frame
        int receivecount = 0,sizecount = imgsize;
        uchar *iptr = client_img.data;
        while(sizecount > 0){
            int sizela;
            if (sizecount >= BUFSIZE) sizela = BUFSIZE;
            else sizela = sizecount;
            uchar imgbuffer[sizela];
            recv(clientfd,imgbuffer,sizeof(imgbuffer),0);
            memcpy(iptr+receivecount, imgbuffer, sizela);
            receivecount+=sizela;
            sizecount -= sizela;
        }
        //send receive or dont need to send to server
        string msgre;
        if (!exitflag){
            msgre = "Ok.";
            char recbuf[BUFSIZE];
            strcpy(recbuf,msgre.c_str());
            send(clientfd,recbuf,sizeof(recbuf),0);
        }
        //printf("before playing.\n");
        //show img has problems!!!
        // show the frame 
        if (exitflag == true) break;
        imshow("Video", client_img);  

    }
    destroyAllWindows();
    printf("$ ");
    isPLAY = false;
    return 0;
}
void getresponse(int clientfd){   
    bzero(&res_socket,sizeof(res_socket));
    char buf[BUFSIZE];
    bzero(&buf,sizeof(buf));
    if (recv(clientfd,buf,sizeof(buf),0) < 0) ERR_EXIT("Client RECV RESPONSE error!\n");
    int length = atoi(buf);
    int transfered = 0;
    while (transfered < length){
        bzero(&buf,sizeof(buf));
        if(recv(clientfd,buf,sizeof(buf),0) < 0) ERR_EXIT("Client RECV RESPONSE error!\n");
        strncpy(res_socket+transfered,buf,BUFSIZE);
        transfered += BUFSIZE;
    }
}
void getrequest(int clientfd){
    int translen = int(strlen(req_socket));
    char transbuff[BUFSIZE];
    bzero(&transbuff,sizeof(transbuff));
    sprintf(transbuff,"%d",translen);
    if(send(clientfd,transbuff,sizeof(transbuff),0) < 0) ERR_EXIT("Client SEND REQUEST error!\n");
    int transfered = 0;
    while (translen > 0){
        bzero(&transbuff,sizeof(transbuff));
        strncpy(transbuff,req_socket+transfered,BUFSIZE);
        if(send(clientfd,transbuff,sizeof(transbuff),0) < 0) ERR_EXIT("Client SEND REQUEST error!\n");
        translen -= BUFSIZE;
        transfered += BUFSIZE;
    }
    bzero(&req_socket,sizeof(req_socket));
}
