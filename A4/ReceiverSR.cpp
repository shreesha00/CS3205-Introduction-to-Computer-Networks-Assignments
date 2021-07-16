#include<bits/stdc++.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<random>
#include<thread> 
#include<unistd.h>
#define packet_length 2000
using namespace std;

bool debug_mode = false;
int max_packets;
double drop_probab;
int num_packets;
int receiver_port_num;
int ack_expected;
int sender_window_size;
int max_buffer_size;
int seq_bits;
struct packet_info
{
    double time_received;  
    char* data;
};

map<int, packet_info*> buffered_packets;
set<int> buffered_seq_num;

int main(int argc, char** argv)
{
    if((argc != 13) && (argc != 14))
    {
        cout<<"Usage :\n./ReceiverSR ­-d -p <receiver_port_num> -N <max_packets> -n <sequence_number_bits> -W <sender_window_size> -B <max_buffer_size> -e <packet_error_rate> "<<endl;
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
		else if((string)argv[i] == "-N")
		{
			i++;
			max_packets = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-n")
		{
			i++;
			seq_bits = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-W")
		{
			i++;
			sender_window_size = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-B")
		{
			i++;
			max_buffer_size = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-e")
		{
			i++;
			drop_probab = atof(argv[i]);
            flag++;
		}
		else 
		{
			cout<<"unidentified flag "<<"\'"<<argv[i]<<"\'"<<endl;
			cout<<"Usage :\n./ReceiverSR ­-d -p <receiver_port_num> -N <max_packets> -n <sequence_number_bits> -W <sender_window_size> -B <max_buffer_size> -e <packet_error_rate> "<<endl;
            cout<<"The debug flag \'-d\' is optional. If not provided, DEBUG mode is off"<<endl;
            cout<<"Rest of the parameters are compulsory and must be provided"<<endl;
            return(0);
		}
	}
    
    if(flag != 6)
    {
        cout<<"all necessary parameters have not been supplied"<<endl;
        cout<<"Usage :\n./ReceiverSR ­-d -p <receiver_port_num> -N <max_packets> -n <sequence_number_bits> -W <sender_window_size> -B <max_buffer_size> -e <packet_error_rate> "<<endl;
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
                ack_expected = (ack_expected + 1) % ((int)pow(2, seq_bits));
                sprintf(packet, "%d", ack);
                sendto(socket_fd, (const char *)packet, strlen(packet), MSG_CONFIRM,(const struct sockaddr *)&cliaddr, len);
                num_packets++;
                if(debug_mode)
                {
                    cout<<"Seq #: "<<ack<<" Time Received: "<<milliseconds<<endl;
                }
                int i = ack_expected;
                while(buffered_seq_num.find(i) != buffered_seq_num.end())
                {
                    packet_info* pckt_info = buffered_packets[i];
                    buffered_seq_num.erase(i);
                    buffered_packets.erase(i);
                    cout<<"Seq #: "<<i<<" Time Received: "<<pckt_info->time_received<<endl;

                    num_packets++;
                    if(num_packets >= max_packets)
                    {
                        break;
                    }

                    i = (i + 1) % ((int)pow(2, seq_bits));
                }
                ack_expected = i;
                if(num_packets >= max_packets)
                {
                    break;
                }
            }
            else if(ack > ack_expected)
            {
                if(buffered_packets.size() < sender_window_size)
                {
                    /* Store Frame */
                    packet_info* new_packet_info = new packet_info;
                    char* data = NULL; /* Reality, store the data */
                    new_packet_info->time_received = milliseconds;
                    buffered_packets[ack] = new_packet_info;
                    buffered_seq_num.insert(ack);

                    /* send acknowledgement */
                    sprintf(packet, "%d", ack);
                    sendto(socket_fd, (const char *)packet, strlen(packet), MSG_CONFIRM,(const struct sockaddr *)&cliaddr, len);
                }
            }
        }

        
    }
    return 0;
}


