/* pts_mux is TODO

   Author: michael tetemke mehari
   Email:  michael.mehari@intec.ugent.be
*/

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <pty.h>
#include <errno.h>
#include <signal.h>

#ifdef DEBUG
#include <sys/time.h>
#endif

#include "uthash.h"
#include "base64.h"
#include "jsmn.h"

// MACRO DEFINITION

#define BOLD		"\033[1m\033[30m"
#define RESET		"\033[0m"

struct RECORD_t
{
	int		ptsFd;			// pts File Descriptor
	char		ptsFile[36];		// pts File path
	int		clientFd;		// temporary client File Descriptor
	int		clientRequest;		// Flag to tell for active client request
	char		recvd_msg[1024];	// received message
	int		recvd_len;		// received message length
	UT_hash_handle	hh;			// hash function handler
};

struct serial_payload_t
{
	uint8_t decoded_len;
	uint8_t encoded_len;
	uint8_t	padding[8];
	char	data_base64[1024];
};

struct msg_queue_t
{
	int			ptsFd;		// pts File Descriptor
	int			clientFd;	// client File Descriptor
	int			size;		// payload size
	struct serial_payload_t payload;	// payload content
	struct msg_queue_t	*next;		// Next message pointer
};

#define CLIENT_TO_PTS 0
#define PTS_TO_CLIENT 1

struct pollfd *pFds = NULL;
struct timeval systime;
int masterFd, numFds = 0;
#ifdef DEBUG
char *logFile = NULL;
FILE *debugFd;
#endif

struct RECORD_t *PTS_ptr, *tmp_ptr, *hash_ptr = NULL;
struct msg_queue_t *msg_queue_start_ptr = NULL, *msg_queue_curr_ptr, *msg_queue_prev_ptr;

// Send message to a client
int send_msg(void *msg, int fd, int size)
{
	int send_len = 0;
	while(send_len < size)
	{
		int r;
		r = write(fd, msg + send_len, size - send_len);
		if(r < 0)
		{
			if(errno == EINTR || errno == EAGAIN)
			{
				usleep(1000);
				continue;
			}
			else
			{
				return -1;
			}
		}
		else if(r == 0)
		{
			break;
		}

		send_len += r;
		usleep(1000);
	}

	return send_len;
}

// Set File Descriptor to no delay mode
int set_fd_no_delay_mode(int fd)
{
	int flags, newflags;

	/* Set the file descriptor to no-delay / non-blocking mode. */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		fprintf(stderr, "fcntl F_GETFL: %s\n", strerror(errno));
		close( fd );
		return -1;
	}

	newflags = flags | (int) O_NDELAY;
	if (newflags != flags)
	{
		if ( fcntl(fd, F_SETFL, flags | O_NDELAY) < 0)
		{
			fprintf(stderr, "fcntl O_NDELAY: %s\n", strerror(errno));
			close(fd);
			return -1;
		}
	}

	return 0;
}

// Add File Descriptor into a polling list
int add_poll_fd(int fd)
{
	struct pollfd *pFds_temp;

	numFds++;

	pFds_temp = realloc(pFds, numFds*sizeof(struct pollfd));
	if(pFds_temp == NULL)
	{
		fprintf(stderr, "Increasing the size of polling list failed\n");
		return -1;
	}
	pFds = pFds_temp;
	pFds[numFds-1].fd = fd;
	pFds[numFds-1].events = POLLIN;
	return 0;
}

// Delete File Descriptor from a polling list
int del_poll_fd(int fd)
{
	struct pollfd *pFds_temp;
	int i;

	// Search the index of the File Descriptor to be deleted
	for(i = 0; i < numFds; i++)
	{
		if(pFds[i].fd == fd)
			break;
	}

	numFds--;

	// If the File Descriptor is not at the end, replace it by the last element
	if(i != numFds)
	{
		if(memmove(&pFds[i], &pFds[numFds], sizeof(struct pollfd)) == NULL)
		{
			fprintf(stderr, "moving pFds memory failed\n");
			return -1;
		}
	}

	// Reallocate polling list memory
	pFds_temp = realloc(pFds, numFds*sizeof(struct pollfd));
	if(pFds_temp == NULL)
	{
		fprintf(stderr, "Decreasing the size of polling list failed\n");
		return -1;
	}
	pFds = pFds_temp;
	return 0;
}

// Print serial buffer
void print_serial_buf(char *buffer, int msg_dir)
{
	uint8_t opcode = *((uint8_t *)buffer);
	uint8_t num_param = *((uint8_t *)(buffer + 1));
	uint32_t seq_nr = *((uint8_t *)(buffer + 2));

	fprintf(debugFd, "opcode=%u num_param=%u seq_nr=%u|", opcode, num_param, seq_nr);

	int j, k, offset = 6;
	for(j = 0; j < num_param; j++)
	{
		uint16_t uid = *((uint16_t *)(buffer + offset));
		uint8_t type = *((uint8_t *)(buffer + offset + 2));
		uint8_t len = *((uint8_t *)(buffer + offset + 3));

		fprintf(debugFd, "param=%u uid=%u type=%u len=%u", j+1, uid, type, len);

		// PARAM GET and client -> pts message direction
		if(opcode == 0 && msg_dir == CLIENT_TO_PTS)
		{
			offset += 4;
		}
		// PARAM GET and pts -> client message direction or PARAM SET
		else if((opcode == 0 && msg_dir == PTS_TO_CLIENT) || (opcode == 1))
		{
			char *value = (char *) malloc(sizeof(char) * len);
			memcpy(value, buffer + offset + 4, len);

			fprintf(debugFd, " value=");
			for(k = 0; k < len; k++)
				fprintf(debugFd, "%.2x", (value[k] & 0xFF));

			offset += 4 + len;
		}
		// PARAM ERROR
		else if(opcode == 127)
		{
			char *value = (char *) malloc(sizeof(char) * (len + 1));
			memcpy(value, buffer + offset + 4, len);
			value[len] = '\0';

			fprintf(debugFd, " value='%s'", value);

			offset += 4 + len;
		}
		fprintf(debugFd, "|");
	}
}

// Decode base64 encoded string
char* base64_str_decode(char *enc_str, int *num_dec_bytes)
{
	int num_enc_bytes = 0;
	int enc_str_len = strlen(enc_str);
	char *dec_str;

	dec_str = (char *) malloc( sizeof(char) * (enc_str_len + 1) );
	*num_dec_bytes = 0;
	while(num_enc_bytes < enc_str_len)
	{
		int dec_len;
		base64_decode((unsigned char*) &enc_str[num_enc_bytes], &dec_len, (unsigned char*) &dec_str[*num_dec_bytes]);
		num_enc_bytes += 4;
		*num_dec_bytes += dec_len;
	}
	dec_str[*num_dec_bytes + 1] = '\0';

	return dec_str;
}

// Retrieve JSON value
int retr_json_val(char* json, jsmntok_t *tok, int r, char *key, jsmntype_t *jsmntype, char *json_val)
{
	int i, tok_len;

	/* Loop over the received message searching for the key */
	for (i = 1; i < r; i++)
	{
		tok_len = tok[i].end - tok[i].start;
		if( (tok[i].type == JSMN_STRING) && (strlen(key) == tok_len) && (strncmp(json + tok[i].start, key, tok_len) == 0) )
		{
			tok_len = tok[i+1].end - tok[i+1].start;

			*jsmntype = tok[i+1].type;
			strncpy(json_val, json + tok[i+1].start, tok_len);
			json_val[tok_len] = '\0';

			break;
		}
	}

	// If pts file path is not received
	if(i == r)
	{
		return -1;
	}
	else
	{
		return tok_len;
	}
}

// Create a pts device of the given path
int create_pts(char *device, speed_t speed)
{
	int fd, hs;
	struct termios options;

	// Open a pts device in read/write mode, non controlling port, non blocking io and synchronized file operation
	fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY | O_SYNC);
	if(fd < 0)
	{
		perror("open");
		return -1;
	}

	// Get file descriptor attributes
	if(tcgetattr(fd, &options) < 0)
	{
		perror("tcgetattr");
		return -1;
	}

	// configure in/out serial baudrate
	cfsetispeed(&options, speed);
	cfsetospeed(&options, speed);

	// Enable the receiver and set local mode
	options.c_cflag |= (CLOCAL | CREAD);
	// Mask the character size bits and turn off (odd) parity
	options.c_cflag &= ~(CSIZE | PARENB | PARODD);
	// Select 8 data bits
	options.c_cflag |= CS8;

	// Raw input
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	// Raw output
	options.c_oflag &= ~OPOST;

	// Set file descriptor attributes
	if(tcsetattr(fd, TCSANOW, &options) < 0)
	{
		perror("could not set options");
		return -1;
	}
	tcflush(fd,TCIOFLUSH);

	// Get the status of modem bits.
	ioctl(fd, TIOCMGET, &hs);

	// Disable RTS
	hs &= ~ TIOCM_RTS;
	ioctl(fd,TIOCMSET,&hs);

	// Disable DTR
	hs &= ~ TIOCM_DTR;
	ioctl(fd,TIOCMSET,&hs);

	return fd;
}

/* Function to explode string to array */
char** explode(char delimiter, char* str)
{
	char *p = strtok (str, &delimiter);
	char ** res  = NULL;
	int n_spaces = 0;

	/* split string and append tokens to 'res' */
	while (p)
	{
		res = realloc (res, sizeof (char*) * ++n_spaces);
		if (res == NULL)
			exit (-1); /* memory allocation failed */

		res[n_spaces-1] = p;
		p = strtok (NULL, &delimiter);
	}

	/* realloc one extra element for the last NULL */
	res = realloc (res, sizeof (char*) * (n_spaces+1));
	res[n_spaces] = 0;

	return res;
}

/*
 * The memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
void *memmem(const void *haystack, size_t hlen, const void *needle, size_t nlen)
{
	int needle_first;
	const void *p = haystack;
	size_t plen = hlen;

	if(haystack == NULL || needle == NULL || hlen == 0 || nlen == 0 || hlen < nlen)
		return NULL;

	needle_first = *(unsigned char *)needle;

	while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
	{
		if (memcmp(p, needle, nlen) == 0)
			return (void *)p;

		p++;
		plen = hlen - (p - haystack);
	}

	return NULL;
}

/*
 * memrmem.c -- reverse search for memory block inside another memory block
 */
void *memrmem(const void *haystack, size_t hlen, const void *needle, size_t nlen)
{
	const char *p;

	if(haystack == NULL || needle == NULL || hlen == 0 || nlen == 0 || hlen < nlen)
		return NULL;

	for (p = haystack + (hlen - nlen); hlen >= nlen; --p, --hlen)
	{
		if (memcmp(p, needle, nlen) == 0)
			return (void *)p;
	}

	return NULL;
}

// cleanup resources upon SIGINT and SIGTERM
static void cleanup(int signo)
{

	// Delete and close all polling File Descriptors
	int i, pFd;
	for(i=0; i<numFds-1; i++)
	{
		pFd = pFds[i].fd;
		if(del_poll_fd(pFd) != -1)
		{
			#ifdef DEBUG
/*			gettimeofday(&systime, NULL);
			fprintf(debugFd, "%lu.%-6lu Client@%u is deleted from the polling list. numFds=%d\n", systime.tv_sec, systime.tv_usec, pFd, numFds);
			fflush(debugFd);*/
			#endif
		}
		close(pFd);
	}

	// Delete and close master File Descriptor
	free(pFds); numFds--;
	close(masterFd);
	#ifdef DEBUG
/*	gettimeofday(&systime, NULL);
	fprintf(debugFd, "%lu.%-6lu Master@%u is deleted from the polling list. numFds=%d\n", systime.tv_sec, systime.tv_usec, masterFd, numFds);
	fflush(debugFd);*/

	if(logFile != NULL)
		fclose(debugFd);
	#endif

	// Exit the program
	exit(1);
}

int main(int argc , char *argv[])
{
	uint16_t port = 6200;
	speed_t speed = B230400;
	char *group_pts = NULL, **pts_array;
	uint8_t pts_count = 0;

	int option = -1;
	#ifdef DEBUG
	logFile = NULL;
	while ((option = getopt (argc, argv, "hp:b:l:g:")) != -1)
	#else
	while ((option = getopt (argc, argv, "hp:b:g:")) != -1)
	#endif
	{
		switch (option)
		{
		case 'h':
			fprintf(stderr, "Description\n");
			fprintf(stderr, "-----------\n");
			fprintf(stderr, BOLD "pts_mux" RESET " is multiplexer program used to send client requests to pseudo terminal (pts) devices.\n");
			fprintf(stderr, "A request message is composed of pts device names and a request string structured in a JSON format\n");
			fprintf(stderr, "(e.g. {ptsFile: '/dev/pts/23', request: '*request*'}). But first, the multiplexer program has to be started as a background service.\n");
			fprintf(stderr, "Argument list\n");
			fprintf(stderr, "-------------\n");
			fprintf(stderr, "-h\t\t\thelp menu\n");
			fprintf(stderr, "-p port\t\t\tserver port number [eg. -p 6200]\n");
			fprintf(stderr, "-b baudrate\t\tbaudrate to connect to serial device (19200, 38400, 57600, 115200, 230400) [eg. -b 230400]\n");
			#ifdef DEBUG
			fprintf(stderr, "-l logFile\t\tSave stderr messages to logFile [eg. -l /tmp/pts_mux.log]\n");
			#endif
			fprintf(stderr, "-g group_pts\t\tComma separated group of pts file paths [eg. -g /dev/pts/15,/dev/pts/17]\n\n");
			return 1;
		case 'p':
			port = atoi(optarg);
			break;
		case 'b':
			if(strcmp(optarg, "19200") == 0)
				speed = B19200;
			else if(strcmp(optarg, "38400") == 0)
				speed = B38400;
			else if(strcmp(optarg, "57600") == 0)
				speed = B57600;
			else if(strcmp(optarg, "115200") == 0)
				speed = B115200;
			else if (strcmp(optarg, "230400") == 0)
				speed = B230400;
			else
			{
				fprintf(stderr, "unsupported speed: %s\n", optarg);
				return 1;
			}
			break;
		#ifdef DEBUG
		case 'l':
			logFile = strdup(optarg);
			break;
		#endif
		case 'g':
			group_pts = strdup(optarg);
			break;
		default:
			fprintf(stderr, "pts_mux: missing operand. Type './pts_mux -h' for more information.\n");
			return 1;
		}
	}

	if(group_pts == NULL)
	{
		fprintf(stderr,"group_pts is not specified. Type './pts_mux -h' for more information\n");
		return 1;
	}

	int i, ptsFd, clientFd, size, r;
	struct sockaddr_in serverAddr, clientAddr;

	char recvd_msg[1024];

	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */
	jsmntype_t jsmntype;

	char ptsFile[36];
	struct serial_payload_t payload;

	// handle SIGTERM, SIGINT and SIGCHLD signals
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);
	// Ignore broken pipe signal
	signal(SIGPIPE, SIG_IGN);

	#ifdef DEBUG
	debugFd = (logFile != NULL) ? fopen(logFile, "w") : stderr;
	if(debugFd == NULL)
	{
		fprintf(stderr, "Can't open the logFile %s for writing!\n", logFile);
		return 1;
	}
	#endif

	// ------------- create a master socket -------------- //
	// Create streaming socket
	masterFd = socket(PF_INET, SOCK_STREAM, 0);
	if(masterFd < 0)
	{
		fprintf(stderr, "SOCK_STREAM: %s\n", strerror(errno));
		return 1;
	}

	fcntl(masterFd, F_SETFD, 1);

	// Reuse Address
	int reuse = 1;
	if(setsockopt(masterFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		fprintf(stderr, "SO_REUSEADDR: %s\n", strerror(errno));
		return 1;
	}

	// Prepare the sockaddr_in structure
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind
	if(bind(masterFd,(struct sockaddr *)&serverAddr , sizeof(struct sockaddr_in)) < 0)
	{
		fprintf(stderr, "BIND: %s\n", strerror(errno));
		return 1;
	}

	// Set master File Descriptor to no delay mode
	set_fd_no_delay_mode(masterFd);

	// Listen maximum socket connections
	listen(masterFd, SOMAXCONN);

	// Watch the master File Descriptor
	if(add_poll_fd(masterFd) != -1)
	{
		#ifdef DEBUG
/*		gettimeofday(&systime, NULL);
		fprintf(debugFd, "%lu.%-6lu Master@%u is added to polling list. numFds=%d\n", systime.tv_sec, systime.tv_usec, masterFd, numFds);
		fflush(debugFd);*/
		#endif
	}

	// ------------- create pts sockets -------------- //
	// Explode the group_pts string into an array
	pts_array = explode(',', group_pts);
	do{
		// Create a new pts file descriptor of the given path
		ptsFd = create_pts(pts_array[pts_count], speed);

		// Watch the pts File Descriptor
		if(add_poll_fd(ptsFd) != -1)
		{
			#ifdef DEBUG
/*			gettimeofday(&systime, NULL);
			fprintf(debugFd, "%lu.%-6lu pts@%s is added to polling list. fd=%d, numFds=%d\n", systime.tv_sec, systime.tv_usec, pts_array[pts_count], ptsFd, numFds);
			fflush(debugFd);*/
			#endif
		}

		// pts HASH record
		PTS_ptr = (struct RECORD_t*)malloc(sizeof(struct RECORD_t));
		if(PTS_ptr == NULL)
		{
			fprintf(stderr, "Unable to create record!\n");
			return 1;
		}
		PTS_ptr->ptsFd = ptsFd;
		strcpy(PTS_ptr->ptsFile, pts_array[pts_count]);
		PTS_ptr->clientFd = -1;
		PTS_ptr->clientRequest = 0;
		memset(PTS_ptr->recvd_msg, '\0', sizeof(PTS_ptr->recvd_msg));
		PTS_ptr->recvd_len = 0;
		// Add the new record to the hash table
		HASH_ADD_INT(hash_ptr, ptsFd, PTS_ptr);

		pts_count++;
	}while (pts_array[pts_count] != NULL);

	sleep(1);
	printf("pts_mux has STARTED");
	fflush(stdout);

	// Accept connection from an incoming client
	for(;;)
	{
		// Poll all socket descriptors. These are client sockets and pts sockets
		poll(pFds, numFds, -1);

		// Look data from client and pts sockets
		for (i = 1; i < numFds; i++)
		{
			if(pFds[i].revents & POLLIN)
			{
				ptsFd = pFds[i].fd;
				HASH_FIND_INT(hash_ptr, &ptsFd, PTS_ptr);

				// Message from pts
				if(PTS_ptr != NULL)
				{
					char *padding_ptr;
					uint8_t encoded_len;

					// read message from pts session
					size = read(PTS_ptr->ptsFd, PTS_ptr->recvd_msg + PTS_ptr->recvd_len, sizeof(PTS_ptr->recvd_msg) - PTS_ptr->recvd_len);

					// Update received message length
					PTS_ptr->recvd_len += size;

					// Search for the padding pattern and calculate the encoded message length
					padding_ptr = memrmem(PTS_ptr->recvd_msg, PTS_ptr->recvd_len, "FFFFFFFF", 8);
					if(padding_ptr != NULL)
					{
						encoded_len = ((struct serial_payload_t *)(padding_ptr - (sizeof(payload.decoded_len) + sizeof(payload.encoded_len))))->encoded_len;

						// If full message is received from the node, send it to the waiting client
						if(strlen(padding_ptr) >= (encoded_len - (sizeof(payload.decoded_len) + sizeof(payload.encoded_len))) && PTS_ptr->clientFd != -1)
						{
							size = send_msg(padding_ptr + sizeof(payload.padding), PTS_ptr->clientFd, encoded_len - (sizeof(payload.decoded_len) + sizeof(payload.encoded_len) + sizeof(payload.padding)));

							#ifdef DEBUG
							gettimeofday(&systime, NULL);
							fprintf(debugFd, "%lu.%-6lu pts@%s -> client@%u: message size=%d. ", systime.tv_sec, systime.tv_usec, PTS_ptr->ptsFile, PTS_ptr->clientFd, size);
							fflush(debugFd);

							char *serial_buf;
							int num_dec_bytes;
							serial_buf = base64_str_decode(padding_ptr + 8, &num_dec_bytes);

							// print serial buffer
							print_serial_buf(serial_buf, PTS_TO_CLIENT);
							fprintf(debugFd, "\n");
							fflush(debugFd);
							#endif

							memset(PTS_ptr->recvd_msg, '\0', PTS_ptr->recvd_len);
							PTS_ptr->recvd_len = 0;
						}
					}
				}
				// Message from client
				else
				{
					clientFd = pFds[i].fd;

					// Clear the receive buffer
					memset(recvd_msg, 0, sizeof(recvd_msg));

					// Receive message from the client
					size = read(clientFd , recvd_msg, sizeof(recvd_msg));

					// Process client request
					if (size > 0)
					{
						jsmn_init(&p);
						r = jsmn_parse(&p, recvd_msg, strlen(recvd_msg), t, sizeof(t)/sizeof(t[0]));
						if(r < 0)
						{
							fprintf(stderr, "JSON: Parser failed. %s\n", recvd_msg);
							continue;
						}

						/* Assume the top-level element is an object */
						if (r < 1 || t[0].type != JSMN_OBJECT)
						{
							fprintf(stderr, "JSON: Object expected\n");
							continue;
						}

						// Retrieve pts file path from received message
						size = retr_json_val(recvd_msg, t, r, "ptsFile", &jsmntype, &(ptsFile[0]));
						if(size == -1 || jsmntype != JSMN_STRING)
						{
							fprintf(stderr, "JSON: ptsFile is either empty or not a string\n");
							continue;
						}

						// Check if a pts RECORD exists and send received message to it
						HASH_ITER(hh, hash_ptr, PTS_ptr, tmp_ptr)
						{
							if(strcmp(PTS_ptr->ptsFile, ptsFile) == 0)
							{
								// Retrieve payload from received message
								size = retr_json_val(recvd_msg, t, r, "request", &jsmntype, &(payload.data_base64[0]));
								if(size == -1 || jsmntype != JSMN_STRING)
								{
									fprintf(stderr, "JSON: request is either empty or not a string\n");
									continue;
								}

								// Append newline character before sending to the request
								payload.data_base64[size] = '\n';

								// Update the size of the serial message
								size = sizeof(payload.decoded_len) + sizeof(payload.encoded_len) + sizeof(payload.padding) + size + 1;

								payload.decoded_len = 0;
								payload.encoded_len = size;
								memcpy(payload.padding, "FFFFFFFF", sizeof(payload.padding));

								// Make sure pts device is not serving a client request
								if(PTS_ptr->clientFd == -1)
								{
									// Assign client file descriptor
									PTS_ptr->clientFd = clientFd;

									// Send message to pts device
									size = send_msg(&payload, PTS_ptr->ptsFd, size);

									#ifdef DEBUG
									gettimeofday(&systime, NULL);
									fprintf(debugFd, "%lu.%-6lu client@%u -> pts@%s: message size=%d. ", systime.tv_sec, systime.tv_usec, clientFd, ptsFile, size);
									fflush(debugFd);

									char *serial_buf;
									int num_dec_bytes;
									serial_buf = base64_str_decode(payload.data_base64, &num_dec_bytes);

									// print serial buffer
									print_serial_buf(serial_buf, CLIENT_TO_PTS);
									fprintf(debugFd, "\n");
									fflush(debugFd);
									#endif
								}
								// Else put the new request into a message queue
								else
								{
									// First message in the queue
									if(msg_queue_start_ptr == NULL)
									{
										// Create message buffer
										msg_queue_start_ptr = (struct msg_queue_t *) malloc( sizeof(struct msg_queue_t) );
										if(msg_queue_start_ptr == NULL)
										{
											perror("malloc");
											break;
										}

										// Insert the message into the queue
										msg_queue_start_ptr->ptsFd = PTS_ptr->ptsFd;
										msg_queue_start_ptr->clientFd = clientFd;
										msg_queue_start_ptr->size = size;
										memcpy(&(msg_queue_start_ptr->payload), &payload, size);
										msg_queue_start_ptr->next = NULL;
									}
									// Not the first message in the queue
									else
									{
										// Locate the last message of the queue
										msg_queue_curr_ptr = msg_queue_start_ptr;
										while(msg_queue_curr_ptr->next != NULL)
											msg_queue_curr_ptr = msg_queue_curr_ptr->next;

										// Create message buffer
										msg_queue_curr_ptr->next = (struct msg_queue_t *) malloc( sizeof(struct msg_queue_t) );
										if(msg_queue_curr_ptr->next == NULL)
										{
											perror("malloc");
											break;
										}

										// Insert the message into the queue
										msg_queue_curr_ptr = msg_queue_curr_ptr->next;
										msg_queue_curr_ptr->ptsFd = PTS_ptr->ptsFd;
										msg_queue_curr_ptr->clientFd = clientFd;
										msg_queue_curr_ptr->size = size;
										memcpy(&(msg_queue_curr_ptr->payload), &payload, size);
										msg_queue_curr_ptr->next = NULL;
									}

									#ifdef DEBUG
									gettimeofday(&systime, NULL);
									fprintf(debugFd, "%lu.%-6lu client@%u -> msg_queue => pts@%s: message size=%d. ", systime.tv_sec, systime.tv_usec, clientFd, ptsFile, size);
									fflush(debugFd);

									/*char *serial_buf;
									int num_dec_bytes;
									serial_buf = base64_str_decode(payload.data_base64, &num_dec_bytes);

									// print serial buffer
									print_serial_buf(serial_buf, CLIENT_TO_PTS);*/
									fprintf(debugFd, "\n");
									fflush(debugFd);

									// Print message list
									/*msg_queue_curr_ptr = msg_queue_start_ptr;
									fprintf(debugFd, "%lu.%-6lu msg_ptr -> ", systime.tv_sec, systime.tv_usec);
									while(msg_queue_curr_ptr != NULL)
									{
										fprintf(debugFd, "client@%u -> ", msg_queue_curr_ptr->clientFd);
										msg_queue_curr_ptr = msg_queue_curr_ptr->next;
									}
									fprintf(debugFd, "NULL\n");
									fflush(debugFd);*/
									#endif
								}

								break;
							}
						}
						// If there was no pts device found in the RECORD
						if(PTS_ptr == NULL)
						{
							fprintf(stderr, "PTS: %s device does not exist\n", ptsFile);
						}
					}
					// Client exits connection
					else if (size == 0)
					{
						// Delete the File Descriptor from the polling list
						if(del_poll_fd(clientFd) != 1)
						{
							#ifdef DEBUG
/*							gettimeofday(&systime, NULL);
							fprintf(debugFd, "%lu.%-6lu Client@%u is deleted from the polling list. numFds=%d\n", systime.tv_sec, systime.tv_usec, clientFd, numFds);
							fflush(debugFd);*/
							#endif
						}

						// Close client file descriptor
						close(clientFd);

						// Detach the client from the pts device as well
						HASH_ITER(hh, hash_ptr, PTS_ptr, tmp_ptr)
						{
							if(PTS_ptr->clientFd == clientFd)
							{
								PTS_ptr->clientFd = -1;

								#ifdef DEBUG
								gettimeofday(&systime, NULL);
								fprintf(debugFd, "%lu.%-6lu Client@%u finished request from %s\n", systime.tv_sec, systime.tv_usec, clientFd, PTS_ptr->ptsFile);
								fflush(debugFd);
								#endif

								break;
							}
						}
					}
					else
					{
						perror("read");
					}
				}
			}
		}

		// Send waiting messages from the queue to pts devices
		if(msg_queue_start_ptr != NULL)
		{
			msg_queue_prev_ptr = msg_queue_start_ptr;
			msg_queue_curr_ptr = msg_queue_start_ptr;
			while(msg_queue_curr_ptr != NULL)
			{
				// Is pty device free to serve a client?
				HASH_FIND_INT(hash_ptr, &(msg_queue_curr_ptr->ptsFd), PTS_ptr);
				if(PTS_ptr->clientFd == -1)
				{
					// Assign client file descriptor
					PTS_ptr->clientFd = msg_queue_curr_ptr->clientFd;

					// Send message to pts device
					size = send_msg(&(msg_queue_curr_ptr->payload), msg_queue_curr_ptr->ptsFd, msg_queue_curr_ptr->size);

					#ifdef DEBUG
					gettimeofday(&systime, NULL);
					fprintf(debugFd, "%lu.%-6lu client@%u => msg_queue -> pts@%s: message size=%d. ", systime.tv_sec, systime.tv_usec, PTS_ptr->clientFd, PTS_ptr->ptsFile, size);
					fflush(debugFd);

					char *serial_buf;
					int num_dec_bytes;
					serial_buf = base64_str_decode(msg_queue_curr_ptr->payload.data_base64, &num_dec_bytes);

					// print serial buffer
					print_serial_buf(serial_buf, CLIENT_TO_PTS);
					fprintf(debugFd, "\n");
					fflush(debugFd);
					#endif

					// Remove the message from the queue
					// First message
					if(msg_queue_curr_ptr == msg_queue_start_ptr)
					{
						msg_queue_start_ptr = msg_queue_curr_ptr->next;
						free(msg_queue_curr_ptr);

						msg_queue_prev_ptr = msg_queue_start_ptr;
						msg_queue_curr_ptr = msg_queue_start_ptr;
					}
					else
					{
						msg_queue_prev_ptr->next = msg_queue_curr_ptr->next;
						free(msg_queue_curr_ptr);
						msg_queue_curr_ptr = msg_queue_prev_ptr->next;
					}

					#ifdef DEBUG
					// Print message list
					/*msg_queue_curr_ptr = msg_queue_start_ptr;
					fprintf(debugFd, "%lu.%-6lu msg_ptr -> ", systime.tv_sec, systime.tv_usec);
					while(msg_queue_curr_ptr != NULL)
					{
						fprintf(debugFd, "client@%u -> ", msg_queue_curr_ptr->clientFd);
						msg_queue_curr_ptr = msg_queue_curr_ptr->next;
					}
					fprintf(debugFd, "NULL\n");
					fflush(debugFd);*/
					#endif

					break;
				}
				else
				{
					msg_queue_prev_ptr = msg_queue_curr_ptr;
					msg_queue_curr_ptr = msg_queue_curr_ptr->next;
				}
			}
		}

		// Data on masterFd
		if(pFds[0].revents & POLLIN)
		{
			// Accept the connection
			socklen_t sockLen = sizeof(struct sockaddr_in);
			clientFd = accept(masterFd, (struct sockaddr *)&clientAddr, &sockLen);

			fcntl(clientFd, F_SETFD, 1);

			// Set client File Descriptor to no delay mode
			set_fd_no_delay_mode(clientFd);

			// Watch the File Descriptor
			if(add_poll_fd(clientFd) != -1)
			{
				#ifdef DEBUG
/*				gettimeofday(&systime, NULL);
				fprintf(debugFd, "%lu.%-6lu Client@%u is added to the polling list. numFds=%d\n", systime.tv_sec, systime.tv_usec, clientFd, numFds);
				fflush(debugFd);*/
				#endif
			}
		}
	}

	return 0;
}
