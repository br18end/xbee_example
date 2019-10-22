#include <stdio.h> /* Standard input/output definitions */
#include <stdlib.h> /* Standard General Utilities Library */
#include <fcntl.h> /* File Control Definitions */
#include <unistd.h> /* UNIX Standard Definitions */
#include <errno.h> /* ERROR Number Definitions */
#include <string.h> /* String function definitions */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <time.h> /* Date and time manipulation */
#include <json-c/json.h> /* Parser-JSON */
#include <mysql/mysql.h> /* MariaDB Database Definitions */
#include <xbee.h> /* Digi's XBee wireless modules definitions */
#define BUFFER_SIZE 256 // Buffer size
#define SEND_TIME 5 // Send time in sec

// Declare functions
struct xbee * configure_xbee(struct xbee *xbee, xbee_err ret);
struct xbee_con * connection_xbee(struct xbee *xbee, struct xbee_con *con, xbee_err ret);
void receive_data(struct xbee *xbee, struct xbee_con *con, xbee_err ret);
MYSQL * mariadb_connection();
void callback_function(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data);
// Create buffer to store the data received
char read_buffer[BUFFER_SIZE];

void main() {
   // Xbee errors variable
    xbee_err ret;

    // Configure xbee
    struct xbee *xbee = configure_xbee(xbee, ret);
    // Create a new connection
    struct xbee_con *con = connection_xbee(xbee, con, ret);
    
    // Receive data from the remote xbee
    while(1) {
        receive_data(xbee, con, ret);
        sleep(SEND_TIME);
        mariadb_connection();
    }
    
    // Shutdown connection
    xbee_conEnd(con);
    // Shutdown libxbee 
    xbee_shutdown(xbee);
}

struct xbee * configure_xbee(struct xbee *xbee, xbee_err ret) {
    // Setup libxbee, using USB port to serial adapter ttyUSBX at 9600 baud and check if errors 
    if((ret = xbee_setup(&xbee, "xbeeZB", "/dev/ttyUSB0", 9600)) == XBEE_ENONE)
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
    address.addr64[6] = 0xAD;
    address.addr64[7] = 0xDE;

    // Create a new AT connection to the remote xbee
    if((ret = xbee_conNew(xbee, &con, "Data", &address)) == XBEE_ENONE)
        printf("Connecting xbee: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
    else
        printf("Connecting xbee: %s(Code:%d)\n", xbee_errorToStr(ret), ret);

    // Return connection
    return con;
}

void receive_data(struct xbee *xbee, struct xbee_con *con, xbee_err ret) {
    // Associate data with a connection
    if ((ret = xbee_conDataSet(con, xbee, NULL)) != XBEE_ENONE)
        printf("Associating data: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
    // Configure a callback for the connection, this function is called every time a packet for this connection is received 
    if ((ret = xbee_conCallbackSet(con, callback_function, NULL)) != XBEE_ENONE)
        printf("Configuring callback: %s(Code:%d)\n", xbee_errorToStr(ret), ret);
}

/* Callback function, it will be executed once for each packet that is received on an associated connection */
void callback_function(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data) {
    // Print received data
    printf("rx: %s\n", (*pkt)->data);
    // Store data in buffer
    memset(read_buffer, '\0', BUFFER_SIZE);
    strcpy(read_buffer, (*pkt)->data);
    // Transmit a message
    xbee_conTx(con, NULL, "Receiving data: OK");
}

MYSQL * mariadb_connection() {
    // Create connection
    MYSQL *con = mysql_init(NULL);

    // Check if errors trying to connect
    if (con == NULL)
        printf("Initializing connection: %s\n", mysql_error(con));
    else
        printf("Initializing connection: Success\n");
    
    // Connect to mariadb
    if (mysql_real_connect(
        con, /* MYSQL structure to use */
        "localhost", // server hostname or IP address 
        "", // mysql user
        "", // password
        NULL, // default database to use, NULL for none 
        0, // port number, 0 for default
        NULL, // socket file or named pipe name
        0 // connection flags  
        ) == NULL)
        printf("Connecting to mariadb: %s\n", mysql_error(con));
    else
        printf("Connecting to mariadb: Success\n");

    // Select database
    if (mysql_select_db(con, "agroiotec") != 0)
        printf("Selecting database: %s\n", mysql_error(con));
    else
        printf("Selecting database: Success\n");

    /* Pass temp buffer to json object*/
    json_object *jtemp = json_tokener_parse(read_buffer);
    // Get date from json object
    json_object *jdate;
    json_object_object_get_ex(jtemp, "Date", &jdate);
    char *date;
    date = (char *)json_object_get_string(jdate);
    // Get time from json object
    json_object *jtime;
    json_object_object_get_ex(jtemp, "Time", &jtime);
    char *time;
    time = (char *)json_object_get_string(jtime);
    // Get moisture from json object
    json_object *jmoisture;
    json_object_object_get_ex(jtemp, "Moisture", &jmoisture);
    int moisture = json_object_get_int(jmoisture);

    // Create query
    char query[BUFFER_SIZE];
    sprintf(query, "INSERT INTO mediciones(fecha, hora, medicion) VALUES(\"%s\", \"%s\", %d)", date, time, moisture);

    // Insert data and check if errors
    if (mysql_query(con, query) != 0)
        printf("Inserting data: %s\n", mysql_error(con));
    else
        printf("Inserting data: Success\n");

    return con;
}

Sharing settings
