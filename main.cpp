#include <stdio.h> // for perror
#include <stdlib.h>
#include <errno.h>
#include <malloc.h>
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <sys/types.h>
#include <resolv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netdb.h>   


#include <list>
#include <thread>
#include <mutex>

using namespace std;
list<pair <char*, int>> connected_list;

mutex m;

void find_host(const uint8_t* data, const char* find, char** host, int* host_len)
{
    int i = 0;
	int len = strlen(find);
	while(true){
		if(!memcmp((uint8_t*)data + i, find, len)){
			*host = (char*)((uint8_t*)data + i + strlen(find) + 2);
			int j = 0;
			while(memcmp((uint8_t*)data + i + strlen(find) + 2 + j, "\x0d\x0a", 2)) { j++; }
			*host_len = j;
			return;
		}
		else if(!memcmp((uint8_t*)data + i, "\x0d\x0a\x0d\x0a", 4)){

			return;
		}
		else{
			i++;
		}
	}
}

void http_response(int fd, int web_fd)
{
    while(true) {
		const static int BUFSIZE = 1024;
		char buf[BUFSIZE];
		ssize_t received = recv(web_fd, buf, BUFSIZE - 1, 0);
	        printf("server msg: ");
	        if (received == 0 || received == -1) {
	                perror("recv failed");
	                break;
	        }
		ssize_t sent = send(fd, buf, strlen(buf), 0);
	}
}

void http_request(int fd)
{
    while(true)
    {
        const static int BUFSIZE = 1024;
        char buf[BUFSIZE];
        ssize_t received = recv(fd, buf, BUFSIZE -1, 0);
        if (received == 0 || received == -1) {
			perror("recv failed");
			break;
		}
        buf[received] = '\0';
        char* host;
        int host_len;
        find_host((uint8_t*)buf, "Host",  &host, &host_len);
        int tf=0;
        host[host_len]='\0';
        m.lock();
        for(list<pair<char*, int>>::iterator it = connected_list.begin(); it!=connected_list.end();it++)
        {
            if(!strncmp(host, it->first, strlen(host))) {
                ssize_t sent = send(it->second, buf, strlen(buf), 0);
                if (sent == 0 ){
                    perror("send failed");
                    }
                    tf=1;
                    thread t(http_response, fd, it->second);
                    t.detach();
                    break;
            }
        }
        m.unlock();
        if(tf) continue;
        else {
			addrinfo *addr_info;
			addrinfo hint = {0};
			hint.ai_flags = AI_NUMERICHOST;
			hint.ai_family = AF_INET;
			hint.ai_socktype = SOCK_STREAM;
			hint.ai_protocol = IPPROTO_TCP;
 			if(getaddrinfo(host, "80", &hint, &addr_info) != 0) {
				perror("error during getting ip addr info");
				freeaddrinfo(addr_info);
				continue;
			}
			uint32_t ip_addr;
			memcpy(&ip_addr, &(((struct sockaddr_in *)(addr_info->ai_addr))->sin_addr), sizeof(ip_addr));
			int web_fd = socket(80, ip_addr, 1);
			freeaddrinfo(addr_info);
			m.lock();
			pair<char*, int> p(host, web_fd);
			connected_list.push_back(p);
			m.unlock();
			ssize_t sent = send(web_fd, buf, strlen(buf), 0);
			m.unlock();
			if (sent == 0) {
				perror("send failed");
				break;
			}
			thread t(http_response, fd, web_fd);
			t.detach();
		}
    }
}




int main(int argc, char* argv[])
{
    if(argc !=2)
    {
        return -1;
    }
    
    int port=0;
    port=atoi(argv[1]);
    int sockfd = socket(80, INADDR_ANY, 2);
    if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}
    int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

    while (true) {
		struct sockaddr_in addr;
		socklen_t clientlen = sizeof(sockaddr);
		int childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (childfd < 0) {
			perror("ERROR on accept");
			break;
		}

		thread t(http_request, childfd);
		t.detach();
	}
	
	close(sockfd);
	return 0;
}