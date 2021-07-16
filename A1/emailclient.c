#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <regex.h>
#include <dirent.h>
#include <arpa/inet.h>

#define HELP_FILE_LENGTH 1000
#define MAX_PROMPT_INPUT_LENGTH 500
#define MAX_PROMPT_OUTPUT_LENGTH 500
#define MAX_PASSED_MESSAGE_LEN 1000
#define MAX_SUBJECT_LEN 200
#define MAX_CONTENT_LEN 800
#define MAX_USERNAME_LENGTH 101
#define MAX_TOTAL_MESSAGE_LEN 1200
#define MAX_PROMPT_INPUT_WORDS 2
#define MAX_INPUT_WORD_LENGTH 100
#define PASSWORD_LENGTH 100
#define sockaddr struct sockaddr
#define PORT 8080

char user_help[HELP_FILE_LENGTH];
char main_help[HELP_FILE_LENGTH];

void network_interface(int socket_fd)
{
    char* prompt_input_word = (char*)calloc(MAX_INPUT_WORD_LENGTH, sizeof(char));
    char* message = (char*)calloc(MAX_PASSED_MESSAGE_LEN, sizeof(char));
    while(1)
    {
        printf("%s", "MainPrompt> ");
        scanf("%s", prompt_input_word);
        memset(message, 0, MAX_PASSED_MESSAGE_LEN);
        if(strcmp(prompt_input_word, "Listusers") == 0)
        {
            strcpy(message, "LSTU");
        }
        else if(strcmp(prompt_input_word, "Adduser") == 0)
        {
            char* password = (char*)calloc(PASSWORD_LENGTH, sizeof(char));
            scanf("%s", prompt_input_word);
            scanf("%s", password);
            strcpy(message, "ADDU ");
            strcat(message, prompt_input_word);
            strcat(message, " ");
            strcat(message, password);
            free(password);
        }
        else if(strcmp(prompt_input_word, "SetUser") == 0)
        {
            char* username = (char*)calloc(MAX_USERNAME_LENGTH, sizeof(char));
            scanf("%s", prompt_input_word);
            strcpy(username, prompt_input_word);
            scanf("%s", prompt_input_word);
            strcpy(message, "USER ");
            strcat(message, username);
            strcat(message, " ");
            strcat(message, prompt_input_word);
            write(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            read(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
            printf("%s\n", message);
            if(!(strncmp(message, "Specified user exists", 21) == 0))
            {
                continue;
            }
            while(1)
            {
                printf("Sub-Prompt-%s> ", username);
                scanf("%s", prompt_input_word);
                memset(message, 0, MAX_PASSED_MESSAGE_LEN);
                if(strcmp(prompt_input_word, "Read") == 0)
                {
                    strcpy(message, "READM");
                }
                else if(strcmp(prompt_input_word, "Delete") == 0)
                {
                    strcpy(message, "DELM");
                }
                else if(strcmp(prompt_input_word, "Send") == 0)
                {
                    scanf("%s", prompt_input_word);
                    getc(stdin);
                    char* subject = (char*)calloc(MAX_SUBJECT_LEN, sizeof(char));
                    printf("Type Subject: ");
                    fgets(subject, MAX_SUBJECT_LEN, stdin);
                    char* content = (char*)calloc(MAX_CONTENT_LEN, sizeof(char));
                    printf("Type Message: ");
                    int flag = 0;
                    int num_hash = 0;
                    char c;
                    int i = 0;
                    while(1)
                    {   
                        c = getc(stdin);
                        content[i] = c;
                        if(c == '#')
                        {
                            flag = 1;
                            num_hash++;
                            if(num_hash == 3)
                            {
                                flag = 0;
                                num_hash = 0;
                                break;
                            }
                        }
                        else if(c != '#' && flag == 1)
                        {
                            num_hash = 0;
                            flag = 0;
                        }
                        i++;
                    }
                    getc(stdin); // for the newline
                    strcpy(message, "SEND ");
                    strcat(message, prompt_input_word);
                    strcat(message, " ");
                    strcat(message, subject);
                    strcat(message, content);
                    free(subject);
                    free(content);
                }
                else if(strcmp(prompt_input_word, "Done") == 0)
                {
                    strcpy(message, "DONEU");
                }
                else if(strcmp(prompt_input_word, "help") == 0)
                {
                    printf("%s", user_help);
                    continue;
                }
                else
                {
                    printf("Invalid Command %s\nUse \"help\" for allowed commands\n", prompt_input_word);
                    continue;
                }
                write(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
                memset(message, 0, MAX_PASSED_MESSAGE_LEN);
                read(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
                printf("%s\n", message);
                if((strcmp(prompt_input_word, "Done") == 0) && (strcmp(message, "Done") == 0))
                {
                    free(username);
                    break;
                }
            }
            continue;
        }
        else if(strcmp(prompt_input_word, "Quit") == 0)
        {
            strcpy(message, "QUIT");
            write(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
            memset(message, 0, MAX_PASSED_MESSAGE_LEN);
            read(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
            printf("%s\n", message);
            break;
        }
        else if(strcmp(prompt_input_word, "help") == 0)
        {
            printf("%s", main_help);
            continue;
        }
        else
        {
            printf("Invalid Command %s\nUse \"help\" for allowed commands\n", prompt_input_word);
            continue;
        }
        write(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
        memset(message, 0, MAX_PASSED_MESSAGE_LEN);
        read(socket_fd, message, MAX_PASSED_MESSAGE_LEN);
        printf("%s\n", message);
    }
    free(prompt_input_word);
    free(message);
}

int main(int argc, char** argv)
{
    FILE* fp = fopen("Help_Text_Files/main_help.txt", "r");
    fread(main_help, sizeof(char), HELP_FILE_LENGTH, fp);
    fclose(fp);

    fp = fopen("Help_Text_Files/user_help.txt", "r");
    fread(user_help, sizeof(char), HELP_FILE_LENGTH, fp);
    fclose(fp);
    
    if(argc != 3)
    {
        printf("Usage : \n./emailclient <SERVER_IP> <SERVER_PORT_NUMBER>\n");
        exit(1);
    }

    int socket_fd, conn_fd;
    struct sockaddr_in server_addr, cli;

    int port = atoi(argv[2]);
    /* socket creation and verification */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1)
    {
        printf("socket creation failure\n");
        exit(0);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(port);

    if(connect(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) != 0)
    {
        printf("connection failure with server\n");
        exit(0);
    }

    network_interface(socket_fd);
    
    close(socket_fd);
    return 0;
}