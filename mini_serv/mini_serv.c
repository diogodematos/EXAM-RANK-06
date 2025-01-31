#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

int count = 0, maxfd = 0, ids[65536];
char *messages[65536];
fd_set readfd, writefd, allfd;
char buff_send[42], buff_rec[1001];
int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;
	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}
char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;
	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}
void fatal_error()
{
    write(2, "Fatal error\n", 12);
    exit(1);
}
int create_socket()
{
    maxfd = socket(AF_INET, SOCK_STREAM, 0);
    if (maxfd < 0)
        fatal_error();
    FD_SET(maxfd, &allfd);
    return (maxfd);
}
void notify(int auth, char *str)
{
    for(int i = 0; i <= maxfd; i++)
        if(i!= auth && FD_ISSET(i, &writefd))
            send(i, str, strlen(str), 0);
}
void register_client(int fd)
{
    if (maxfd < fd)
        maxfd = fd;
    ids[fd] = count++;
    messages[fd] = NULL;
    FD_SET(fd, &allfd);
    sprintf(buff_send, "server: client %d just arrived\n", ids[fd]);
    notify(fd, buff_send);
}
void remove_client(int fd)
{
    sprintf(buff_send, "server: client %d just left\n", ids[fd]);
    notify(fd, buff_send);
    free(messages[fd]);
    FD_CLR(fd, &allfd);
    close(fd);
}
void send_message(int fd)
{
    char *msg;
    while(extract_message(&(messages[fd]), &msg))
    {
        sprintf(buff_send, "client %d: ", ids[fd]);
        notify(fd, buff_send);
        notify(fd, msg);
        free(msg);
    }
}

int main(int argc, char **argv) 
{
    if (argc != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }
    FD_ZERO(&allfd);
	int sockfd = create_socket();
	struct sockaddr_in sa; 
	bzero(&sa, sizeof(sa)); 
	sa.sin_family = AF_INET; 
	sa.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	sa.sin_port = htons(atoi(argv[1])); 
	if (bind(sockfd, (const struct sockaddr *)&sa, sizeof(sa)))
		fatal_error();
	if (listen(sockfd, SOMAXCONN))
		fatal_error();
	while(1)
    {
        readfd = writefd = allfd;
        if (select(maxfd + 1, &readfd, &writefd, NULL, NULL) < 0)
            fatal_error();
        for(int fd = 0; fd <= maxfd; fd++)
        {
            if(!FD_ISSET(fd, &readfd))
                continue;
            if (fd == sockfd)
            {
                socklen_t sockl = sizeof(sa);
                int client_fd = accept(sockfd, (struct sockaddr *)&sa, &sockl);
                if(client_fd >= 0)
                {
                    register_client(client_fd);
                    break;
                }
            }
            else
            {
                int bread = recv(fd, buff_rec, 1000, 0);
                if (bread <= 0)
                {
                    remove_client(fd);
                    break;
                }
                buff_rec[bread] = 0;
                messages[fd] = str_join(messages[fd], buff_rec);
                send_message(fd);
            }
        }
    }
    return(0);
}
