#include <stdio.h>  // console input/output, perror
#include <stdlib.h> // exit
#include <string.h> // string manipulation
#include <ctype.h>  // ctypes
#include <netdb.h>  // getnameinfo

#include <sys/socket.h> // socket APIs
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>     // open, close

#include <signal.h> // signal handling
#include <time.h>   // time

#include <sqlite3.h> 

#define SIZE 1024  // buffer size
#define PORT 2728  // port number
#define BACKLOG 10 // number of pending connections queue will hold
#define SUBMAP_SIZE 20

/**
 * @brief Generates file URL based on route
 * @param route requested route
 * @param fileurl generated url
 */
void getFileURL(char *route, char *fileURL);

/**
 * @brief Sets *MIME to the mime type of file
 * @param file file URL
 * @param mime mime type of file
 */
void getMimeType(char *file, char *mime);

/**
 * @brief Handles SIGINT signal
 */
void handleSignal(int signal);

/**
 * @brief Returns a string with the current time in HTTP response date format
 * @param buf buffer to store the time string
 * https://stackoverflow.com/questions/7548759/generate-a-date-string-in-http-response-date-format-in-c
 */
void getTimeString(char *buf);

typedef struct {
    FILE *fp;
    sqlite3 *dbGiven;
    int color;
    int subMap;
    int selected;
    int credits;
} CallbackData;

void sqlQuery(const char *data, FILE *fGiven, sqlite3 *dbGiven, CallbackData *dbData, int *choices, int choicesCnt);
static int callback(void *data, int argc, char **argv, char **NotUsed);
int choicesArr(int n, int *choices);

int serverSocket;
int clientSocket;

char *request;



int main()
{

  // register signal handler
  signal(SIGINT, handleSignal);

  // server internet socket address
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;                     // IPv4
  serverAddress.sin_port = htons(PORT);                   // port number in network byte order (host-to-network short)
  serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost (host to network long)

  // socket of type IPv4 using TCP protocol
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  // reuse address and port
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

  // bind socket to address
  if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    printf("Error: The server is not bound to the address.\n");
    return 1;
  }

  // listen for connections
  if (listen(serverSocket, BACKLOG) < 0)
  {
    printf("Error: The server is not listening.\n");
    return 1;
  }

  // get server address information
  char hostBuffer[NI_MAXHOST], serviceBuffer[NI_MAXSERV];
  int error = getnameinfo((struct sockaddr *)&serverAddress, sizeof(serverAddress), hostBuffer,
                          sizeof(hostBuffer), serviceBuffer, sizeof(serviceBuffer), 0);

  if (error != 0)
  {
    printf("Error: %s\n", gai_strerror(error));
    return 1;
  }

  printf("\nServer is listening on http://%s:%s/\n\n", hostBuffer, serviceBuffer);

  while (1)
  {
    // buffer to store data (request)
    request = (char *)malloc(SIZE * sizeof(char));
    char method[10], route[100];

    // accept connection and read data
    clientSocket = accept(serverSocket, NULL, NULL);
    read(clientSocket, request, SIZE);

    // parse HTTP request
    sscanf(request, "%s %s", method, route);
    printf("%s %s", method, route);

    free(request);

    // only support GET method
    if (strcmp(method, "GET") != 0)
    {
      const char response[] = "HTTP/1.1 400 Bad Request\r\n\n";
      send(clientSocket, response, sizeof(response), 0);
    }
    else
    {
      char fileURL[100];

      // generate file URL
      getFileURL(route, fileURL);
      // read file
      FILE *file = fopen(fileURL, "r");

      if (file)
      {
        // generate HTTP response header
        char resHeader[SIZE];

        // get current time
        char timeBuf[100];
        getTimeString(timeBuf);

        // generate mime type from file URL
        char mimeType[32];
        getMimeType(fileURL, mimeType);

        sprintf(resHeader, "HTTP/1.1 200 OK\r\nDate: %s\r\nContent-Type: %s\r\n\n", timeBuf, mimeType);
        int headerSize = strlen(resHeader);

        printf(" %s", mimeType);

        // Calculate file size
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Allocates memory for response buffer and copies response header and file contents to it
        char *resBuffer = (char *)malloc(fsize + headerSize);
        strcpy(resBuffer, resHeader);

        // Starting position of file contents in response buffer
        char *fileBuffer = resBuffer + headerSize;
        fread(fileBuffer, fsize, 1, file);

        send(clientSocket, resBuffer, fsize + headerSize, 0);
        free(resBuffer);
        fclose(file);
      }
      else
      {
        const char response[] = "HTTP/1.1 404 Not Found\r\n\n";
        send(clientSocket, response, sizeof(response), 0);
      }
    }
    close(clientSocket);
    printf("\n");
  }
}

void getFileURL(char *route, char *fileURL)
{
    CallbackData callbackData;
    callbackData.color = 1;
    callbackData.selected = 1;
    char *ascend_descend = NULL;
    int sort = 0;
    
    if (strstr(route, "addSelected"))
    {
        callbackData.selected = 3;
    }
    else if (strstr(route, "clearSelected"))
    {
        callbackData.selected = 4;
    }
    else if (strstr(route, "clearAll"))
    {
        callbackData.selected = 5;
    }
    // If route has parameters, extract them
    char *question = strrchr(route, '?');
    if (question) {
        // Extract parameter values
        int *choices = (int *)malloc(sizeof(int) * 10);
        if (choices == NULL)
        {
            printf("Not enough memory!\n");
        }
        int choicesMem = 10;
        int choicesNum = 0;
        int first = 1;
        int uni = 0;
        int fac = 0;
        int degree = 0;
        int semester = 0;
        char sqlQueryString[SIZE] = "SELECT id,Code,Course,Semester,Credits,Faculty,Studylevel,8 FROM courses WHERE";
        char *parameters = question + 1; // Skip the '?'
        char *pair = strtok(parameters, "&");
        while (pair != NULL) {
            char key[25];
            int and = 0;
            strcpy(key, pair);
            for (long unsigned int i = 0; i < strlen(key); i++)
            {
                if (key[i] == '=')
                {
                    key[i] = '\0';
                }
            }
            char value[64];
            char *tempValue = strrchr(pair, '=') + 1;
            strcpy(value, tempValue);
            if (strrchr(value, '\'') || strrchr(value, '%'))
            {
                fprintf(stderr, "Error! invalid query string\n");
                break;
            }
            int valueLen = strlen(value);
            for (int i = 0; i < valueLen + 1; i++)
            {
                if (value[i] == '+')
                {
                    value[i] = ' ';
                }
            }
            
            //printf("Key: %s and Value: %s\n", key, value);
            int queryLen = strlen(sqlQueryString) + 1;
            char tempSqlString[queryLen];
            for (int i = 0; i < queryLen; i++)
            {
                tempSqlString[i] = sqlQueryString[i];
                if (sqlQueryString[i] == '\0')
                {
                    break;
                }
            }
            if (tempValue) {
                
                if (strcmp(key, "fac") == 0)
                {
                    if (strcmp(value, "Mari") == 0)
                    {
                        strcpy(value, "Estonian Maritime Academy");
                    }
                    else if (strcmp(value, "Busi") == 0)
                    {
                        strcpy(value, "School of Business and Governance");
                    }
                    else if (strcmp(value, "Engi") == 0)
                    {
                        strcpy(value, "School of Engineering");
                    }
                    else if (strcmp(value, "Infor") == 0)
                    {
                        strcpy(value, "School of Information Technologies");
                    }
                    else if (strcmp(value, "Scien") == 0)
                    {
                        strcpy(value, "School of Science");
                    }
                    
                    
                    else if (strcmp(value, "AplM") == 0)
                    {
                        strcpy(value, "Department of Applied Mathematics and Computer Science");
                    }
                    else if (strcmp(value, "Phys") == 0)
                    {
                        strcpy(value, "Department of Physics");
                    }
                    else if (strcmp(value, "Envir") == 0)
                    {
                        strcpy(value, "Department of Environmental and Resource Engineering");
                    }
                    else if (strcmp(value, "Healt") == 0)
                    {
                        strcpy(value, "Department of Health Technology");
                    }
                    else if (strcmp(value, "Food") == 0)
                    {
                        strcpy(value, "National Food Institute");
                    }
                    else if (strcmp(value, "Aqua") == 0)
                    {
                        strcpy(value, "National Institute of Aquatic Resources");
                    }
                    else if (strcmp(value, "Chem") == 0)
                    {
                        strcpy(value, "Department of Chemistry");
                    }
                    else if (strcmp(value, "Bio") == 0)
                    {
                        strcpy(value, "Department of Biotechnology and Biomedicine");
                    }
                    else if (strcmp(value, "Chemical") == 0)
                    {
                        strcpy(value, "Department of Chemical Engineering");
                    }
                    else if (strcmp(value, "Biosus") == 0)
                    {
                        strcpy(value, "DTU Biosustain");
                    }
                    else if (strcmp(value, "Space") == 0)
                    {
                        strcpy(value, "National Space Institute");
                    }
                    else if (strcmp(value, "Elect") == 0)
                    {
                        strcpy(value, "Department of Electrical and Photonics Engineering");
                    }
                    else if (strcmp(value, "Mech") == 0)
                    {
                        strcpy(value, "Department of Civil and Mechanical Engineering");
                    }
                    else if (strcmp(value, "Manag") == 0)
                    {
                        strcpy(value, "Department of Technology Management and Economics");
                    }
                    else if (strcmp(value, "Wind") == 0)
                    {
                        strcpy(value, "Department of Wind and Energy Systems");
                    }
                    else if (strcmp(value, "Conver") == 0)
                    {
                        strcpy(value, "Department of Energy Conversion and Storage");
                    }
                    else if (strcmp(value, "Didac") == 0)
                    {
                        strcpy(value, "Department of Engineering Technology and Didactics");
                    }
                    else if (strcmp(value, "OtherC") == 0)
                    {
                        strcpy(value, "Other courses");
                    }
                    
                }
                if (strcmp(key, "sort") == 0) 
                {
                    sort = atoi(value);
                } 
                else if (strcmp(key, "ascend_descend") == 0) 
                {
                    if (strcmp(value, "descending") == 0)
                    {
                        ascend_descend = "DESC";
                    }
                    else 
                    {
                        ascend_descend = "ASC";
                    }
                } 
                else if (first == 1)
                {
                    first = 0;
                }
                else
                {
                    and = 1;
                }
                
                
                if (strcmp(key, "fac") == 0 && fac == 1) 
                {
                    sprintf(sqlQueryString, "%s or Faculty = '%s'", tempSqlString, value);
                    fac = 1;
                }
                else if (strcmp(key, "fac") == 0 && and == 1) 
                {
                    sprintf(sqlQueryString, "%s and Faculty = '%s'", tempSqlString, value);
                    fac = 1;
                }
                else if (strcmp(key, "fac") == 0) 
                {
                    sprintf(sqlQueryString, "%s Faculty = '%s'", tempSqlString, value);
                    fac = 1;
                }
                
                else if (strcmp(key, "degree") == 0 && degree == 1) 
                {
                    sprintf(sqlQueryString, "%s or Studylevel like '%%%s%%'", tempSqlString, value);
                    degree = 1;
                } 
                else if (strcmp(key, "degree") == 0 && and == 1) 
                {
                    sprintf(sqlQueryString, "%s and Studylevel like '%%%s%%'", tempSqlString, value);
                    degree = 1;
                } 
                else if (strcmp(key, "degree") == 0) 
                {
                    sprintf(sqlQueryString, "%s Studylevel like '%%%s%%'", tempSqlString, value);
                    degree = 1;
                } 
                
                else if (strcmp(key, "semester") == 0 && semester == 1) 
                {
                    sprintf(sqlQueryString, "%s or Semester like '%%%s%%'", tempSqlString, value);
                    semester = 1;
                } 
                else if (strcmp(key, "semester") == 0 && and == 1) 
                {
                    sprintf(sqlQueryString, "%s and Semester like '%%%s%%'", tempSqlString, value);
                    semester = 1;
                } 
                else if (strcmp(key, "semester") == 0) 
                {
                    sprintf(sqlQueryString, "%s Semester like '%%%s%%'", tempSqlString, value);
                    semester = 1;
                } 
                
                else if (strcmp(key, "uni") == 0 && uni == 1) 
                {
                    sprintf(sqlQueryString, "%s or University = '%s'", tempSqlString, value);
                    uni = 1;
                }
                else if (strcmp(key, "uni") == 0 && and == 1) 
                {
                    sprintf(sqlQueryString, "%s and University = '%s'", tempSqlString, value);
                    uni = 1;
                }
                else if (strcmp(key, "uni") == 0) 
                {
                    sprintf(sqlQueryString, "%s University = '%s'", tempSqlString, value);
                    uni = 1;
                }
                
                else if (strcmp(key, "cname") == 0 && and == 1)
                {
                    sprintf(sqlQueryString, "%s and Course like '%%%s%%'", tempSqlString, value);
                }
                else if (strcmp(key, "cname") == 0)
                {
                    sprintf(sqlQueryString, "%s Course like '%%%s%%'", tempSqlString, value);
                }
                else if (strcmp(key, "choice") == 0)
                {
                    if (choicesNum + 1 >= choicesMem)
                    {
                        choicesMem = choicesArr(choicesMem, choices);
                    }

                    int *pChoice = choices + choicesNum;
                    *pChoice = atoi(value);
                    choicesNum++;
                }
                else if (strcmp(key, "selected") == 0)
                {
                    printf("selected = 2\n");
                    callbackData.selected = 2;
                }
                
            }
            pair = strtok(NULL, "&");
        }
        int queryLen = strlen(sqlQueryString) + 1;
        char tempSqlString[queryLen];
        for (int i = 0; i < queryLen; i++)
        {
            tempSqlString[i] = sqlQueryString[i];
            if (sqlQueryString[i] == '\0')
            {
                break;
            }
        }
        if (strstr(tempSqlString, "University") == NULL && first == 0)
        {
            sprintf(sqlQueryString, "%s and University = '%s'", tempSqlString, "CTU");
        }
        else if (strstr(tempSqlString, "University") == NULL && first == 1)
        {
            sprintf(sqlQueryString, "%s University = '%s'", tempSqlString, "CTU");
        }
        
        
        int queryLen2 = strlen(sqlQueryString) + 1;
        char tempSqlString2[queryLen2];
        for (int i = 0; i < queryLen2; i++)
        {
            tempSqlString2[i] = sqlQueryString[i];
            if (sqlQueryString[i] == '\0')
            {
                break;
            }
        }
        if (sort != 0 && ascend_descend != NULL)
        {
            sprintf(sqlQueryString, "%s ORDER BY %d %s", tempSqlString2, sort, ascend_descend);
        }
        else if (sort != 0)
        {
            sprintf(sqlQueryString, "%s ORDER BY %d", tempSqlString2, sort);
        }
        
        if (callbackData.selected == 1)
        {
            //SQL QUERY CALL
            printf("\nSQL: %s\n", sqlQueryString);
            sqlQuery(sqlQueryString, 0, 0, &callbackData, NULL, 0);
            sqlite3_close(callbackData.dbGiven);
            fprintf(stderr, "Closed database successfully\n");
            fclose(callbackData.fp);
            fprintf(stderr, "Closed output.html successfully\n");
        }
        else if (callbackData.selected == 2)
        {
            sqlQuery("selec", 0, 0, &callbackData, NULL, 0);
            sqlite3_close(callbackData.dbGiven);
            fprintf(stderr, "Closed database successfully\n");
            fclose(callbackData.fp);
            fprintf(stderr, "Closed output.html successfully\n");
        }
        else
        {
            sqlQuery(sqlQueryString, 0, 0, &callbackData, choices, choicesNum);
            sqlite3_close(callbackData.dbGiven);
            fprintf(stderr, "Closed database successfully\n");
        }
        free(choices);
        *question = '\0'; // Remove parameters by replacing '?' with '\0'
        
    }
  
    // if route is empty, set it to index.html
    if (route[strlen(route) - 1] == '/')
    {
        strcat(route, "index.html");
    }

    // get filename from route
    strcpy(fileURL, "htdocs");
    strcat(fileURL, route);

    // if filename does not have an extension, set it to .html
    const char *dot = strrchr(fileURL, '.');
    if (!dot || dot == fileURL)
    {
        strcat(fileURL, ".html");
    }
    if (callbackData.selected == 4 || callbackData.selected == 5)
    {
        strcpy(fileURL, "ctu.html");
    }
}

void getMimeType(char *file, char *mime)
{
  // position in string with period character
  const char *dot = strrchr(file, '.');

  // if period not found, set mime type to text/html
  if (dot == NULL)
    strcpy(mime, "text/html");

  else if (strcmp(dot, ".html") == 0)
    strcpy(mime, "text/html");

  else if (strcmp(dot, ".css") == 0)
    strcpy(mime, "text/css");

  else if (strcmp(dot, ".js") == 0)
    strcpy(mime, "application/js");

  else if (strcmp(dot, ".jpg") == 0)
    strcpy(mime, "image/jpeg");

  else if (strcmp(dot, ".png") == 0)
    strcpy(mime, "image/png");

  else if (strcmp(dot, ".gif") == 0)
    strcpy(mime, "image/gif");
    
  else
    strcpy(mime, "text/html");
}

void handleSignal(int signal)
{
  if (signal == SIGINT)
  {
    printf("\nShutting down server...\n");

    close(clientSocket);
    close(serverSocket);

    if (request != NULL)
      free(request);

    exit(0);
  }
}

void getTimeString(char *buf)
{
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}


void sqlQuery(const char *data, FILE *fGiven, sqlite3 *dbGiven, CallbackData *dbData, int *choices, int choicesCnt)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    char sql[SIZE];
    FILE *fp;
    CallbackData *callbackData = dbData;
    callbackData->subMap = 0;
    
    /* Open database */
    if (dbGiven)
    {
        db = dbGiven;
    }
    else
    {
        rc = sqlite3_open("euroteq.db", &db);
        
        if( rc ) {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            exit(EXIT_FAILURE);
        } else {
            fprintf(stderr, "Opened database successfully\n");
        } 
        callbackData->dbGiven = db;
    }
    
   
    /* Create SQL statement */
    if (callbackData->selected == 5)
    {
        strcpy(sql, "DELETE FROM selected");
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);

        if (rc != SQLITE_OK) 
        {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } 
    }
    else if (callbackData->selected == 4 && choices != NULL)
    {
        for (int i = 0; i < choicesCnt; i++)
        {
            int *pChoice = choices + i;
            sprintf(sql, "DELETE FROM selected where id = '%d'", *pChoice);
            rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);

            if (rc != SQLITE_OK) 
            {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }
        }
        
        return;
    }
    else if (choices != NULL)
    {
        for (int i = 0; i < choicesCnt; i++)
        {
            int *pChoice = choices + i;
            sprintf(sql, "SELECT * from courses where id = '%d'", *pChoice);
            rc = sqlite3_exec(db, sql, callback, (void*)callbackData, &zErrMsg);

            if (rc != SQLITE_OK) 
            {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }
        }
        
        return;
    }
    else if (strcmp(data, "selec") == 0)
    {
        callbackData->credits = 0;
        strcpy(sql, "SELECT * from selected");
        printf("\nSQL: %s\n", sql);
    }
    else if (strstr(data, "SELECT"))
    {
        strcpy(sql, data);
    }
    else if (strstr(data, "=fnSubMap"))
    {
        char *equals = strrchr(data, '=');
        int idC;
        *equals = '\0';
        sprintf(sql, "SELECT * from subjectmap where id = '%s'", data);
        idC = atoi(data);
        callbackData->subMap = idC;
    }
    else
    {
        strcpy(sql, "SELECT * from courses");
    }
    
    // Open file for writing HTML table
    if (fGiven)
    {
        fp = fGiven;
    }
    else
    {
        fp = fopen("htdocs/output.html", "w");
        if(fp == NULL) {
            fprintf(stderr, "Error opening file\n");
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Opened output.html successfully\n");
        // Write HTML table header
        fprintf(fp, "<html>\n<head>\n<link href=\"styles/styleOutput.css\" "
                    "rel=\"stylesheet\" type=\"text/css\" />"
                    "\n</head>\n<body>\n");
        
        fprintf(fp, "<form action=\"selection\" method=\"get\">\n"
                    "<input type=\"submit\" name=\"addSelected\" value=\"Add Selected\">\n"
                    "<input type=\"submit\" name=\"clearSelected\" value=\"Clear Selected\">\n");
        
        fprintf(fp, "<input type=\"submit\" name=\"clearAll\" value=\"Clear all\">\n"
                    "<div style=\"overflow:scroll; height:600px;\">"
                    "<table border=\"1\" cellspacing=\"0\">\n");
                
        fprintf(fp, "<thead>\n<tr class=\"pair\">\n<th>Code</th><th>Course</th>"
                    "<th>Semester</th><th>Credits</th><th>Faculty</th>"
                    "<th>Study level</th><th>Choose</th>\n</tr>\n</thead>\n<tbody>\n");
        
        callbackData->fp = fp;
    }
    
    
    
    
    /* Execute SQL statement */
    rc = sqlite3_exec(db, sql, callback, (void*)callbackData, &zErrMsg);
   
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        //fprintf(stdout, "Operation done successfully\n");
    }
    
    if (!fGiven && callbackData->selected == 2)
    {
        fprintf(fp, "</div>\n</form>\n<p>Total credits: %d</p>\n"
                    "</tbody>\n</table>\n</body>\n</html>", callbackData->credits);
    }
    else if (!fGiven)
    {
        fprintf(fp, "</div>\n</form>\n</tbody>\n</table>\n</body>\n</html>");
    }
    
}


static int callback(void *data, int argc, char **argv, char **NotUsed)
{
    int i;
    CallbackData *callbackData = (CallbackData *)data;
    FILE *fp = callbackData->fp;
    sqlite3 *db = callbackData->dbGiven;
    
    if (callbackData->selected == 3)
    {
        char *zErrMsg = 0;
        int rc;
        char sql[SIZE];
        sprintf(sql, "INSERT OR IGNORE INTO selected (id, Code, Course, Semester, Credits, Faculty, Studylevel, University) VALUES (%d, '%s', '%s', '%s', %d, '%s', '%s', '%s')",
                atoi(argv[0]),
                argv[1],
                argv[2],
                argv[3],
                atoi(argv[4]),
                argv[5],
                argv[6],
                argv[7]);
        printf("\nSQL: %s\n", sql);
        rc = sqlite3_exec(db, sql, 0, 0, &zErrMsg);

        if (rc != SQLITE_OK) 
        {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        return 0;
    }
    if (callbackData->subMap != 0)
    {
        fprintf(fp, "<a href=\"%s\">%s</a>", argv[3] ? argv[3] : "NULL",
        argv[2] ? argv[2] : "NULL");
        callbackData->subMap = 0;
        return 0;
    }
    if (callbackData->color % 2 == 0)
    {
        fprintf(fp, "<tr class=\"pair\">");
    }
    else
    {
        fprintf(fp, "<tr class=\"pairless\">");
    }
    
    callbackData->color++;

    for(i = 1; i<argc - 1; i++) {
        if (i == 2)
        {
            char tempStr[SUBMAP_SIZE];
            sprintf(tempStr, "%s=fnSubMap", argv[i - 2]);
            fprintf(fp, "<td>");
            sqlQuery(tempStr, fp, db, callbackData, NULL, 0);
            fprintf(fp, "</td>");
        }
        else if (i == 4)
        {
            callbackData->credits = callbackData->credits + atoi(argv[i]);
            fprintf(fp, "<td>%s</td>", argv[i] ? argv[i] : "NULL");
        }
        else
        {
            fprintf(fp, "<td>%s</td>", argv[i] ? argv[i] : "NULL");
        }
        
    }


    fprintf(fp, "<td><input type=\"checkbox\" id=\"%s\" name=\"choice\" value=\"%s\" class=\"clr-checkbox\"></td>", argv[0], argv[0]);
    // End HTML table row
    fprintf(fp, "</tr>\n");
   
    return 0;
}

int choicesArr(int n, int *choices)
{
    int newLimit = n + 10;

    int *pTemp = (int *)realloc(choices, sizeof(int) * newLimit);
    if (choices != NULL)
    {
        choices = pTemp;
    }
    else
    {
        printf("Not enough memory!\n");
    }
    return newLimit;
}
