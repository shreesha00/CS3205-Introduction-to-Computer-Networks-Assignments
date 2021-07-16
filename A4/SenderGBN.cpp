#include<bits/stdc++.h>
#include<chrono>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<random>
#include<thread> 
#include<unistd.h>
#include<future>
#define TIMEOUT_MULT 8
#define SEQ_NUM 64
#define PADDING_LENGTH 1000
using namespace std;

class Timer 
{
public:
    Timer()
	{
		/* Do nothing */;
    }
    void reset() {
		m_beg = clock_::now();
        m_start = m_beg;
    }

	void reset_with_tracker() {
		m_beg = clock_::now();
	}

    double elapsed() const {
        return chrono::duration_cast<chrono::microseconds>(
                clock_::now() - m_beg).count();
    }

	double elapsed_start() const {
		return chrono::duration_cast<chrono::microseconds>(
                clock_::now() - m_start).count();
	}


private:
    typedef chrono::high_resolution_clock clock_;
    typedef chrono::duration<double, std::ratio<1> > second_;
    chrono::time_point<clock_> m_beg;
	chrono::time_point<clock_> m_start;
};

struct packet 
{
	int seq; 
	int packet_num;
	char* data;
};

struct generated_packet
{
	int num_tries;
	chrono::time_point<chrono::high_resolution_clock> time_gen;
};


chrono::time_point<chrono::high_resolution_clock> time_start;
int packet_length;			// length of each packet in bytes
int packet_gen_rate;		// rate of packet generation in packets per second
int max_packets;			// maximum number of packets to be acknowledged after which termination occurs
int window_size;			// window size in units of packets
int max_buffer_size;		// maximum buffer size in units of packets
bool debug_mode = false;	// set when debug mode on
string receiver_IP;			// receiver IP address
int receiver_port_num;		// receiver port number
int num_sent;				// number of transmissions

map<int, Timer*> timers; 	// timer map for each sequence number

mutex thread_exit_mutex;	// mutex for thread exit
bool thread_exit; 			// variable to exit

float timeout;				// timeout value in milliseconds
float RTT;					// round trip time
int num_acked;				// total number of acknowledged packets 
int current_packet_num;		// current packet number 
map<int, generated_packet*> generated_packets;	// number of tries for each packet

mutex unacked_mutex;		// mutex for queue of unacked packets
vector<struct packet*> unacked;	// vector of unacked packets


map<int, int> retransmissions; 	// number of retransmissions for each sequence number
int current_seq_num;		// current sequence number to send
							// can be between 0 to window_size

queue<char*> buffer;		// sender buffer
mutex buffer_mutex;			// mutex for sender buffer

/* random integer generator between a and b */
int random_int_between(int& a, int& b)
{
	return a + rand()%(b - a + 1);
}

void fill_random_chars(char* str, int length)
{
	for(int i = 0; i < length; i++)
    {
		str[i] = 'A' + rand() % 26;
	}
	str[length - 1] = (char)0;
}

void packet_generator()
{
	int i = 0;
    while(1)
    {
        usleep(1.0e6/packet_gen_rate);
        
        unique_lock<mutex> lock(buffer_mutex);

        if(buffer.size() == max_buffer_size)
        {
            lock.unlock();
            continue;
        }

		if(i == 0)
		{
			time_start = chrono::high_resolution_clock::now();
		}
		/* create a new packet */
		char* string = (char*)malloc(packet_length * sizeof(char));
        fill_random_chars(string, packet_length);

		/* place it into the buffer */
		buffer.push(string);

		generated_packets[i] = new generated_packet;
		generated_packets[i]->time_gen = chrono::high_resolution_clock::now();
		generated_packets[i]->num_tries = 0;

		lock.unlock();

		unique_lock<mutex> exit_lock(thread_exit_mutex);
		if(thread_exit == 1)
		{
			exit_lock.unlock();
			break;
		}
		exit_lock.unlock();
		i++;
    }
}

void sender(int socket_fd)
{
	char padding[PADDING_LENGTH];
	char message[packet_length];
	int sent_seq_num;
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
      
	/* TODO : Fix this for communication to other systems etc */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(receiver_port_num);
    server_addr.sin_addr.s_addr = inet_addr(receiver_IP.c_str());

	while(1)
	{
		unique_lock<mutex> unacked_lock(unacked_mutex);

		/* check for timeout */
		if(unacked.size() > 0)
		{
			struct packet* temp_packet = unacked.front();
			float curr_timeout = num_acked > 10 ? TIMEOUT_MULT * RTT : 100 * 1000; 
			if(timers[temp_packet->seq]->elapsed() >= curr_timeout)
			{
				for(auto ele : unacked)
				{
					generated_packets[ele->packet_num]->num_tries++;
					retransmissions[ele->seq]++;
					strcpy(message, ele->data);
					num_sent++;
					sendto(socket_fd, (const char *)message, packet_length, MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
					timers[ele->seq]->reset_with_tracker();
				}
			}
		}

		bool flag = 0;
		for(auto ele : retransmissions)
		{
			if(ele.second > 5)
			{
				flag = 1;
			}
		}
		if(flag == 1)
		{
			unique_lock<mutex> exit_lock(thread_exit_mutex);
			thread_exit = 1;
			exit_lock.unlock();
			unacked_lock.unlock();
			break;
		}

		/* check if window available to send fresh packets */
		if(unacked.size() == window_size)
		{
			unacked_lock.unlock();
			continue;
		}
		
		memset(&message, 0, packet_length);

		char* data = NULL;
		unique_lock<mutex> buffer_lock(buffer_mutex);
		if(!buffer.empty())
		{
			data = buffer.front();
			buffer.pop();
		}
		buffer_lock.unlock();
		
		if(data != NULL)
		{
			sprintf(message, "%d", current_seq_num);
			sent_seq_num = current_seq_num;
			current_seq_num = (current_seq_num + 1) % (SEQ_NUM);

			strcat(message, " ");
			strcat(message, data);
			message[packet_length - 1] = (char)0;

			free(data);

			/* send frame to receiver */
			num_sent++;
			sendto(socket_fd, (const char *)message, packet_length, MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));

			generated_packets[current_packet_num]->num_tries = 1;

			/* start timer for the sequence number */
			timers[sent_seq_num]->reset();

			/* store a copy of the frame in case of retransmissions */
			struct packet* stored_packet = (struct packet*)malloc(sizeof(struct packet));
			char* stored_data = (char*)malloc(sizeof(char) * packet_length);
			strcpy(stored_data, message);
			stored_packet->data = stored_data;
			stored_packet->seq = sent_seq_num;
			stored_packet->packet_num = current_packet_num;
			unacked.push_back(stored_packet);
			current_packet_num++;
		}
		unacked_lock.unlock();

		unique_lock<mutex> thread_exit_lock(thread_exit_mutex);
		if(thread_exit == 1)
		{
			thread_exit_lock.unlock();
			break;
		}
		thread_exit_lock.unlock();
	}
}

int main(int argc, char** argv)
{
    if((argc != 15) && (argc != 16))
    {
        cout<<"Usage :\n./SenderGBN ­-d -s <receiver_IP> -p <receiver_port_num> -l <packet_length> -r <packet_gen_rate> -n <max_packets> -w <window_size> -b <max_buffer_size>"<<endl;
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
		else if((string)argv[i] == "-s")
		{
            i++;
            receiver_IP = string(argv[i]);
            flag++;
		}
		else if((string)argv[i] == "-p")
		{
			i++;
			receiver_port_num = atoi(argv[i]);
			flag++;
		}
		else if((string)argv[i] == "-l")
		{
			i++;
			packet_length = atoi(argv[i]);
            flag++;
		}
		else if((string)argv[i] == "-r")
		{
			i++;
			packet_gen_rate = atoi(argv[i]);
            flag++;
		}
		else if((string)argv[i] == "-n")
		{
			i++;
			max_packets = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-w")
		{
			i++;
			window_size = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-b")
		{
			i++;
			max_buffer_size = atoi(argv[i]);
            flag++;
		}
		else 
		{
			cout<<"unidentified flag "<<"\'"<<argv[i]<<"\'"<<endl;
			cout<<"Usage :\n./SenderGBN ­-d -s <receiver_IP> -p <receiver_port_num> -l <packet_length> -r <packet_gen_rate> -n <max_packets> -w <window_size> -b <max_buffer_size>"<<endl;
            cout<<"The debug flag \'-d\' is optional. If not provided, DEBUG mode is off"<<endl;
            cout<<"Rest of the parameters are compulsory and must be provided"<<endl;
			return 0;
		}
	}
    
    if(flag != 7)
    {
        cout<<"all necessary parameters have not been supplied"<<endl;
        cout<<"Usage :\n./SenderGBN ­-d -s <receiver_IP> -p <receiver_port_num> -l <packet_length> -r <packet_gen_rate> -n <max_packets> -w <window_size> -b <max_buffer_size>"<<endl;
        cout<<"The debug flag \'-d\' is optional. If not provided, DEBUG mode is off"<<endl;
        cout<<"Rest of the parameters are compulsory and must be provided"<<endl;
        return 0;
    }


	int socket_fd;
  
    // creating a socket file descriptor
    if((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
        cout<<"socket creation failure"<<endl;
		return 0;
    }
	
    
	/* initialize timers */
	for(int i = 0; i < SEQ_NUM; i++)
	{
		Timer* timer = new Timer; 
		timers[i] = timer;
	}

	/* initialize retransmissions to 0 */
	for(int i = 0; i < SEQ_NUM; i++)
	{
		retransmissions[i] = 0;
	}

	thread packet_generator_thread(packet_generator);
	thread sender_thread(sender, socket_fd);

	int n;
	socklen_t len;
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
      
	/* TODO : Fix this for communication to other systems etc */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(receiver_port_num);
    server_addr.sin_addr.s_addr = inet_addr(receiver_IP.c_str());

	len = sizeof(server_addr);

	char recvd_msg[packet_length];
	while(1)
	{
		memset(&recvd_msg, 0, packet_length);
		n = recvfrom(socket_fd, (char *)recvd_msg, packet_length, MSG_WAITALL, (struct sockaddr*)&server_addr, &len);
		recvd_msg[n] = '\0';
	
		char* token = strtok(recvd_msg, " ");
		
		unique_lock<mutex> unacked_lock(unacked_mutex);
		struct packet* pckt = unacked.front();
		unacked.erase(unacked.begin());

		float RTT_current = timers[pckt->seq]->elapsed_start();
		chrono::time_point<chrono::high_resolution_clock> time_gen = generated_packets[pckt->packet_num]->time_gen;
		int microseconds = chrono::duration_cast<chrono::microseconds>(time_gen - time_start).count();
		double milliseconds = microseconds/1000;
		num_acked++;
		RTT += (RTT_current - RTT)/num_acked;

		if(debug_mode)
		{
			cout<<"Seq #: "<< atoi(token) <<" Time Generated: "<<milliseconds<<" RTT: "<<RTT_current<<" Number of Attempts: "<<generated_packets[pckt->packet_num]->num_tries<<endl;
		}

		free(pckt->data);
		free(pckt);


		if(num_acked >= max_packets)
		{
			unique_lock<mutex> exit_lock(thread_exit_mutex);
			thread_exit = 1; 
			exit_lock.unlock();
			unacked_lock.unlock();
			break; 
		}

		unique_lock<mutex> thread_exit_lock(thread_exit_mutex);
		if(thread_exit == 1)
		{
			thread_exit_lock.unlock();
			unacked_lock.unlock();
			break;
		}
	}
	packet_generator_thread.join();
	sender_thread.join();

	cout<<"Packet Generatation Rate: "<<packet_gen_rate<<" packets/sec"<<endl;
	cout<<"Packet Length: "<<packet_length<<" bytes"<<endl;
	cout<<"Retransmission Ratio "<<((double)num_sent)/num_acked<<endl;
	cout<<"Average RTT: "<<RTT<<" microseconds"<<endl;
    return 0;
}