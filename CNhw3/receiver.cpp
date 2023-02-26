#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <zlib.h>
#include <string>
#include "opencv2/opencv.hpp"
using namespace std;
using namespace cv;
#define ERR_EXIT(msg) {fprintf(stderr,msg);exit(0);}

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn; //if 1 means width and height
	int ack;
    unsigned long checksum;
} HEADER;

typedef struct{
	HEADER header;
	char data[1000];
} SEGMENT;
//global variables
int bufsize = 256;
int bufptr = -1;
int seqnum = 0, acknum = 1;
SEGMENT bufferarr[256];
bool fin_initial = false;
void packethandler(int sockno,struct sockaddr_in receiver, struct sockaddr_in agent,bool endflag);
void recvvideo(int sockno,struct sockaddr_in receiver, struct sockaddr_in agent);
int main(int argc, const char** argv) {
    //dealing with input
    if (argc != 3) ERR_EXIT("Uncorrect argument number!\n");
    int receiver_port,agent_port;
    char agent_IP[50];
    sscanf(argv[1],"%d",&receiver_port);
    sscanf(argv[2],"%[^:]:%d",agent_IP,&agent_port);
    if (strcmp(agent_IP,"0.0.0.0") == 0 || strcmp(agent_IP,"local") == 0 || strcmp(agent_IP,"localhost") == 0){
        agent_IP[0] = '\0';
        strcpy(agent_IP,"127.0.0.1\0");
    }
    // set UDP socket
    int recv_socket = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in receiver, agent;
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(receiver_port);
    receiver.sin_addr.s_addr = htonl(INADDR_ANY);
    //receiver.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agent_port);
    agent.sin_addr.s_addr = inet_addr(agent_IP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));
    bind(recv_socket,(struct sockaddr *)&receiver,sizeof(receiver));
    //finish setting
    //print test
    //char test[1000] = "Hello!";
    // socklen_t agentsize = sizeof(agent);
    // //sendto(recv_socket,test,1000,0,(struct sockaddr *)&agent,agentsize);
    // SEGMENT temp1,temp2;
    // printf("wait for get\n");
    // recvfrom(recv_socket,&temp1,sizeof(SEGMENT),0,(struct sockaddr *)&agent,&agentsize);
    // printf("Recv get:%s\n",temp1.data);
    // //memset(thetest,0,sizeof(thetest));
    // char printtest[1000] = "Hello from your babe!";
    // strncpy(temp2.data,printtest,1000);
    // temp2.header.ack = 1;
    // printf("finish printing\n");
    // sendto(recv_socket, &temp2, 1032, 0, (struct sockaddr *)&agent,agentsize);
    // printf("finish sending\n");
    
    //receiving packets
    recvvideo(recv_socket,receiver,agent);
    return 0;
}
void recvvideo(int sockno,struct sockaddr_in receiver, struct sockaddr_in agent){
    char whbuf[1000];
    //get width
    packethandler(sockno,receiver,agent,false);
    memset(whbuf,0,1000);
    memcpy(whbuf,bufferarr[bufptr].data,1000);
    int width = atoi(whbuf);
    //get height
    packethandler(sockno,receiver,agent,false);
    memset(whbuf,0,1000);
    memcpy(whbuf,bufferarr[bufptr].data,1000);
    int height = atoi(whbuf);
    Mat client_img = Mat::zeros(height, width, CV_8UC3);
    //how to get size from server???
    if(!client_img.isContinuous()){
         client_img = client_img.clone();
    }
    while (1){
        //get img size from server???
        //if imgsize == 0, means finish???
        //get imgsize
        char imgbuf[1000];
        memset(imgbuf,0,1000);
        packethandler(sockno,receiver,agent,false);
        memcpy(imgbuf,bufferarr[bufptr].data,1000);
        int sizecount = atoi(imgbuf);
        if (sizecount == 0){
            packethandler(sockno,receiver,agent,true);
            break;
        }
        uchar *iptr = client_img.data;
        int recvcount = 0;
        while(sizecount > 0){
            int sizela;
            if (sizecount >= 1000) sizela = 1000;
            else sizela = sizecount;
            //char preready[1000];
            uchar imgbuffer[1000];
            memset(imgbuffer,0,1000);
            packethandler(sockno,receiver,agent,false);
            // memcpy(preready,bufferarr[bufptr].data,1000);
            // for (int i = 0; i < 1000; i++){
            //     imgbuffer[i] = preready[i]+128;
            // }
            memcpy(imgbuffer,bufferarr[bufptr].data,1000);
            memcpy(iptr+recvcount,imgbuffer,sizela);
            recvcount += sizela;
            sizecount -= sizela;
        }
        //get img from server???
        imshow("Video", client_img);  
        char c = (char)waitKey(1000);
        if (c==27) break;
    }
    destroyAllWindows();
}
void packethandler(int sockno,struct sockaddr_in receiver, struct sockaddr_in agent,bool endflag){
    bufptr++;
    socklen_t agentsize = sizeof(agent);
    while(1){ //break when recv real pkt
        SEGMENT temp;
        recvfrom(sockno, &temp, sizeof(temp), 0, (struct sockaddr *)&agent, &agentsize);
        bool error = false;
        unsigned long localcrc = crc32(0L, (const Bytef *)temp.data, 1000);
        //check out of order
        if (temp.header.seqNumber != acknum){
            printf("drop\tdata\t#%d\t(out of order)\n",temp.header.seqNumber);
            error = true;
        }
        //check corrupted
        else if (temp.header.checksum != crc32(0L, (const Bytef *)temp.data, 1000)){
            printf("drop\tdata\t#%d\t(corrupted)\n",temp.header.seqNumber);
            error = true;
        }
        //check buffer overflow
        else if (bufptr==256){
            printf("drop\tdata\t#%d\t(buffer overflow)\n",temp.header.seqNumber);
            error = true;
        }else{
            if (temp.header.fin == 1){
                printf("recv\tfin\n");
            }else{
                printf("recv\tdata\t#%d\n",temp.header.seqNumber);
            }
        }
        //send ack
        if (temp.header.fin == 1 && !error){
            printf("send\tfinack\n");
        }else{
            if (!error) printf("send\tack\t#%d\n",acknum);
            else printf("send\tack\t#%d\n",acknum-1);
        }
        if (!error){
            //printf("bufptr:%d\n",bufptr);
            bufferarr[bufptr] = temp;
            //memcpy(bufferarr[bufptr],temp,sizeof(temp));
        }
        SEGMENT acktemp;
        acktemp.header.length = 0;
        acktemp.header.seqNumber = seqnum;
        if (!error) acktemp.header.ackNumber = acknum;
        else acktemp.header.ackNumber = acknum-1;
        if (temp.header.fin == 1 && !error) acktemp.header.fin = 1;
        else acktemp.header.fin = 0;
        acktemp.header.syn = 0;
        acktemp.header.ack = 1;
        char ackdata[1000] = "Computer Network A+ please!\0";
        //acktemp.header.checksum = crc32(0L, (const Bytef *)ackdata, 1000);
        strncpy(acktemp.data,ackdata,1000);
        sendto(sockno, &acktemp, sizeof(SEGMENT), 0, (struct sockaddr *)&agent,agentsize);
        if ((temp.header.fin == 1 && !error) || bufptr == 256){
            printf("flush\n");
            for (int i = 0; i < 256; i++){
                memset(&bufferarr[i],0,sizeof(SEGMENT));
            }
            bufptr = 0;
            if (temp.header.fin == 1) break;
        } 
        if (!error) {acknum++;break;}
    }
}