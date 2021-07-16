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
#define TIMEOUT_MULT 12
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
        m_start = m_beg = clock_::now();
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


int uniform_between(int a, int b)
{
    return a + rand() % (b - a);
}

int seq_bits; 				// number of bits in sequence number
int max_packet_length;		// max length of each packet in bytes
int packet_gen_rate;		// rate of packet generation in packets per second
int max_packets;			// maximum number of packets to be acknowledged after which termination occurs
int window_size;			// window size in units of packets
int max_buffer_size;		// maximum buffer size in units of packets
bool debug_mode = false;	// set when debug mode on
string receiver_IP;			// receiver IP address
int receiver_port_num;		// receiver port number
int num_sent;				// number of transmissions


chrono::time_point<chrono::high_resolution_clock> time_start;
map<int, Timer*> timers; 	// timer map for each sequence number

mutex thread_exit_mutex;	// mutex for thread exit
bool thread_exit; 			// variable to exit

float timeout;				// timeout value in milliseconds
float RTT;					// round trip time
int num_acked;				// total number of acknowledged packets 
int current_packet_num;		// current packet number 
map<int, generated_packet*> generated_packets;	// number of tries for each packet


int unacked_length;			// number of non-null entries in unacked
mutex unacked_mutex;		// mutex for queue of unacked packets 
map<int, packet*> unacked;	// unacked packet of max window size from sequence number to packet

map<int, int> retransmissions; 	// number of retransmissions for each sequence number
int current_seq_num;		// current sequence number to send
							// can be between 0 to 2^(seq_bits) - 1

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
		int packet_length = uniform_between(40, max_packet_length);
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
	char message[max_packet_length];
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
		for(auto ele : unacked)
		{
			struct packet* temp_packet = ele.second;
			float curr_timeout = num_acked > 10 ? TIMEOUT_MULT * RTT : 300 * 1000; 
			if(timers[temp_packet->seq]->elapsed() >= curr_timeout)
			{
				generated_packets[temp_packet->packet_num]->num_tries++;
				retransmissions[temp_packet->seq]++;
				strcpy(message, temp_packet->data);
				num_sent++;
				sendto(socket_fd, (const char *)message, max_packet_length, MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
				timers[temp_packet->seq]->reset_with_tracker();
			}
		}
		
		bool flag = 0;
		for(auto ele : retransmissions)
		{
			if(ele.second > 10)
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
		if(unacked_length == window_size)
		{
			unacked_lock.unlock();
			continue;
		}
		
		memset(&message, 0, max_packet_length);

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
			current_seq_num = (current_seq_num + 1) % ((int)pow(2, seq_bits));

			strcat(message, " ");
			strcat(message, data);
			message[max_packet_length - 1] = (char)0;

			free(data);

			/* send frame to receiver */
			num_sent++;
			sendto(socket_fd, (const char *)message, max_packet_length, MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));

			generated_packets[current_packet_num]->num_tries = 1;

			/* start timer for the sequence number */
			timers[sent_seq_num]->reset();

			/* store a copy of the frame in case of retransmissions */
			struct packet* stored_packet = (struct packet*)malloc(sizeof(struct packet));
			char* stored_data = (char*)malloc(sizeof(char) * max_packet_length);
			strcpy(stored_data, message);
			stored_packet->data = stored_data;
			stored_packet->seq = sent_seq_num;
			stored_packet->packet_num = current_packet_num;

			unacked[sent_seq_num] = stored_packet;
			unacked_length++;
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
    if((argc != 18) && (argc != 17))
    {
        cout<<"Usage :\n./SenderSR ­-d -s <receiver_IP> -p <receiver_port_num> -n <sequence_number_bits> -L <max_packet_length> -R <packet_gen_rate> -N <max_packets> -W <window_size> -B <max_buffer_size>"<<endl;
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
		else if((string)argv[i] == "-n")
		{
			i++;
			seq_bits = atoi(argv[i]);
            flag++;
		}
		else if((string)argv[i] == "-L")
		{
			i++;
			max_packet_length = atoi(argv[i]);
            flag++;
		}
		else if((string)argv[i] == "-R")
		{
			i++;
			packet_gen_rate = atoi(argv[i]);
            flag++;
		}
		else if((string)argv[i] == "-N")
		{
			i++;
			max_packets = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-W")
		{
			i++;
			window_size = atoi(argv[i]);
            flag++;
		}
        else if((string)argv[i] == "-B")
		{
			i++;
			max_buffer_size = atoi(argv[i]);
            flag++;
		}
		else 
		{
			cout<<"unidentified flag "<<"\'"<<argv[i]<<"\'"<<endl;
			cout<<"Usage :\n./SenderSR ­-d -s <receiver_IP> -p <receiver_port_num> -n <sequence_number_bits> -L <max_packet_length> -R <packet_gen_rate> -N <max_packets> -W <window_size> -B <max_buffer_size>"<<endl;
            cout<<"The debug flag \'-d\' is optional. If not provided, DEBUG mode is off"<<endl;
            cout<<"Rest of the parameters are compulsory and must be provided"<<endl;
			return 0;
		}
	}
    
    if(flag != 8)
    {
        cout<<"all necessary parameters have not been supplied"<<endl;
        cout<<"Usage :\n./SenderSR ­-d -s <receiver_IP> -p <receiver_port_num> -n <sequence_number_bits> -L <max_packet_length> -R <packet_gen_rate> -N <max_packets> -W <window_size> -B <max_buffer_size>"<<endl;
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
	for(int i = 0; i < (int)pow(2, seq_bits); i++)
	{
		Timer* timer = new Timer; 
		timers[i] = timer;
	}

	/* initialize retransmissions to 0 */
	for(int i = 0; i < (int)pow(2, seq_bits); i++)
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

	char recvd_msg[max_packet_length];
	while(1)
	{
		memset(&recvd_msg, 0, max_packet_length);
		n = recvfrom(socket_fd, (char *)recvd_msg, max_packet_length, MSG_WAITALL, (struct sockaddr*)&server_addr, &len);
		recvd_msg[n] = '\0';
	
		char* token = strtok(recvd_msg, " ");
		
		unique_lock<mutex> unacked_lock(unacked_mutex);
		struct packet* pckt;
		pckt = unacked[atoi(token)];
		unacked.erase(atoi(token));
		unacked_length--;
		float RTT_current = timers[pckt->seq]->elapsed_start();
		RTT = (num_acked * RTT + RTT_current)/(num_acked + 1);
		chrono::time_point<chrono::high_resolution_clock> time_gen = generated_packets[pckt->packet_num]->time_gen;
		int microseconds = chrono::duration_cast<chrono::microseconds>(time_gen - time_start).count();
		double milliseconds = microseconds/1000;
		num_acked++;
		if(debug_mode)
		{
			cout<<"Seq #: "<< atoi(token) <<" Time Generated: "<<milliseconds<<" RTT: "<<RTT_current<<" Number of Attempts: "<<generated_packets[pckt->packet_num]->num_tries<<endl;
		}
		free(pckt->data);
		free(pckt);

		if(num_acked == max_packets)
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
	cout<<"Maximum Packet Length: "<<max_packet_length<<" bytes"<<endl;
	cout<<"Retransmission Ratio "<<((double)num_sent)/num_acked<<endl;
	cout<<"Average RTT: "<<RTT<<" microseconds"<<endl;
    return 0;
}