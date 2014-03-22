#include <thread>
#include <chrono>
#include <iostream>
#include <ctime>
#include <string>
#include <vector>

#include "MOOS/libMOOS/Comms/MOOSAsyncCommClient.h"
#include "MOOS/libMOOS/Utils/MOOSUtilityFunctions.h"

#include "observation.pb.h"

#define CONSUME_MS double(1.0/100.0)
#define PRODUCE_MS int( (1.0/1.0)*1000)


bool OnConnectConsumer(void *pParam);
bool OnConnectStats(void *pParam);
bool OnMail(void *pParam); // only called when subscribed event isn't handled by an Active Queue
bool onConsumerMail(CMOOSMsg &M, void *param);
bool onStatsMail(CMOOSMsg &M, void *param);

void receiverThread(std::string clientName, bool (*onConnectCallback)(void*),
                    std::string msgName, bool (*mailCallback)(CMOOSMsg &, void*));
void producerThread();

int main(int argc , char * argv [])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION; // Verify library matches included headers

    std::thread p (producerThread);
    std::thread c0 (receiverThread, "CONSUMER_0", OnConnectConsumer, "shared_var", onConsumerMail);
    std::thread c1 (receiverThread, "CONSUMER_1", OnConnectConsumer, "shared_var", onConsumerMail);
    std::thread sm0 (receiverThread, "STATS_MONITOR_0", OnConnectStats, "latency_CONSUMER_0", onStatsMail);
    std::thread sm1 (receiverThread, "STATS_MONITOR_1", OnConnectStats, "latency_CONSUMER_1", onStatsMail);

    while (true); // Could do [thread].join();, but this is the same effect

    // We never get here...but shown for completeness
    google::protobuf::ShutdownProtobufLibrary(); // Delete all global objects allocated by libprotobuf

    return 0;
}


void receiverThread(std::string clientName, bool (*onConnectCallback)(void*),
                    std::string msgName, bool (*mailCallback)(CMOOSMsg &, void*))
{
    MOOS::MOOSAsyncCommClient client;
    std::vector<void*> params;
    params.push_back(static_cast<void*>(&clientName));
    params.push_back(static_cast<void*>(&client));

    client.SetOnConnectCallBack(onConnectCallback, &client);
    client.SetOnMailCallBack(OnMail, &client);
    client.AddActiveQueue("testing_callback", mailCallback, static_cast<void*>(&params));
    client.AddMessageRouteToActiveQueue("testing_callback", msgName);

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

        // Create an observation
        moos_hello::Observation observation;
        std::string name = "" + std::to_string(MOOSTime(false));
        observation.set_name(name);
        moos_hello::Point *point = new moos_hello::Point;
        point->set_x(123);
        point->set_y(234);
        observation.set_allocated_point(point);

        // Print out the observation
        std::cout << "Observation Sent: " << observation.DebugString() << std::endl;

        // Send the compressed observation
        client.Notify("shared_var", observation.SerializeAsString());
    }
}

bool OnConnectStats(void *pParam)
{
    CMOOSCommClient* pC = reinterpret_cast<CMOOSCommClient*> (pParam);

    pC->Register("*", "CONSUMER_0", CONSUME_MS);
    pC->Register("*", "CONSUMER_1", CONSUME_MS);

    return true;
}

bool OnConnectConsumer(void *pParam)
{
    CMOOSCommClient* pC = reinterpret_cast<CMOOSCommClient*> (pParam);

    pC->Register("*", "PRODUCER", CONSUME_MS);

    return true;
}

// Note: this only appears to be called when the event is not captured in an active queue
bool OnMail(void *pParam)
{
    CMOOSCommClient* pC = reinterpret_cast<CMOOSCommClient*> (pParam);

    MOOSMSG_LIST M;
    MOOSMSG_LIST::iterator q;

    pC->Fetch(M); // Get new mail

    // Loop through the mail and print out details
    for (q = M.begin () ; q!=M.end () ; q++)
    {
        std::cout << "Message from: " << q->GetSource() << ", Name: " << q->GetName() << std::endl;
    }

    return true;
}

bool onConsumerMail(CMOOSMsg &M, void *param)
{
    std::stringstream var_name;
    std::vector<void*> *params = static_cast<std::vector<void*>*>(param);
    std::string clientName = *static_cast<std::string*>((*params)[0]);
    CMOOSCommClient *pClient = reinterpret_cast<CMOOSCommClient*>((*params)[1]);

    var_name << "latency_" << clientName;

    double receiveTime = MOOSTime();
    double sentTime = M.GetTime();
    double latency = receiveTime - sentTime;

    // Send the message latency
    pClient->Notify(var_name.str(), latency);

    // Print out the received Observation
    std::string data = M.GetAsString();
    moos_hello::Observation observation;
    if ( observation.ParseFromArray(&data[0], data.size()) )
        std::cout << "Observation Received: " << observation.DebugString();

    return true;
}

bool onStatsMail(CMOOSMsg &M, void *param)
{
    std::stringstream var_name;
    std::vector<void*> *params = static_cast<std::vector<void*>*>(param);
    std::string clientName = *static_cast<std::string*>((*params)[0]);

    std::cout << "Latency (" << clientName << "): " << M.GetDouble()*1000 << " us" << std::endl;


    return true;
}
