#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <regex.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

#define sockaddr struct sockaddr
#define MAX_PASSED_MESSAGE_LEN 1000
#define MAX_SUBJECT_LEN 200
#define MAX_CONTENT_LEN 800
#define MAX_TOTAL_MESSAGE_LEN 1200
#define DATE_LENGTH 300
#define MAX_USERS 100
#define MAX_USERNAME_LENGTH 101
#define PATH "MAILSERVER/"
#define MAX_FILENAME_PATH_LENGTH 200
#define NIL '\0'
#define PASSWORD_STORE_LENGTH 200
#define PASSWORD_LENGTH 100

char user_passwords[MAX_USERS][PASSWORD_STORE_LENGTH];
char list_active_users[MAX_USERS][MAX_USERNAME_LENGTH];    // array of active user_ids
long current_mail_pointer[MAX_USERS];    // -1 represents invalid
int current_username_index;    // -1 represents invalid

/*
Initialize mail pointers to -1 to indicate invalid mail pointers
*/
void init_mail_pointers()
{
    for(int i = 0; i < MAX_USERS; i++)
    {
        current_mail_pointer[i] = -1; 
    }
}

/* 
Write password array to memory 
*/
void write_passwords_to_file()
{
    FILE *fp = fopen("MAILSERVER/definitely_not_passwords", "wb");
    fwrite(user_passwords, sizeof(char), sizeof(user_passwords), fp);
    fclose(fp);
}

void read_passwords_from_file()
{
    FILE *fp = fopen("MAILSERVER/definitely_not_passwords", "rb");
    fread(user_passwords, sizeof(char), sizeof(user_passwords), fp);
    fclose(fp);
}

/* 
Setup the server based on the files in MAILSERVER in case of server breakdown and restart 
*/
void init_server()
{
    /* Setup list_active_users based on existing file names */
    DIR *d;
    struct dirent *dir;
    d = opendir("MAILSERVER");
    if(d)
    {
        /* If directory exists, assumes that the directory has been saved from a prior execution */
        read_passwords_from_file();
        char* password_store = (char*)calloc(PASSWORD_STORE_LENGTH, sizeof(char));
        for(int i = 0; i < MAX_USERS; i++)
        {
            if(user_passwords[i][0])
            {
                strcpy(password_store, user_passwords[i]);
                char* token = strtok(password_store, " ");
                strcpy(list_active_users[i], token);
            }
        }
        free(password_store);
        closedir(d);
    }
    else
    {
        /* If directory doesn't exist, create both the directory and the corresponding password file */
        int check = mkdir("MAILSERVER", 0777);
        if(check)
        {
            printf("Error in creating server directories\n");
            exit(1);
        }
        FILE* fp = fopen("MAILSERVER/definitely_not_passwords", "w");
        if(fp == NULL)
        {
            printf("Password file creation error\n");
            exit(1);
        }
        fclose(fp);
    }


    /* Initialize mail pointers to -1 */
    init_mail_pointers();

    /* Initialize current user to -1 */
    current_username_index = -1;
}

/* 
Utility function to create a filename path for a given user. 
Note, allocates space on heap, caller responsibility to deallocate/free.
*/
char* make_filename_path(char* username)
{
    char* filename = (char*)calloc(MAX_FILENAME_PATH_LENGTH, sizeof(char));
    strcat(filename, PATH);
    strcat(filename, username);
    return filename;
}

/* 
Utility function to create a from entry string for a given user
Note, allocates space on heap, caller responsibility to deallocate/free.
*/
char* make_from_entry(char* username)
{
    char* from_string = (char*)calloc(7 /* len("From: ") + len("\n") */ + strlen(username) + 1, sizeof(char));
    strcat(from_string, "From: ");
    strcat(from_string, username);
    strcat(from_string, "\n");
    return from_string;
}

/* 
Utility function to create a to entry string for a given user
Note, allocates space on heap, caller responsibility to deallocate/free.
*/
char* make_to_entry(char* username)
{
    char* to_string = (char*)calloc(5 /* len("To: ") + len("\n") */ + strlen(username) + 1, sizeof(char));
    strcat(to_string, "To: ");
    strcat(to_string, username);
    strcat(to_string, "\n");
    return to_string;
}

/* 
Returns a pointer to a space seperated string of user_id's 
Note, allocates space on heap, caller responsibility to deallocate/free.
*/
char* do_lstu()
{
    char* message_string = (char*)calloc(MAX_USERS * MAX_USERNAME_LENGTH, sizeof(char));
    for(int i = 0; i < MAX_USERS; i++)
    {
        if(list_active_users[i][0] != NIL)
        {
            strcat(message_string, list_active_users[i]);
            strcat(message_string, " ");
        }
    }
    return message_string;
}

/* 
Creates a mail spool file for the user and returns 1 on a success, returns 0 if the user already exists, -1 on a file creation error 
Password for the user is also set based on the corresponding argument and stored in the server's files. 
*/
int do_addu(char* username, char* password)
{
    int i;
    for(i = 0; i < MAX_USERS; i++)
    {
        if(strcmp(username, list_active_users[i]) == 0)
        {
            return 0;
        }
        else if(list_active_users[i][0] == NIL)
        {
            int j;
            for(j = 0; ((username[j] != NIL) && (j < MAX_USERNAME_LENGTH - 1)); j++)
            {
                list_active_users[i][j] = username[j];
            }
            list_active_users[i][j] = NIL; 
            strcpy(user_passwords[i], list_active_users[i]);
            strcat(user_passwords[i], " ");
            strcat(user_passwords[i], password);
            write_passwords_to_file();
            break;
        }
        else 
        {
            continue;
        }
    }
    char* filename = make_filename_path(list_active_users[i]);
    FILE* fp = fopen(filename, "w");
    free(filename);
    if(fp != NULL)
    {
        fclose(fp);
        return 1;
    }
    else
    {
        return -1;
    }
}

/* 
If the user exists, and his password matches with the one on the server's local store, returns number of mails in the user's spool file, current_file_index is set to that of the given user and the current_mail pointer for this user is set to the beginning of his/her spool file. 
If the user does not exist, returns -1. 
If the user exists, but the password doesn't match, returns -2. 
Returns -3 on file errors or malloc errors. 
*/
int do_user(char* username, char* password)
{
    int flag = 0;
    int i;
    for(i = 0; i < MAX_USERS; i++)
    {
        if(strcmp(username, list_active_users[i]) == 0)
        {
            flag = 1;
            break;
        }
    }
    if(flag == 0)
    {
        return -1;
    }
    current_username_index = i;

    /* password checking */
    char* password_store = (char*)calloc(PASSWORD_STORE_LENGTH, sizeof(char));
    strcpy(password_store, user_passwords[current_username_index]);
    char* token = strtok(password_store, " ");
    token = strtok(NULL, " ");
    if(strcmp(password, token) != 0)
    {
        free(password_store);
        return -2;
    }
    free(password_store);

    /* open the corresponding spool file for reading */
    char* filename = make_filename_path(username);
    FILE* fp = fopen(filename, "r");
    free(filename);

    char* mail_spool_buffer;
    long len_mail_spool;
    
    /* quit if the file does not exist */
    if(fp == NULL)
    {
        printf("file error\n");
        return -3;
    }
    
    /* Get the number of bytes */
    fseek(fp, 0L, SEEK_END);
    len_mail_spool = ftell(fp); // number of bytes excluding the null charecter required to terminate the string
    
    /* reset the file position indicator to 
    the beginning of the file */
    fseek(fp, 0L, SEEK_SET);	
    
    /* grab sufficient memory for the 
    buffer to hold the text */
    mail_spool_buffer = (char*)calloc(len_mail_spool + 1, sizeof(char));	
    
    /* memory error */
    if(mail_spool_buffer == NULL)
    {
        printf("malloc error\n");
        return -3;
    }
    
    /* copy all the text into the buffer */
    fread(mail_spool_buffer, sizeof(char), len_mail_spool, fp);

    /* count the number of emails in the file */
    flag = 0;
    int num_hash = 0;
    int num_mails = 0;
    long j = 0;
    while(mail_spool_buffer[j])
    {   
        if(mail_spool_buffer[j] == '#')
        {
            flag = 1;
            num_hash++;
            if(num_hash == 3)
            {
                flag = 0;
                num_hash = 0;
                num_mails++;
            }
        }
        else if(mail_spool_buffer[j] != '#' && flag == 1)
        {
            num_hash = 0;
            flag = 0;
        }
        j++;
    }

    if(num_mails > 0)
    {
        current_mail_pointer[current_username_index] = 0;
    }

    /* close the file */
    fclose(fp);

    /* free the memory we used for the buffer */
    free(mail_spool_buffer);

    return num_mails;
}

/* 
Returns NULL if the current user's mail pointer is invalid or if the current user itself is not set (Also in case of malloc or file handling errors)
If not returns a pointer to a string containing the current mail (as pointed to by the current mail pointer). Current mail pointer is incremented to the next mail in the spool file (or loops back to the start of the spool file).
Note allocates memory on the heap, deallocating/freeing is caller's responsibility. 
*/ 
char* do_readm()
{
    if((current_username_index == -1) || (current_mail_pointer[current_username_index] == -1))
    {
        return NULL;
    }
    int current_byte_index = current_mail_pointer[current_username_index];

    /* open the corresponding spool file for reading */
    char* filename = make_filename_path(list_active_users[current_username_index]);
    FILE* fp = fopen(filename, "r");
    free(filename);
    char* mail_spool_buffer;
    long len_mail_spool;
    
    /* quit if the file does not exist */
    if(fp == NULL)
    {
        printf("file error\n");
        return NULL;
    }
    
    /* Get the number of bytes */
    fseek(fp, 0L, SEEK_END);
    len_mail_spool = ftell(fp);

    
    /* reset the file position indicator to 
    the beginning of the file */
    fseek(fp, 0L, SEEK_SET);	
    
    /* grab sufficient memory for the 
    buffer to hold the text */
    mail_spool_buffer = (char*)calloc(len_mail_spool + 1, sizeof(char));	
    
    /* memory error */
    if(mail_spool_buffer == NULL)
    {
        printf("malloc error\n");
        return NULL;
    }
    
    /* copy all the text into the buffer */
    fread(mail_spool_buffer, sizeof(char), len_mail_spool, fp);

    /* while loop section to perform the desired behaviour */
    int flag = 0;
    int num_hash = 0;
    long len_mail = 0;
    long i = current_byte_index;

    while(mail_spool_buffer[i])
    {   
        if(mail_spool_buffer[i] == '#')
        {
            flag = 1;
            num_hash++;
            if(num_hash == 3)
            {
                flag = 0;
                num_hash = 0;
                len_mail = i - current_byte_index - 1;
                if(i != len_mail_spool - 1)
                {
                    current_mail_pointer[current_username_index] = i + 1;
                }
                else 
                {
                    current_mail_pointer[current_username_index] = 0;
                }
                break;
            }
        }
        else if(mail_spool_buffer[i] != '#' && flag == 1)
        {
            num_hash = 0;
            flag = 0;
        }
        i++;
    }

    char* mail_string = (char*)calloc(len_mail, sizeof(char));
    memcpy(mail_string, mail_spool_buffer + current_byte_index, len_mail - 1);

    /* close the file */
    fclose(fp);

    /* free the memory we used for the buffer */
    free(mail_spool_buffer);

    return mail_string;
}

/*
Returns 0 if the current user's mail pointer is invalid or if the current user itself is not set. 
In case of malloc or file errors, returns -1
If not deletes the email pointed to by the current mail pointer. The current mail pointer is made to point to the next email (or loops back to starting email if it exists).
Return 1 on a successful delete.
*/
int do_delm()
{
    if((current_username_index == -1) || (current_mail_pointer[current_username_index] == -1))
    {
        return 0;
    }
    int current_byte_index = current_mail_pointer[current_username_index];

    /* open the corresponding spool file for reading */
    char* filename = make_filename_path(list_active_users[current_username_index]);
    FILE* fp = fopen(filename, "r");
    char* mail_spool_buffer;
    long len_mail_spool;
    
    /* quit if the file does not exist */
    if(fp == NULL)
    {
        printf("file error\n");
        return -1;
    }
    
    /* Get the number of bytes */
    fseek(fp, 0L, SEEK_END);
    len_mail_spool = ftell(fp);

    /* reset the file position indicator to 
    the beginning of the file */
    fseek(fp, 0L, SEEK_SET);	
    
    /* grab sufficient memory for the 
    buffer to hold the text */
    mail_spool_buffer = (char*)calloc(len_mail_spool + 1, sizeof(char));	
    
    /* memory error */
    if(mail_spool_buffer == NULL)
    {
        printf("malloc error\n");
        return -1;
    }
    
    /* copy all the text into the buffer */
    fread(mail_spool_buffer, sizeof(char), len_mail_spool, fp);
    fclose(fp);

    char* new_mail_spool_buffer = (char*)calloc(len_mail_spool + 1, sizeof(char));

    /* while loop section to perform the desired behaviour */
    int flag = 0;
    int num_hash = 0;
    long len_new_mail_spool = 0;
    long i = current_byte_index;

    while(mail_spool_buffer[i])
    {   
        if(mail_spool_buffer[i] == '#')
        {
            flag = 1;
            num_hash++;
            if(num_hash == 3)
            {
                flag = 0;
                num_hash = 0;
                len_new_mail_spool = len_mail_spool - (i - current_byte_index + 1);
                i++;
                break;
            }
        }
        else if(mail_spool_buffer[i] != '#' && flag == 1)
        {
            num_hash = 0;
            flag = 0;
        }
        i++;
    }
    
    int j = 0;
    while(j < current_byte_index)
    {
        new_mail_spool_buffer[j] = mail_spool_buffer[j];
        j++;
    }

    long saved_j = j;
    long saved_i = i;
    while(i < len_mail_spool)
    {
        new_mail_spool_buffer[j] = mail_spool_buffer[i];
        i++;
        j++;
    }

    /* open the corresponding spool file for writing */
    /* Note : Also clears the old file */ 
    fp = fopen(filename, "w");
    free(filename);
    if(fp == NULL)
    {
        printf("file error\n");
        return -1;
    }

    if(len_new_mail_spool != 0)
    {
        fwrite(new_mail_spool_buffer, sizeof(char), len_new_mail_spool, fp);
        current_mail_pointer[current_username_index] = (saved_i != len_mail_spool) ? saved_j : 0;
    }
    else
    {
        current_mail_pointer[current_username_index] = -1;
    }
    free(new_mail_spool_buffer);
    free(mail_spool_buffer);
    fclose(fp);
    return 1;
}

/*
If the username does not exist, returns 0, -1 in case of malloc or file errors 
In case of a valid username, appends a new email to the recipient's mail spool file and returns 1. 
Note that the message sent as an argument must contain the timestamp, subject and the content entries. The message sent as argument must also be terminated with ###. 
The from and to entries will be added by this function. 
*/
int do_sendm(char* username, char* message)
{
    int flag = 0;
    int i;
    for(i = 0; i < MAX_USERS; i++)
    {
        if(strcmp(list_active_users[i], username) == 0)
        {
            flag = 1;
            break;
        }
    }
    if(flag == 0)
    {
        return 0;   // username does not exist
    }

    char* filename = make_filename_path(username);
    char* from_string = make_from_entry(list_active_users[current_username_index]);
    char* to_string = make_to_entry(username);
    char* full_message = (char*)calloc(strlen(from_string) + strlen(to_string) + strlen(message) + 1, sizeof(char));
    strcat(full_message, from_string);
    strcat(full_message, to_string);
    strcat(full_message, message);

    /* open the file of the recipient for appending */
    FILE* fp = fopen(filename, "a+");

    if(fp == NULL)
    {
        printf("file error\n");
        return -1;
    }

    /* append the message to the end of the recipient's mail spool file */
    fputs(full_message, fp);

    if((current_mail_pointer[current_username_index] == -1) && (strcmp(username, list_active_users[current_username_index]) == 0))
    {
        current_mail_pointer[current_username_index] = 0;
    }
    fclose(fp);
    free(full_message);
    free(filename);
    free(from_string);
    free(to_string);
    return 1; 
}

/* 
Makes the current mail pointer of the current user and the current user both invalid by setting them to -1 
*/ 
void do_doneu()
{
    current_mail_pointer[current_username_index] = -1;
    current_username_index = -1;
}

/* 
Network Interface : Communicates with the client
*/
void network_interface(int socket_fd)
{
    /* char length_string[10]; */
    char* message = (char*)calloc(MAX_PASSED_MESSAGE_LEN, sizeof(char));
    while(1)
    {
        // Debugging
        memset(message, 0, MAX_PASSED_MESSAGE_LEN);
        read(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
        if(strcmp(message, "LSTU") == 0)
        {
            char* list_of_users = do_lstu();
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            if(strlen(list_of_users) == 0)
            {
                strcpy(message, "No users");
            }
            else
            {
                strcpy(message, list_of_users);
            }
            free(list_of_users);
        }
        else if(strncmp(message, "ADDU", 4) == 0)
        {
            char* username = (char*)calloc(MAX_USERNAME_LENGTH, sizeof(char));
            char* password = (char*)calloc(PASSWORD_LENGTH, sizeof(char));
            char* token = strtok(message + 5, " ");
            strcpy(username, token);
            token = strtok(NULL, " ");
            strcpy(password, token);
            int status = do_addu(username, password);
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            if(status == 1)
            {
                strcpy(message, "Userid successfully added. Password has been set");
            }
            else if(status == 0) 
            {
                strcpy(message, "Userid already exists");
            }
            free(username);
            free(password);
            /* status = -1 implies file creation error */
        }
        else if(strncmp(message, "USER", 4) == 0)
        {
            char* username = (char*)calloc(MAX_USERNAME_LENGTH, sizeof(char));
            char* password = (char*)calloc(PASSWORD_LENGTH, sizeof(char));
            char* token = strtok(message + 5, " ");
            strcpy(username, token);
            token = strtok(NULL, " ");
            strcpy(password, token);
            char num_string[10] = {};
            int n_mails = do_user(username, password);
            free(username);
            free(password);
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            if(n_mails >= 0)
            {
                sprintf(num_string, "%d", n_mails);
                strcpy(message, "Specified user exists. User has been successfully set, ");
                strcat(message, num_string);
                strcat(message, " mails");
            }
            else if(n_mails == -1)
            {
                strcpy(message, "Specified user does not exist");
            }
            else if(n_mails == -2)
            {
                strcpy(message, "Authentication failure. Incorrect Password");
            }
            /* -3 in case of file or malloc issues */
        }
        else if(strcmp(message, "READM") == 0)
        {
            char* returned_message = do_readm();
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            if(returned_message == NULL)
            {
                strcpy(message, "No More Mail");
            }
            else 
            {
                strcpy(message, returned_message);
            }
            free(returned_message);
        }
        else if(strcmp(message, "DELM") == 0)
        {
            int status = do_delm();
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            if(status == 1)
            {
                strcpy(message, "Message Deleted");
            }
            else if(status == 0)
            {
                strcpy(message, "No More Mail");
            }
            /* -1 in case of file or malloc issues */
        }
        else if(strncmp(message, "SEND", 4) == 0)
        {
            time_t t;   
            time(&t);

            char* token = strtok(message + 5, " ");
            char* username = (char*)calloc(MAX_USERNAME_LENGTH, sizeof(char));
            strcpy(username, token);

            token = strtok(NULL, "\n");

            char* date_time = ctime(&t);
            char* date_string = (char*)calloc(DATE_LENGTH, sizeof(char));
            strcpy(date_string, "Date: ");
            strcat(date_string, date_time);

            char* subject = (char*)calloc(MAX_SUBJECT_LEN, sizeof(char));
            strcpy(subject, "Subject: ");
            strcat(subject, token);
            strcat(subject, "\n");

            token += 1 + strlen(token);
            char* content = (char*)calloc(MAX_CONTENT_LEN, sizeof(char));
            strcpy(content, "Content: ");
            strcat(content, token);

            char* full_message = (char*)calloc(MAX_TOTAL_MESSAGE_LEN, sizeof(char));
            strcpy(full_message, date_string);
            strcat(full_message, subject);
            strcat(full_message, content);

            int status = do_sendm(username, full_message);
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            if(status == 1)
            {
                strcat(message, "Email Sent");
            }
            else if(status == 0)
            {
                strcat(message, "Recipient Userid does not exist");
            }
            /* -1 in case of malloc or file errors */

            free(content);
            free(full_message);
            free(date_string);
            free(subject);
            free(username);
        }
        else if(strcmp(message, "DONEU") == 0)
        {
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            do_doneu();
            strcpy(message, "Done");
        }
        else if(strcmp(message, "QUIT") == 0)
        {   
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            strcpy(message, "Closing connection with server");
            write(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
            break;
        }   
        else
        {
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            strcpy(message, "Invalid Format");
        }
        write(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
    }
    free(message);
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        printf("Usage : \n./emailserver <PORT_NUMBER>\n");
        exit(1);
    }
    init_server();

    int port = atoi(argv[1]);
    int socket_fd, conn_fd;
    int opt = 1;
    struct sockaddr_in server_addr, cli;

    /* socket creation and verification */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
        printf("socket creation failure\n");
        exit(0);
    }

    /* Forcefully attach socket to port */
    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) 
    { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    }

    /* assign IP and PORT */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    /* binding newly created socket to given IP and verification */
    if((bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr))) != 0)
    {
        printf("socket bind failure\n");
        exit(0);
    }

    if((listen(socket_fd, 5)) != 0)
    {
        printf("listen failure\n");
        exit(0);
    }

    int len = sizeof(cli);
    while(1)
    {
        conn_fd = accept(socket_fd, (sockaddr*)&cli, &len);
        if(conn_fd < 0)
        {
            printf("server accept failure\n");
            exit(0);
        }

        network_interface(conn_fd);

        close(conn_fd);
    }

    close(socket_fd);

    /*
    // Some testing code for the server command processor
    //do_addu("Shreeshappa");
    //do_addu("Madappa");
    printf("%s\n", do_lstu());
    do_user("Madappa");
    do_sendm("Shreeshappa", "Enappa? Hengidya###");
    do_doneu();
    do_user("Shreeshappa");
    do_sendm("Madappa", "Enilappa! Aramiddini! Nev hege??###");
    do_doneu();
    */
    return 0;
}