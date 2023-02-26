#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <zlib.h>
#include <signal.h>
#include "opencv2/opencv.hpp"
using namespace std;
using namespace cv;
#define ERR_EXIT(msg) {fprintf(stderr,msg);exit(0);}
#define OSARGMAX 2097153
typedef struct {
	int length; //imgsize
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;  //wh
	int ack;
    unsigned long checksum;
} HEADER;

typedef struct{
	HEADER header;
	char data[1000];
} SEGMENT;

//global variable
int wh = -1;
int windowsize = 1;
int thres = 16;
int seqnum = 1,acknum = 0;
int beensend = 0;
int sendpktcount = 0; //how many packet inside the windows
int tempbufcount = 0;
bool perfect = false;
SEGMENT *windowpkt,*tempbuf;
char mpgtempname[OSARGMAX];
void sendvideo(int sockno, struct sockaddr_in sender,struct sockaddr_in agent);
void msgsender(int sockno,struct sockaddr_in sender,struct sockaddr_in agent,bool endflag);
void msghandler(int sockno, struct sockaddr_in sender,struct sockaddr_in agent, char msg[1000],bool isFIN);
int main(int argc, const char** argv) {
    //dealing with input
    if (argc != 4) ERR_EXIT("Uncorrect argument number!\n");
    int sender_port,agent_port;
    char agent_IP[50];
    sscanf(argv[1], "%d",&sender_port);
    sscanf(argv[2],"%[^:]:%d",agent_IP,&agent_port);
    sscanf(argv[3],"%s",mpgtempname);
    string mpgname_str = mpgtempname;
    int mpglen = mpgname_str.size();
    if (mpglen < 4 || mpgname_str.substr(mpglen-4) != ".mpg"){
        ERR_EXIT("Not mpg file!\n");
    }
    if (mpgname_str[0] != '.'){
        mpgname_str = "./"+mpgname_str;
    }
    strcpy(mpgtempname,mpgname_str.c_str()); //    ./short.mpg
    if (strcmp(agent_IP,"0.0.0.0") == 0 || strcmp(agent_IP,"local") == 0 || strcmp(agent_IP,"localhost") == 0){
        agent_IP[0] = '\0';
        strcpy(agent_IP,"127.0.0.1\0");
    }
    // set UDP socket
    int send_socket = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in sender, agent;
    sender.sin_family = AF_INET;
    sender.sin_port = htons(sender_port);
    //sender.sin_addr.s_addr = inet_addr("127.0.0.1");
    sender.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agent_port);
    agent.sin_addr.s_addr = inet_addr(agent_IP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));
    bind(send_socket,(struct sockaddr *)&sender,sizeof(sender));
    //set timeout
    struct timeval timeoutlen = {1,0}; // timeout for 1s 
    int ret = setsockopt(send_socket,SOL_SOCKET,SO_RCVTIMEO,&timeoutlen,sizeof(timeoutlen));
    //finish setting
    //set window pkt container
    tempbuf = new SEGMENT[1000000];
    //print test
    // socklen_t agentsize = sizeof(agent);
    // SEGMENT temp1,temp2;
    // char printtest[1000] = "Hello from the otherside!";
    // strncpy(temp1.data,printtest,1000);
    // temp1.header.ack = 0;
    // printf("finish printing\n");
    // printf("thesizeoftemp1: %ld\n",sizeof(SEGMENT));
    // sendto(send_socket, &temp1, 1032, 0, (struct sockaddr *)&agent,agentsize);
    // //memset(thetest,0,sizeof(thetest));
    // printf("finish sending\n");
    // printf("wait for get\n");
    // recvfrom(send_socket,&temp2,sizeof(SEGMENT),0,(struct sockaddr *)&agent,&agentsize);
    // printf("Sender get:%s\n",temp2.data);
    //sending packets
    sendvideo(send_socket,sender,agent);
    return 0;
}
void sendvideo(int sockno, struct sockaddr_in sender,struct sockaddr_in agent){
    VideoCapture cap(mpgtempname);
    Mat server_img;
    int width = cap.get(CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CAP_PROP_FRAME_HEIGHT);
    char whbuf[1000];
    //send width
    memset(whbuf, 0, 1000);
    sprintf(whbuf,"%d%c",width,'\0');
    msghandler(sockno,sender,agent,whbuf,false);
    msgsender(sockno,sender,agent,false);
    //send height
    memset(whbuf, 0, 1000);
    sprintf(whbuf,"%d%c",height,'\0');
    //printf("beforehand\n");
    msghandler(sockno,sender,agent,whbuf,false);
    //printf("afterhand\n");
    msgsender(sockno,sender,agent,false);
    //printf("lalaland\n");
    //wh = width*10000+height; //imp!
    server_img = Mat::zeros(height, width, CV_8UC3);
    //how to send size to client???
    if(!server_img.isContinuous()){
         server_img = server_img.clone();
    }
    while (1){
        cap >> server_img;
        int imgSize = server_img.total() * server_img.elemSize();
        //send imgsize
        char imgbuf[1000];
        memset(imgbuf,0,1000);
        sprintf(imgbuf,"%d%c",imgSize,'\0');
        msghandler(sockno,sender,agent,imgbuf,false);
        msgsender(sockno,sender,agent,false);
        if (imgSize == 0){
            //handling end???
            char buffer[1000];
            memset(buffer, 0, 1000);
            msghandler(sockno,sender,agent,buffer,true);
            msgsender(sockno,sender,agent,true);
            break;
        }
        //send img to client???
        int sizecounter = imgSize,alreadycount = 0;
        while (sizecounter > 0){
            int sizela;
            if (sizecounter >= 1000) sizela = 1000;
            else sizela = sizecounter;
            char buffer[1000];
            // for (int i = 0; i < 1000; i++){
            //     uchar* sig = server_img.data+alreadycount+i;
            //     uchar it = *sig;
            //     buffer[i] = it-128;
            // }
            memcpy(buffer, server_img.data+alreadycount, sizela);
            msghandler(sockno,sender,agent,buffer,false);
            msgsender(sockno,sender,agent,false);
            sizecounter -= sizela;
            alreadycount += sizela;
        }
    }
    cap.release();
}
void msghandler(int sockno, struct sockaddr_in sender,struct sockaddr_in agent, char msg[1000],bool isFIN){
    //SEGMENT temp;
    tempbuf[tempbufcount].header.length = 0;
    tempbuf[tempbufcount].header.seqNumber = seqnum;
    seqnum++;
    tempbuf[tempbufcount].header.ackNumber = acknum;
    if (isFIN) tempbuf[tempbufcount].header.fin = 1;
    else tempbuf[tempbufcount].header.fin = 0;
    tempbuf[tempbufcount].header.syn = 0;
    tempbuf[tempbufcount].header.ack = 0;
    //tempbuf[tempbufcount].header.checksum = crc32(0L, (const Bytef *)msg, 1000);
    //printf("No.: %d ,mycrc32:%lx\n",seqnum-1,crc32(0L, (const Bytef *)msg, 1000));
    // char front10[10];
    // strncpy(front10,msg,9);
    // printf("front10:%s",front10);
    memcpy(tempbuf[tempbufcount].data,msg,1000);
    //strncpy(tempbuf[tempbufcount].data,msg,1000);
    //tempbuf[tempbufcount] = temp;
    tempbufcount++;
}
void msgsender(int sockno,struct sockaddr_in sender,struct sockaddr_in agent,bool endflag){
    socklen_t agentsize = sizeof(agent);
    //int itercount = 0;  //tmp
    while ((!endflag&&(tempbufcount >= windowsize))||(endflag&&(tempbufcount > 0))){
        //if (seqnum > 335) exit(1); //tmp
        //tmp
        // itercount++;
        // if (itercount > 10) exit(1);
        //tmp
        int winsize = -1;
        if (endflag) winsize = min(windowsize,tempbufcount);
        else winsize = windowsize;
        windowpkt = new SEGMENT[winsize];
        for (int i = 0; i < winsize; i++){
            windowpkt[i] = tempbuf[i];
        }
        //deal with send
        for (int i = 0; i < winsize; i++){
            if (windowpkt[i].header.seqNumber > beensend){
                if (windowpkt[i].header.fin == 1){
                    endflag = true;
                    printf("send\tfin\n");
                }else{
                    printf("send\tdata\t#%d,\twinSize = %d\n",windowpkt[i].header.seqNumber,windowsize);
                }
                beensend++;
            }else{
                if (windowpkt[i].header.fin == 1){
                    printf("resnd\tfin\n");
                }else{
                    printf("resnd\tdata\t#%d,\twinSize = %d\n",windowpkt[i].header.seqNumber,windowsize);
                }
            }
            //printf("data:%s\n",windowpkt[i].data);
            windowpkt[i].header.checksum = crc32(0L, (const Bytef *)windowpkt[i].data, 1000);
            //printf("Mycrc32(before send):%lx\n",crc32(0L, (const Bytef *)windowpkt[i].data, 1000));
            sendto(sockno, &windowpkt[i], sizeof(SEGMENT), 0, (struct sockaddr *)&agent,agentsize);
        }
        //deal with recv
        int localack = 0;
        for (int i = 0; i < winsize; i++){
            SEGMENT temp;
            memset(&temp,0,sizeof(SEGMENT));
            int ret = recvfrom(sockno,&temp,sizeof(SEGMENT),0,(struct sockaddr *)&agent,&agentsize);
            if (temp.header.syn == 1) perfect = true;
            if (ret == -1/*ret == EWOULDBLOCK || ret == EAGAIN*/){
                //packet loss!!!
                //continue;
                //printf("losssla\n");
                if (perfect) i--;
                else break;
            }else{
                if (acknum+1 == temp.header.ackNumber){
                    acknum++;
                    localack++;
                }
                //print recv ack
                // if (temp.header.ackNumber == 0){
                //     printf("Ret,errorno: %d\n",ret);
                // }else{
                //     printf("Ret,correctno: %d\n",ret);
                // }
                if (temp.header.fin == 1){
                    printf("recv\tfinack\n");
                }else{
                    printf("recv\tack\t#%d\n",temp.header.ackNumber);
                }
            }
        }
        //after sending and recv
        if (localack != winsize){  //should adjust
            thres = max((windowsize)/2,1);
            printf("time\tout,\t\tthreshold = %d\n",thres);
            windowsize = 1;
            for (int i = 0; i < tempbufcount-localack; i++){
                tempbuf[i] = tempbuf[i+localack];
            }
            tempbufcount -= localack;
            //seqnum = acknum+1;
        }else{   //window size increases
            if (!endflag){
                for (int i = 0; i < tempbufcount-winsize; i++){
                    tempbuf[i] = tempbuf[i+winsize];
                }
                tempbufcount -= winsize;
            }else{
                tempbufcount = 0;
            }
            if (windowsize < thres) windowsize*= 2;
            else windowsize++;
        }
        delete windowpkt;
        //windowpkt = new SEGMENT[windowsize];
    }
}