#include <stdio.h> /* Standard input/output definitions */
#include <stdlib.h> /* Standard General Utilities Library */
#include <fcntl.h> /* File Control Definitions */
#include <unistd.h> /* UNIX Standard Definitions */
#include <errno.h> /* ERROR Number Definitions */
#include <string.h> /* String function definitions */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <time.h> /* Date and time manipulation */
#include <json-c/json.h> /* Parser-JSON */
#include <xbee.h> /* Digi's XBee wireless modules definitions */
#define READ_TIME 10 // Read time in sec
#define BUFFER_SIZE 256 // Buffer size

// Declare functions
int open_port();
struct xbee * configure_xbee(struct xbee *xbee, xbee_err ret);
struct xbee_con * connection_xbee(struct xbee *xbee, struct xbee_con *con, xbee_err ret);
void read_send(int fd, struct xbee *xbee, struct xbee_con *con, xbee_err ret);
void callback_function(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data);

void main() {
	// Xbee errors variable
    xbee_err ret;

	// Open port
	int fd = open_port();
    // Configure xbee
    struct xbee *xbee = configure_xbee(xbee, ret);
    // Create a new connection
    struct xbee_con *con = connection_xbee(xbee, con, ret);

    // Read port and send data
	while(1) {
		read_send(fd, xbee, con, ret);
		sleep(READ_TIME);
	}

	// Close file descriptor
	close(fd);
	// Shutdown connection
	xbee_conEnd(con);
	// Shutdown libxbee 
	xbee_shutdown(xbee);
}

int open_port() {
	// File descriptor
	int fd;
	// Open port for read/write data
	fd = open("/dev/ttyUSB2", O_RDWR | O_NOCTTY);
	// ttyUSBX is the serial port
	// O_RDWR   - Read/Write access to serial port
	// O_NOCTTY - No terminal will control the process
	// Check if errors opening port
	if(fd == -1)
		printf("Opening port: %s(Code:%d)\n", strerror(errno), errno);
	else 
		printf("Opening port: %s(Code:%d)\n", strerror(errno), errno);

	// Setting the Attributes of the serial port using termios structure
	// Create the structure
	struct termios SerialPortSettings;
	// Get the current attributes of the Serial port 
	tcgetattr(fd, &SerialPortSettings);

	// Setting the Baud rate
	// Set Read  Speed as 9600
	cfsetispeed(&SerialPortSettings,B9600);
	// Set Write Speed as 9600 
	cfsetospeed(&SerialPortSettings,B9600);
	// cfmakeraw() sets the terminal to something like the "raw" mode of the old Version 7 terminal driver: input is available character by character, echoing is disabled, and all special processing of terminal input and output characters is disabled. 
	cfmakeraw(&SerialPortSettings);

	// 8N1 Mode
	// Disables the Parity Enable bit(PARENB), So No Parity
	SerialPortSettings.c_cflag &= ~PARENB;
	// CSTOPB = 2 Stop bits, here it's cleared so 1 Stop bit
	SerialPortSettings.c_cflag &= ~CSTOPB;
	// Clears the mask for setting the data size
	SerialPortSettings.c_cflag &= ~CSIZE;
	// Set the data bits = 8
	SerialPortSettings.c_cflag |= CS8;
	// No Hardware flow Control
	SerialPortSettings.c_cflag &= ~CRTSCTS;
	// Enable receiver, Ignore Modem Control lines
	SerialPortSettings.c_cflag |= (CREAD | CLOCAL);
	// Disable XON/XOFF flow control both i/p and o/p
	SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);
	// Non Cannonical mode
	SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ISIG);  
	// No Output Processing
	SerialPortSettings.c_oflag &= ~OPOST;

	// Setting Time outs
	// Read at least 1 characters
	SerialPortSettings.c_cc[VMIN] = 1;
	// Wait indefinetly 
	SerialPortSettings.c_cc[VTIME] = 0;

	// Set the attributes to the termios structure
	if((tcsetattr(fd, TCSANOW, &SerialPortSettings)) == -1) 
		printf("Setting attributes: %s(Code:%d)\n", strerror(errno), errno);
	else
		printf("Setting attributes: %s(Code:%d) \nBaudRate: 9600 \nStopBits: 1 \nParity: none\n", strerror(errno), errno);

	// Return file descriptor
	return fd;
}

struct xbee * configure_xbee(struct xbee *xbee, xbee_err ret) {
	// Setup libxbee, using USB port to serial adapter ttyUSBX at 9600 baud and check if errors 
	if((ret = xbee_setup(&xbee, "xbeeZB", "/dev/ttyUSB1", 9600)) == XBEE_ENONE)
		printf("Configuring xbee: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
	else
		printf("Configuring xbee: %s(Code:%d)\n", xbee_errorToStr(ret), ret);

	// Return xbee
	return xbee;
}

struct xbee_con * connection_xbee(struct xbee *xbee, struct xbee_con *con, xbee_err ret) {
	// Address of the remote xbee
	struct xbee_conAddress address;
	memset(&address, 0, sizeof(address));
	address.addr64_enabled = 1;
	address.addr64[0] = 0x00;
	address.addr64[1] = 0x13;
	address.addr64[2] = 0xA2;
	address.addr64[3] = 0x00;
	address.addr64[4] = 0x41;
	address.addr64[5] = 0x80;
	address.addr64[6] = 0xA7;
	address.addr64[7] = 0xC7;

	// Create a new AT connection to the remote xbee
	if((ret = xbee_conNew(xbee, &con, "Data", &address)) == XBEE_ENONE)
		printf("Connecting xbee: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
	else
		printf("Connecting xbee: %s(Code:%d)\n", xbee_errorToStr(ret), ret);

	// Return connection
	return con;
}

void read_send(int fd, struct xbee *xbee, struct xbee_con *con, xbee_err ret) {
	// Read data from serial port
	// Discards old data in the rx buffer
	tcflush(fd, TCIFLUSH);
    // Create buffer to store the data received
    char read_buffer[BUFFER_SIZE];
	// Set buffer in zero
	memset(read_buffer, '\0', BUFFER_SIZE);
	// Number of bytes read by the read() system call
	int  bytes_read = 0;
	// Read the data
	bytes_read = read(fd, read_buffer, BUFFER_SIZE);

	// Get current time and date
    // Initialize date/time struct
    time_t curtime;
    struct tm *timeinfo;
    // Get system current date/time
    time (&curtime);
    // Format date/time variable
    timeinfo = localtime(&curtime);
    // Get current date
    char dateString[12];
	strftime(dateString, sizeof(dateString)-1, "%Y-%m-%d", timeinfo);
    // Get current time
    char timeString[9];
    strftime(timeString, sizeof(timeString), "%H:%M:%S", timeinfo);

	// Create json object
    json_object *jobj = json_object_new_object();
    // Add date/time objects into main json object
	json_object *jdate = json_object_new_string(dateString);
    json_object_object_add(jobj, "Date", jdate);
	json_object *jtime = json_object_new_string(timeString);
    json_object_object_add(jobj, "Time", jtime);

 	// Check if errors reading port
	if(bytes_read == -1)
		printf("Reading port: %s(Code:%d)\n", strerror(errno), errno);
	else {
		// Print the number of bytes read
		printf("Reading %d bytes: %s(Code:%d)\n", bytes_read, strerror(errno), errno);
		// Convert ASCII code to their equivalent character
		int moisture;
		for(int i = 0; i < bytes_read; ++i) {
			char temp[BUFFER_SIZE];
			switch(read_buffer[i]) {
				case 45:
					temp[i] = '-';
					break;
				case 46:
					temp[i] = '.';
					break;
				case 48:
					temp[i] = '0';
					break;
				case 49:
					temp[i] = '1';
					break;
				case 50:
					temp[i] = '2';
					break;
				case 51:
					temp[i] = '3';
					break;
				case 52:
					temp[i] = '4';
					break;
				case 53:
					temp[i] = '5';
					break;
				case 54:
					temp[i] = '6';
					break;
				case 55:
					temp[i] = '7';
					break;
				case 56:
					temp[i] = '8';
					break;
				case 57:
					temp[i] = '9';
					break;
			}
			moisture = atoi(temp);
		}
		// Add the converted value to the json object
		json_object *jmoisture = json_object_new_int(moisture);
		json_object_object_add(jobj,"Moisture", jmoisture);
		// Print json object
		printf("JSON STRING: %s\n", json_object_to_json_string(jobj));
		// Create a temporal buffer
		char temp_buff[BUFFER_SIZE];

		// Send read data to the remote xbee
		if(strcpy(temp_buff, json_object_to_json_string(jobj)) == NULL)
			printf("Copying string: %s(Code:%d)\n", strerror(errno), errno);
		else {
			// Associate data with a connection
			if((ret = xbee_conDataSet(con, xbee, NULL)) == XBEE_ENONE)
				printf("Associating data: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
			else
				printf("Associating data: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
			
			// Configure a callback for the connection, this function is called every time a packet for this connection is received 
			if((ret = xbee_conCallbackSet(con, callback_function, NULL)) == XBEE_ENONE)
				printf("Configuring callback: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
			else
				printf("Configuring callback: %s(Code:%d)\n", xbee_errorToStr(ret), ret);

			// Transmit data to the remote xbee
			if((ret = xbee_conTx(con, NULL, temp_buff)) == XBEE_ENONE)
				printf("Sending data: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
			else
				printf("Sending data: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
			}
		}
}

/* Callback function, it will be executed once for each packet that is received on an associated connection */
void callback_function(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data) {
	printf("rx: %s\n", (*pkt)->data);
}

Sharing settings
