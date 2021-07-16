#include<bits/stdc++.h>
using namespace std;

#define RWS (double)1024 // Reciever Window Size = 1 MB = 1000 KB
#define MSS (double)1    // Maximum Segment Size = 1 KB
#define CTF 0.5   // Congestion Window Threshold = Congestion Threshold Fraction * Current Congestion Window Size
#define KI_TOP 4
#define KI_BOT 1
#define KM_TOP 2
#define KM_BOT 0.5
#define KN_TOP 2
#define KN_BOT 0.5
#define KF_TOP 0.5
#define KF_BOT 0.1
#define PS_TOP_EX 1
#define PS_BOT_EX 0
#define KI_DEF 1
#define KM_DEF 1
#define KN_DEF 1

class parameters
{
public: 
    double m_Ki, m_Km, m_Kn, m_Kf, m_Ps;
    parameters(double Kf, double Ps, double Ki, double Km, double Kn)
    {
        m_Ki = (Ki == -1)? KI_DEF : Ki; 
        m_Km = (Km == -1)? KI_DEF : Km; 
        m_Kn = (Kn == -1)? KI_DEF : Kn; 
        m_Kf = Kf;
        m_Ps = Ps;
        bool flag = 0;
        if(!((m_Ki <= KI_TOP) && (m_Ki >= KI_BOT)))
        {
            cout<<"Ki value not in range ["<<KI_BOT<<", "<<KI_TOP<<"]"<<endl;
            flag = 1;
        }
        if(!((m_Km <= KM_TOP) && (m_Km >= KM_BOT)))
        {
            cout<<"Km value not in range ["<<KM_BOT<<", "<<KM_TOP<<"]"<<endl;
            flag = 1;
        }
        if(!((m_Kn <= KN_TOP) && (m_Kn >= KN_BOT)))
        {
            cout<<"Kn value not in range ["<<KN_BOT<<", "<<KN_TOP<<"]"<<endl;
            flag = 1;
        }
        if(!((m_Kf <= KF_TOP) && (m_Kf >= KF_BOT)))
        {
            cout<<"Kf value not in range ["<<KF_BOT<<", "<<KF_TOP<<"]"<<endl;
            flag = 1;
        }
        if(!((m_Ps < PS_TOP_EX) && (m_Ps > PS_BOT_EX)))
        {
            cout<<"Ps value not in range ("<<PS_BOT_EX<<", "<<PS_TOP_EX<<")"<<endl;
            flag = 1;
        }
        if(flag == 1)
        {
            exit(0);
        }
        else 
        {
            cout<<"parameters set successfully"<<endl;
            cout<<"Ki = "<<m_Ki<<endl;
            cout<<"Km = "<<m_Km<<endl;
            cout<<"Kn = "<<m_Kn<<endl;
            cout<<"Kf = "<<m_Kf<<endl;
            cout<<"Ps = "<<m_Ps<<endl;
        }
    }
};

int main(int argc, char** argv)
{
    if(argc > 15)
    {
        cout<<"Usage :\n./cw -­i <double> -­m <double> ­-n <double> ­-f <double> -­s <double> -­T <int> -­o outfile"<<endl;
        exit(0);
    }
    double Ki, Km, Kn, Kf, Ps, T;
    string outfile;
    
    Ki = Km = Kn = -1;  // default values

    int count = 0;
    for(int i = 1; i < argc; i++)
    {
        if((string)argv[i] == "-i")
        {
            i++;
            Ki = stod(argv[i]);
        }
        else if((string)argv[i] == "-m")
        {
            i++;
            Km = stod(argv[i]);
        }
        else if((string)argv[i] == "-n")
        {
            i++;
            Kn = stod(argv[i]);
        }
        else if((string)argv[i] == "-f")
        {
            i++;
            Kf = stod(argv[i]);
            count++;
        }
        else if((string)argv[i] == "-s")
        {
            i++;
            Ps = stod(argv[i]);
            count++;
        }
        else if((string)argv[i] == "-T")
        {
            i++;
            T = stoi(argv[i]);
            count++;
        }
        else if((string)argv[i] == "-o")
        {
            i++;
            outfile = string(argv[i]);
            count++;
        }
        else 
        {
            cout<<"unidentified flag "<<"\'"<<argv[i]<<"\'"<<endl;
            cout<<"Usage :\n./cw ­-i <double> ­-m <double> -­n <double> ­-f <double> -­s <double> -­T <int> ­-o outfile"<<endl;
            cout<<"Parameter Ranges : "<<endl;
            cout<<"Ki in ["<<KI_BOT<<", "<<KI_TOP<<"]"<<endl;
            cout<<"Km in ["<<KM_BOT<<", "<<KM_TOP<<"]"<<endl;
            cout<<"Kn in ["<<KN_BOT<<", "<<KN_TOP<<"]"<<endl;
            cout<<"Kf in ["<<KF_BOT<<", "<<KF_TOP<<"]"<<endl;
            cout<<"Ps in ("<<PS_BOT_EX<<", "<<PS_TOP_EX<<")"<<endl;
            exit(0);
        }
    }
    if(count != 4)
    {
        cout<<"parameters -s (Ps), -f (Kf), -o (output filename), -T (number of updates) have to specified"<<endl;
        exit(0);
    }
    parameters params(Kf, Ps, Ki, Km, Kn);

    /* bernoulli_distribution random variable generator for P_s */
    mt19937 generator(3);
    bernoulli_distribution distribution(params.m_Ps);

    /* beginning of simulation */
    cout<<"Simulation Running...."<<endl;
    vector<double> cwnd_values;
    vector<double> round_values; 

    double ssthresh = numeric_limits<double>::max();
    double cwnd = params.m_Ki * MSS;

    cwnd_values.push_back(cwnd);
    while(cwnd_values.size() - 1 <= T)
    {
        double current_cwnd = cwnd;
        round_values.push_back(current_cwnd);
        for(int i = 0; i < ceil(current_cwnd/MSS); i++) 
        {
            /* if timeout occurs before acknowledgement for some segment sent in the earlier round */
            if(distribution(generator))
            {
                /* set congestion threshold to CTF fraction of current congestion window size */ 
                ssthresh = CTF * cwnd;     

                /* move back to slow start phase */
                cwnd = max(MSS, params.m_Kf * cwnd);    

                cwnd_values.push_back(cwnd);
                break;
            }
            /* if acknowledgement was recieved for all segments sent, exponential growth occurs when congestion window size <= ssthresh and linear growth for congestion window size > ssthresh */
            cwnd = (cwnd > ssthresh) ? min(cwnd + params.m_Kn * MSS * (MSS/cwnd), RWS) /* linear update */ : min(cwnd + params.m_Km * MSS, RWS) /* exponential update */;  

            cwnd_values.push_back(cwnd);
        }
    }

    cout<<"Simulation completed!!"<<endl;
    ofstream fp1(outfile);
    for(auto ele : cwnd_values)
    {
        fp1<<ele<<"\n";
    }

    ofstream fp2(outfile + "_params");
    fp2<<params.m_Ki<<endl;
    fp2<<params.m_Km<<endl;
    fp2<<params.m_Kn<<endl;
    fp2<<params.m_Kf<<endl;
    fp2<<params.m_Ps<<endl;
    
    ofstream fp3(outfile + "_mean_throughput");
    double temp = 0;
    for(auto ele : round_values)
    {
        temp += ele;
    }
    fp3<<temp/round_values.size();
    cout<<"Output files generated"<<endl;

    return 0;
}