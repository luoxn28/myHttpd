#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "python2.7/Python.h"

void err_sys(char *s);
void accept_request(int client, PyObject *pFunc);
int get_line(int sockfd, char *buf, int size);
void send_file(int client, const char *filename);
void cat(int client, FILE *fp);
void headers(int client);
void not_found(int client);
void unimplemented(int client);
int startup(unsigned int *port);


int main(void)
{
	int listenfd = -1;
	int connfd = -1;
	unsigned port = 54321;
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	
	if((listenfd = startup(&port)) < 0)
		err_sys("startup error");
	printf("httpd runing on port: %d\n", port);

	/* The  python init */
	Py_Initialize();
	if(!Py_IsInitialized())
		err_sys("python init error");
	PyRun_SimpleString("import sys");
	PyRun_SimpleString("sys.path.append('./')");

	PyObject * pModule = PyImport_ImportModule("down"); /* don't carry .py */
	if(!pModule)
		err_sys("pModule error");
	PyObject * pFunc = PyObject_GetAttrString(pModule, "page");
	if(!pFunc)
		err_sys("pFunc error");
	/* End of the python init */

	for( ; ; )
	{
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		if(connfd < 0)
			err_sys("accept error");
		accept_request(connfd, pFunc);
 
		close(connfd);
	}

	/* free the source of python use */
	Py_Finalize();
	close(listenfd);
	printf("bye\n");

	return 0;
}

void err_sys(char *s)
{
	perror(s);
	exit(1);
}

void accept_request(int client, PyObject *pFunc)
{
	char buf[1024];
	char method[255];
	char url[255];
	char web[255];
	size_t i, j;

	get_line(client, buf, sizeof(buf));
	i = j = 0;
	while(!isspace(buf[i]) && (j < sizeof(method) - 1))
		method[j++] = buf[i++];
	method[j] = '\0';

	if(strcasecmp(method, "GET"))
	{
		unimplemented(client);
		return;
	}

	j = 0;
	while(isspace(buf[i]) && (i < sizeof(buf)))
		i++;
	while(!isspace(buf[i]) && (j < sizeof(url) -1) && (i < sizeof(buf)))
		url[j++] = buf[i++];
	url[j] = '\0';

	j = 1;
	while((url[j] != '\0') && (j < sizeof(web)))
	{
		web[j-1]  =url[j];
		j++;
	}
	web[j-1] = '\0';

	/* use python download the web page */
	FILE *fp = NULL;
	PyObject *pParam = Py_BuildValue("(s)", web);
	if(!strncmp(web, "www.", 4))
	{
		printf("%s\n", web);
		PyEval_CallObject(pFunc, pParam);

		fp = fopen("htdocs/index.html", "r");
		if(fp == NULL)
			err_sys("fopen index.html error");
		else
		{
			headers(client);
			cat(client, fp);
		}
		fclose(fp);
		printf("    send the web ok\n");
	}

}

/*
* Get a line from a sockfd, whether the line ends in a newline, carriage
* return, ro a CRLF combination. Terminates the string read with a null
* character.
* Paramters: the sockfd
*			 the buffer to save the data in
*			 the size of the buffer
* Returns: the numbers of bytes stored
*/
int get_line(int sockfd, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n = 0;

	while((i < size -1) && (c != '\n'))
	{
		n = recv(sockfd, &c, 1, 0);
		if(n > 0)
		{
			if(n == '\r')
			{
				n = recv(sockfd, &c, 1, MSG_PEEK);
				if((n > 0) && (c == '\n'))
					recv(sockfd, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else
		{
			c = '\n';
		}
	}
	buf[i] = '\0';

	return (i);
}

/*
* Send a file to client
* Paramters: client socket
*			 FILE *fp
*/
void send_file(int client, const char *filename)
{
	FILE *fp = NULL;
	int recvlen = 1;
	char buf[1024];

	while(recvlen > 0)
		recvlen = get_line(client, buf, sizeof(buf)); /* read & discard headers */
	
	fp = fopen(filename, "r");
	if(fp == NULL)
		err_sys("fopen error");
	else
	{
		headers(client);
		cat(client, fp);
	}
	fclose(fp);
}

/*
* put the file context to socket.
* Paramters: the client socket
*			 FILE pointer fp
*/
void cat(int client, FILE *fp)
{
	char buf[1024];

	fgets(buf, sizeof(buf), fp);
	while(!feof(fp))
	{
		send(client, buf, strlen(buf), 0);
		fgets(buf, sizeof(buf), fp);
	}
}
/*
* Return ht informational HTTP headers
* Paramter: hte socket to print the headers
*/
void headers(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 200 ok\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Server: HDU_httpd/0.1.0\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}

/*
* give a client a 404 not found message
*/
void not_found(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Server: HDU_httpd/0.1.0\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fullfill.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified is unavailable or notexitent\r\n");
	send(client, buf, strlen(buf), 0);
	printf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/*
* Inform the client that the request web method has not been implemented
* Paramter: the client socket
*/
void unimplemented(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Server: HDU_httpd/0.1.0\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/*
* This function starts the process of listening for web connections
* on a specified port. If the port is 0, then dynamically allocate a
* port and modify the original port to reflect the actual port
* Parameters: pointer to variable containing the port to connect on
* Returns: the sockfd
*/
int startup(unsigned int *port)
{
	int sockfd = 0;
	struct sockaddr_in servaddr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err_sys("sockfd error");
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(*port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		err_sys("bind error");
	
	if(listen(sockfd, 5) < 0)
		err_sys("listen error");
	
	if(*port == 0)
	{
		socklen_t socklen = sizeof(servaddr);
		if(getsockname(sockfd, (struct sockaddr *)&servaddr, &socklen) < 0)
			err_sys("getsockname error");
		*port = ntohs(servaddr.sin_port);
	}

	return sockfd;
}
































