#include<bits/stdc++.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<random>
#include<thread> 
#include<unistd.h>
#define packet_length 2000
#define SEQ_NUM 64
using namespace std;

time_t time_init;
bool debug_mode = false;
int max_packets;
float drop_probab;
int num_packets;
int receiver_port_num;
int ack_expected;
int sender_window_size;


int main(int argc, char** argv)
{
    if((argc != 10) && (argc != 11))
    {
        cout<<"Usage :\n./ReceiverGBN ­-d -p <receiver_port_num> -n <max_packets> -w <sender_window_size> -e <packet_error_rate>"<<endl;
        cout<<"The debug flag \'-d\' is optional. If not provided, DEBUG mode is off"<<endl;
        cout<<"Rest of the parameters are compulsory and must be provided"<<endl;
        exit(0);
    }

    int flag = 0;
	for(int i = 1; i < argc; i++)
	{
		if((string)argv[i] == "-d")
		{
			debug_mode = true;
		}
		else if((string)argv[i] == "-p")
		{
			i++;
			receiver_port_num = atoi(argv[i]);
			flag++;
		}
		else if((string)argv[i] == "-n")
		{
			i++;
			max_packets = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-e")
		{
			i++;
			drop_probab = atof(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-w")
		{
			i++;
			sender_window_size = atof(argv[i]);
            flag++;
		}
		else 
		{
			cout<<"unidentified flag "<<"\'"<<argv[i]<<"\'"<<endl;
			cout<<"Usage :\n./ReceiverGBN ­-d -p <receiver_port_num> -n <max_packets> -w <sender_window_size> -e <packet_error_rate>"<<endl;
            cout<<"The debug flag \'-d\' is optional. If not provided, DEBUG mode is off"<<endl;
            cout<<"Rest of the parameters are compulsory and must be provided"<<endl;
            return(0);
		}
	}
    
    if(flag != 4)
    {
        cout<<"all necessary parameters have not been supplied"<<endl;
        cout<<"Usage :\n./ReceiverGBN ­-d -p <receiver_port_num> -n <max_packets> -w <sender_window_size> -e <packet_error_rate>"<<endl;
        cout<<"The debug flag \'-d\' is optional. If not provided, DEBUG mode is off"<<endl;
        cout<<"Rest of the parameters are compulsory and must be provided"<<endl;
        return(0);
    }

    mt19937 generator(0);
    bernoulli_distribution distribution(drop_probab);

    char packet[packet_length];

    int socket_fd;
    struct sockaddr_in servaddr, cliaddr;
    
    // Creating socket file descriptor
    if((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        cout<<"socket creation failure"<<endl;
        return(0);
    }
      
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
      
    // Filling server information
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(receiver_port_num);
      
    // Bind the socket with the server address
    if(bind(socket_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        cout<<"bind failure"<<endl;
        return(0);
    }
      
    int n;
    socklen_t len;
  
    len = sizeof(cliaddr);  


    int i = 0;
    chrono::time_point<chrono::high_resolution_clock> time_start;
    while(1)
    {
        memset(&packet, 0, packet_length);
        n = recvfrom(socket_fd, (char *)packet, packet_length, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        packet[n] = '\0';
        bool dropped = distribution(generator);


        string a = dropped == true ? "true" : "false";

        auto time_received = chrono::high_resolution_clock::now();

        if(i == 0)
        {
            time_start = time_received;
            i = 1;
        }

        char* token = strtok(packet, " ");
        int ack = atoi(token);
		int microseconds = chrono::duration_cast<chrono::microseconds>(time_received - time_start).count();
		double milliseconds = microseconds/1000;
        if(!dropped)
        {
            if(ack == ack_expected)
            {
                ack_expected = (ack_expected + 1) % (SEQ_NUM);
                sprintf(packet, "%d", ack);
                sendto(socket_fd, (const char *)packet, strlen(packet), MSG_CONFIRM,(const struct sockaddr *)&cliaddr, len);
                num_packets++;
                if(num_packets >= max_packets)
                {
                    if(debug_mode)
                    {
                        cout<<"Seq #: "<<ack<<" Time Received: "<<milliseconds<<" Packet dropped: "<<a<<endl;
                    }
                    break;
                }
            }
        }

        if(debug_mode)
        {
            cout<<"Seq #: "<<ack<<" Time Received: "<<milliseconds<<" Packet dropped: "<<a<<endl;
        }
    }
    return 0;
}


