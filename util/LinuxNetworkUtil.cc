//
// Created by chijinxin on 19-5-10.
//

#include "LinuxNetworkUtil.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

bool LinuxNetworkUtil::GetIP(std::string net_name, std::string &strIP)
{
    int sock_get_ip;
    char ipaddr[50];

    struct   sockaddr_in *sin;
    struct   ifreq ifr_ip;

    if ((sock_get_ip=socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        //printf("socket create failse...GetLocalIp!/n");
        return false;
    }

    memset(&ifr_ip, 0, sizeof(ifr_ip));

    strncpy(ifr_ip.ifr_name, net_name.c_str(), sizeof(ifr_ip.ifr_name) - 1);

    if( ioctl( sock_get_ip, SIOCGIFADDR, &ifr_ip) < 0 )
    {
        return false;
    }
    sin = (struct sockaddr_in *)&ifr_ip.ifr_addr;
    strcpy(ipaddr,inet_ntoa(sin->sin_addr));

    //printf("local ip:%s /n",ipaddr);
    close( sock_get_ip );
    strIP = ipaddr;
    return true;
}

bool LinuxNetworkUtil::GetMAC(std::string net_name, std::string &strMAC)
{

    int sock_mac;

    struct ifreq ifr_mac;
    char mac_addr[30];

    sock_mac = socket( AF_INET, SOCK_STREAM, 0 );
    if( sock_mac == -1)
    {
        //perror("create socket falise...mac/n");
        return false;
    }

    memset(&ifr_mac,0,sizeof(ifr_mac));

    strncpy(ifr_mac.ifr_name, net_name.c_str(), sizeof(ifr_mac.ifr_name)-1);

    if( (ioctl( sock_mac, SIOCGIFHWADDR, &ifr_mac)) < 0)
    {
        //printf("mac ioctl error/n");
        return false;
    }

    sprintf(mac_addr,"%02x%02x%02x%02x%02x%02x",
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[3],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr_mac.ifr_hwaddr.sa_data[5]);

    //printf("local mac:%s /n",mac_addr);

    close( sock_mac );
    strMAC = mac_addr;
    return true;
}
