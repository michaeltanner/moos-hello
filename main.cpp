#include <thread>
#include <chrono>
#include <iostream>
#include <ctime>
#include <string>

#include "MOOS/libMOOS/Comms/MOOSAsyncCommClient.h"
#include "MOOS/libMOOS/Utils/MOOSUtilityFunctions.h"

#define CONSUME_MS double(1.0/100.0)
#define PRODUCE_MS int( (1.0/1.0)*1000)


bool OnConnectConsumer(void *pParam);
bool OnConnectStatsMonitor(void *pParam);
bool OnMail(void *pParam);

void receiverThread(std::string clientName, bool (*onConnectCallback)(void*));
void producerThread();

int main(int argc , char * argv [])
{
    std::thread p (producerThread);
    std::thread c0 (receiverThread, "CONSUMER_0", OnConnectConsumer);
    std::thread c1 (receiverThread, "CONSUMER_1", OnConnectConsumer);
    std::thread sm (receiverThread, "STATS_MONITOR", OnConnectStatsMonitor);

    while (true); // Could do [thread].join();, but this is the same effect

    return 0;
}

void receiverThread(std::string clientName, bool (*onConnectCallback)(void*))
{
    MOOS::MOOSAsyncCommClient client;

    client.SetOnConnectCallBack(onConnectCallback, &client);
    client.SetOnMailCallBack(OnMail, &client);
    client.Run("localhost", 9000, clientName);

    while (true); // Wait forever
}

void producerThread()
{
    MOOS::MOOSAsyncCommClient client;

    client.Run("localhost", 9000, "PRODUCER");

    int i = 0;
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(PRODUCE_MS));

        client.Notify("shared_var", std::to_string(i++));
    }
}

bool OnConnectConsumer(void *pParam)
{
    CMOOSCommClient* pC = reinterpret_cast<CMOOSCommClient*> (pParam);

    pC->Register("shared_var", "*", CONSUME_MS);

    return true;
}

bool OnConnectStatsMonitor(void *pParam)
{
    CMOOSCommClient* pC = reinterpret_cast<CMOOSCommClient*> (pParam);

    pC->Register("latency_*", "*", CONSUME_MS);

    return true;
}

bool OnMail(void *pParam)
{
    CMOOSCommClient* pC = reinterpret_cast<CMOOSCommClient*> (pParam);

    MOOSMSG_LIST M;
    MOOSMSG_LIST::iterator q;

    pC->Fetch(M); // Get new mail

    // Loop through the mail and print out details
    for (q = M.begin () ; q!=M.end () ; q++)
    {
        // Figure out what type of variable was updated
        if (q->GetName() == "shared_var")
        {
            std::stringstream var_name;
            var_name << "latency_" << std::this_thread::get_id();

            double receiveTime = MOOSTime();
            double sentTime = q->GetTime();
            double latency = receiveTime - sentTime;

            pC->Notify(var_name.str(), latency);
        }
        else if (q->GetName().substr(0, 7) == "latency")
        {
            // Print out the message latency
            std::cout << q->GetName() << ": " << q->GetDouble()*1000 << " us" << std::endl;
        }
    }

    return true;
}
