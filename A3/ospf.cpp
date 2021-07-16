#include<bits/stdc++.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<random>
#include<thread> 
#include<unistd.h>
using namespace std;

#define HI_DEF 1
#define LSAI_DEF 5
#define SPFI_DEF 20
#define MSG_SIZE 1000
#define RANDOM_SEED 3

class parameters
{
public : 
	int m_hi, m_lsai, m_spfi;
	parameters(int& hi, int& lsai, int& spfi)
    {
        m_hi = (hi == -1)? HI_DEF : hi; 
        m_lsai = (lsai == -1)? LSAI_DEF : lsai; 
        m_spfi = (spfi == -1)? SPFI_DEF : spfi; 
		cout<<"parameters set successfully"<<endl;
		cout<<"hi = "<<m_hi<<endl;
		cout<<"lsai = "<<m_lsai<<endl;
		cout<<"spfi = "<<m_spfi<<endl;
	}
};

class graph
{
public :
	int num_nodes;
	int num_edges;
	vector<int>* adj;
	mutex edge_weight_mutex;
	map<pair<int, int>, int> edge_weight;
	map<pair<int, int>, pair<int, int> > min_max_weights;
	graph(int& n, int& e, vector<pair<int, int> >& edgelist, vector<pair<int, int> >& weights)
	{
		num_nodes = n;
		num_edges = e;
		adj = new vector<int>[num_nodes];
		for(int i = 0; i < num_edges; i++)
		{
			adj[edgelist[i].first].push_back(edgelist[i].second);
			adj[edgelist[i].second].push_back(edgelist[i].first);
			pair<int, int> p;
			p.first = min(edgelist[i].first, edgelist[i].second);
			p.second = max(edgelist[i].first, edgelist[i].second);
			min_max_weights[p] = pair<int, int>(weights[i]);
		}
		edge_weight.clear();
	}
	
	/* function to get edge weight of a given edge */
	int get_edge_weight(int node1, int node2)
	{
		if(node1 > node2) swap(node1, node2); // guarentees node1 < node2

		/* sanity checks */
		if((find(adj[node1].begin(), adj[node1].end(), node2) == adj[node1].end()) || (find(adj[node2].begin(), adj[node2].end(), node1) == adj[node2].end()))
		{
			cout<<"edge does not exist between "<<node1<<" and "<<node2<<endl;	
			return -1;
		}
		
		if((node1 >= num_nodes) || (node2 >= num_nodes))
		{
			cout<<"node does not exist"<<endl;
			return -1;
		}

		return edge_weight[pair<int, int>(node1, node2)];
	}

	/* function to add edge weights to a given edge */
	void add_edge_weight(int node1, int node2, int& weight)
	{
		if(node1 > node2) swap(node1, node2); // guarentees node1 < node2

		/* sanity checks */
		if((find(adj[node1].begin(), adj[node1].end(), node2) == adj[node1].end()) || (find(adj[node2].begin(), adj[node2].end(), node1) == adj[node2].end()))
		{
			cout<<"edge does not exist between "<<node1<<" and "<<node2<<endl;			
			return;
		}
		
		if((node1 >= num_nodes) || (node2 >= num_nodes))
		{
			cout<<"node does not exist"<<endl;
			return;
		}

		edge_weight[pair<int, int>(node1, node2)] = weight;
	}

	pair<int, int> get_min_max_weight(int node1, int node2)
	{
		if(node1 > node2) swap(node1, node2); // guarentees node1 < node2

		/* sanity checks */
		if((find(adj[node1].begin(), adj[node1].end(), node2) == adj[node1].end()) || (find(adj[node2].begin(), adj[node2].end(), node1) == adj[node2].end()))
		{
			cout<<"edge does not exist between "<<node1<<" and "<<node2<<endl;
			return pair<int, int>(-1, -1);
		}
		
		if((node1 >= num_nodes) || (node2 >= num_nodes))
		{
			cout<<"node does not exist"<<endl;
			return pair<int, int>(-1, -1);
		}

		return min_max_weights[pair<int, int>(node1, node2)];
	}

	/* prints edge weights corresponding to the edges */
	void print_edge_weights()
	{
		for(auto p : edge_weight)
		{
			cout<<p.first.first<<" "<<p.first.second<<" -> "<<p.second<<endl;
		}
	}

	/* prints min_max edge weight limits corresponding to the edges */	
	void print_min_max_weights()
	{
		for(auto p : min_max_weights)
		{
			cout<<p.first.first<<" "<<p.first.second<<" -> "<<"Min : "<<p.second.first<<" ; "<<"Max : "<<p.second.second<<endl;
		}
	}

	/* prints num_nodes, num_edges and the adj list */
	void print_structure()
	{
		cout<<num_nodes<<endl;
		cout<<num_edges<<endl;
		for(int i = 0; i < num_nodes; i++)
		{
			cout<<i<<" -> ";
			for(auto ele : adj[i])
			{
				cout<<ele<<" ";
			}
			cout<<endl;
		}
	}
};

void hello_msg_sender(int& socket_fd, int& node_id, graph& connectivity_graph, parameters& params)
{
	char hello_msg[MSG_SIZE] = "HELLO ";
	strcat(hello_msg, to_string(node_id).c_str());

	struct sockaddr_in recvaddr;
	while(1)
	{
		/* sleep for hi seconds */
		sleep(params.m_hi);
		
		for(auto ele : connectivity_graph.adj[node_id])
		{
			if(node_id < ele)
			{
				memset(&recvaddr, 0, sizeof(recvaddr));

				recvaddr.sin_family = AF_INET;
				recvaddr.sin_port = htons(10000 + ele);
				recvaddr.sin_addr.s_addr = INADDR_ANY;

				sendto(socket_fd, (const char*)hello_msg, MSG_SIZE, MSG_CONFIRM, (const struct sockaddr*)&recvaddr, sizeof(recvaddr));
			}
		}
	}
}

void lsa_advertiser(int& socket_fd, int& node_id, graph& connectivity_graph, parameters& params)
{
	int sequence_number = 0;
	char lsa_message[MSG_SIZE];
	struct sockaddr_in recvaddr;
	while(1)
	{
		/* sleep for lsai seconds */
		sleep(params.m_lsai);

		memset(&lsa_message, 0, MSG_SIZE);
		strcat(lsa_message, "LSA ");
		strcat(lsa_message, to_string(node_id).c_str());
		strcat(lsa_message, " ");
		strcat(lsa_message, to_string(sequence_number).c_str());
		strcat(lsa_message, " ");
		strcat(lsa_message, to_string(connectivity_graph.adj[node_id].size()).c_str());
		strcat(lsa_message, " ");
		unique_lock<mutex> lock(connectivity_graph.edge_weight_mutex);
		for(auto neigh : connectivity_graph.adj[node_id])
		{
			strcat(lsa_message, to_string(neigh).c_str());
			strcat(lsa_message, " ");
			strcat(lsa_message, to_string(connectivity_graph.get_edge_weight(neigh, node_id)).c_str());
			strcat(lsa_message, " ");
		}
		lock.unlock();

		for(auto ele : connectivity_graph.adj[node_id])
		{
			memset(&recvaddr, 0, sizeof(recvaddr));

			recvaddr.sin_family = AF_INET;
			recvaddr.sin_port = htons(10000 + ele);
			recvaddr.sin_addr.s_addr = INADDR_ANY;

			//cout<<"lsa sent from "<<node_id<<" to "<<ele<<endl;
			sendto(socket_fd, (const char*)lsa_message, MSG_SIZE, MSG_CONFIRM, (const struct sockaddr*)&recvaddr, sizeof(recvaddr));
		}

		/* incrementing sequence number after every LSA advertisement */
		sequence_number++;
	}
}

void shortest_path(int& node_id, graph& connectivity_graph, parameters& params, string& outfile)
{	
	int time = 0;
	ofstream fp(outfile + "-" + to_string(node_id) + ".txt");
	while(1)
	{
		/* sleep for spfi seconds */
		sleep(params.m_spfi);
		time += params.m_spfi;

		int distance[connectivity_graph.num_nodes];
		int previous[connectivity_graph.num_nodes];
		for(int i = 0; i < connectivity_graph.num_nodes; i++)
		{
			distance[i] = numeric_limits<int>::max();
			previous[i] = -1;
		}
		distance[node_id] = 0;

		priority_queue<pair<int, int>, vector<pair<int, int> >, greater<pair<int, int> > > pq;
		pq.push(pair<int, int>(0, node_id));

		unique_lock<mutex> lock(connectivity_graph.edge_weight_mutex);
		while(!pq.empty())
		{
			int u = pq.top().second;
			pq.pop();

			for(auto v : connectivity_graph.adj[u])
			{
				if(distance[v] > distance[u] + connectivity_graph.get_edge_weight(u, v))
				{
					distance[v] = distance[u] + connectivity_graph.get_edge_weight(u, v);
					previous[v] = u;
					pq.push(pair<int, int>(distance[v], v));
				}
			}
		}
		lock.unlock();

		fp<<"Routing Table for Node No. "<<node_id<<" at time "<<time<<endl;
		fp<<"Destination Path Cost"<<endl;
		
		vector<int> path;
		int node;
		for(int i = 0; i < connectivity_graph.num_nodes; i++)
		{
			if(i != node_id)
			{
				fp<<i<<" ";
				node = i;
				path.clear();
				while(node != -1)
				{
					path.push_back(node);
					node = previous[node];
				}
				for(int j = path.size() - 1; j >= 0; j--)
				{
					j == 0 ? fp<<path[j]<<" " : fp<<path[j]<<"-";
				}
				fp<<distance[i]<<endl;
			}
		}
	}
}


/* random integer generator between a and b */
int random_int_between(int& a, int& b)
{
	return a + rand()%(b-a+1);
}


int main(int argc, char** argv)
{
	if(argc > 13)
    {
        cout<<"Usage :\n./ospf ­-i <node_id> -f <infile> -o <outfile> -h <hi> -a <lsai> -s <spfi>"<<endl;
		cout<<"Note : <node_id>, <infile> and <outfile> fields are compulsory"<<endl;
        exit(0);
    }
	string infile, outfile;
	int node_id; 
	int hi, lsai, spfi;

	hi = lsai = spfi = -1;	// to handle default parameter initialization

	bool node_id_flag, infile_flag, outfile_flag;
	node_id_flag = infile_flag = outfile_flag = false;
	for(int i = 1; i < argc; i++)
	{
		if((string)argv[i] == "-i")
		{
			i++; 
			node_id = stoi(argv[i]);
			if(node_id < 0)
			{
				cout<<"node_id must be >= 0"<<endl;
				return 0;
			}
			node_id_flag = true;
		}
		else if((string)argv[i] == "-f")
		{
			i++;
			infile = string(argv[i]);
			infile_flag = true;
		}
		else if((string)argv[i] == "-o")
		{
			i++;
			outfile = string(argv[i]);
			outfile_flag = true;
		}
		else if((string)argv[i] == "-h")
		{
			i++;
			hi = stoi(argv[i]);
		}
		else if((string)argv[i] == "-a")
		{
			i++;
			lsai = stoi(argv[i]);
		}
		else if((string)argv[i] == "-s")
		{
			i++;
			spfi = stoi(argv[i]);
		}
		else 
		{
			cout<<"unidentified flag "<<"\'"<<argv[i]<<"\'"<<endl;
			cout<<"Usage :\n./ospf ­-i <node_id> -f <infile> -o <outfile> -h <hi> -a <lsai> -s <spfi>"<<endl;
			cout<<"Note : <node_id>, <infile> and <outfile> fields are compulsory"<<endl;
			return 0;
		}
	}

	if(!(infile_flag & outfile_flag & node_id_flag))
	{
		cout<<"Usage :\n./ospf ­-i <node_id> -f <infile> -o <outfile> -h <hi> -a <lsai> -s <spfi>"<<endl;
		cout<<"Note : <node_id>, <infile> and <outfile> fields are compulsory"<<endl;
		return 0;
	}

	/* obtain and initialize parameters */
	parameters params(hi, lsai, spfi);
	
	/* read the input file to build the graph */
	int num_nodes, num_edges;
	ifstream fp(infile);
	fp>>num_nodes;
	fp>>num_edges;
	vector<pair<int, int> > edgelist;
	vector<pair<int, int> > min_max_weights; 
	for(int i = 0; i < num_edges; i++)
	{
		pair<int, int> p;
		pair<int, int> weights;
		fp>>p.first;
		fp>>p.second;
		edgelist.push_back(p);
		fp>>weights.first;
		fp>>weights.second;
		min_max_weights.push_back(weights);
	}

	/* generate the connectivity graph based on the input file given */
	graph connectivity_graph(num_nodes, num_edges, edgelist, min_max_weights);

	/* initialize the edge weights to their maximum possible values */
	for(int i = 0; i < connectivity_graph.num_edges; i++)
	{
		int weight = connectivity_graph.get_min_max_weight(edgelist[i].first, edgelist[i].second).second;
		connectivity_graph.add_edge_weight(edgelist[i].first, edgelist[i].second, weight);
	}
	
	/* setup a socket at port 10000 + node_id for all further communication */
	int socket_fd; 
	struct sockaddr_in addr, sender_addr;

	if((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		cout<<"socket creation failure"<<endl;
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(10000 + node_id);

	if((bind(socket_fd, (const struct sockaddr*)&addr, sizeof(addr))) < 0)
	{
		cout<<"bind failure"<<endl;
		return 0;
	}

	thread hello_thread(hello_msg_sender, ref(socket_fd), ref(node_id), ref(connectivity_graph), ref(params));
	thread lsa_thread(lsa_advertiser, ref(socket_fd), ref(node_id), ref(connectivity_graph), ref(params));
	thread shortest_path_thread(shortest_path, ref(node_id), ref(connectivity_graph), ref(params), ref(outfile));

	/* code of the reciever thread i.e. the main thread */
	int n;
	socklen_t len; 
	len = sizeof(sender_addr);

	vector<int> last_known_seqnum;
	for(auto ele : connectivity_graph.adj[node_id])
	{
		last_known_seqnum.push_back(-1);
	}

	char recvd_msg[MSG_SIZE];
	char reply_msg[MSG_SIZE];
	int sender_node_id;
	while(1)
	{
		memset(&recvd_msg, 0, MSG_SIZE);
		n = recvfrom(socket_fd, (char *)recvd_msg, MSG_SIZE, MSG_WAITALL, (struct sockaddr*)&sender_addr, &len);
		recvd_msg[n] = '\0';
		if(strncmp(recvd_msg, "LSA", 3) == 0)
		{
			strcpy(reply_msg, recvd_msg);
		}
		char* token = strtok(recvd_msg, " ");
		if(strcmp(token, "HELLO") == 0)
		{
			token = strtok(NULL, " ");
			sender_node_id = atoi(token);
			//cout<<"recieved hello from "<<sender_node_id<<endl;

			/* generate random number weight and add it to the graph */
			pair<int, int> p = connectivity_graph.get_min_max_weight(node_id, sender_node_id);
			unique_lock<mutex> lock(connectivity_graph.edge_weight_mutex);
			int random_weight = random_int_between(p.first, p.second);
			connectivity_graph.add_edge_weight(node_id, sender_node_id, random_weight);
			lock.unlock();

			/* generate hello reply and send it back */
			memset(&reply_msg, 0, MSG_SIZE);
			strcat(reply_msg, "HELLOREPLY ");
			strcat(reply_msg, to_string(node_id).c_str());
			strcat(reply_msg, " ");
			strcat(reply_msg, to_string(sender_node_id).c_str());
			strcat(reply_msg, " ");
			strcat(reply_msg, to_string(connectivity_graph.get_edge_weight(node_id, sender_node_id)).c_str()); // no need to acquire the lock as no other thread can currently write to the edge_weight map

			//cout<<"sending helloreply from "<<node_id<<" to "<<sender_node_id<<endl;
			sendto(socket_fd, (const char*)reply_msg, MSG_SIZE, MSG_CONFIRM, (const struct sockaddr*)&sender_addr, sizeof(sender_addr));
		}
		else if(strcmp(token, "HELLOREPLY") == 0)
		{
			//cout<<"recieved helloreply"<<endl;
			token = strtok(NULL, " ");
			sender_node_id = atoi(token);

			token = strtok(NULL, " "); // field contains current node id, not needed, can be discarded
			token = strtok(NULL, " ");
			
			unique_lock<mutex> lock(connectivity_graph.edge_weight_mutex);
			int weight = atoi(token);
			connectivity_graph.add_edge_weight(node_id, sender_node_id, weight);
			lock.unlock();
		}
		else if(strcmp(token, "LSA") == 0)
		{
			token = strtok(NULL, " ");
			int src_id = atoi(token);
			//cout<<"recieved lsa with srcid "<<src_id<<endl;

			if(src_id == node_id) continue;	// discard the LSA message

			token = strtok(NULL, " ");
			int seqnum = atoi(token);
			if(seqnum <= last_known_seqnum[src_id]) continue;	// discard the LSA message

			last_known_seqnum[src_id] = seqnum;
			token = strtok(NULL, " ");
			int length = atoi(token);
			int n_node_id, weight;
			for(int i = 0; i < length; i++)
			{
				token = strtok(NULL, " ");
				n_node_id = atoi(token);
				token = strtok(NULL, " ");
				weight = atoi(token);

				unique_lock<mutex> lock(connectivity_graph.edge_weight_mutex);
				connectivity_graph.add_edge_weight(src_id, n_node_id, weight);
				lock.unlock();
			}

			/* forward LSA message to all neighbours except the current sender */
			struct sockaddr_in recv_addr; 
			for(auto n_id : connectivity_graph.adj[node_id])
			{
				if(htons(10000 + n_id) != sender_addr.sin_port)
				{
					memset(&recv_addr, 0, sizeof(recv_addr));
					recv_addr.sin_family = AF_INET;
					recv_addr.sin_port = htons(10000 + n_id);
					recv_addr.sin_addr.s_addr = INADDR_ANY;

					//cout<<"forwarding recieved lsa to "<<n_id<<endl;
					sendto(socket_fd, (const char*)reply_msg, MSG_SIZE, MSG_CONFIRM, (const struct sockaddr*)&recv_addr, sizeof(recv_addr));
				}
			}
		}
	}
	
    return 0;
}