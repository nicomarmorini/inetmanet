package inet.networklayer.manetrouting;



simple DYMOUM
{
    parameters:
        bool no_path_acc_= default(false);
        bool reissue_rreq_= default(false);
        bool s_bit_= default(false);
        int hello_ival_= default(0); // 0 No hello, link layer feedback
        int MaxPktSec= default(20);
        int NetDiameter= default(10);
        int RouteTimeOut= default(3000); // msecond
        int RouteDeleteTimeOut= default(3000*5); //5*RouteTimeOut
        int RREQWaitTime= default(1000); // msecond
        int RREQTries= default(3); // Rreq reties before fail
// Experimental
		bool promiscuous= default(false); // Experimental
        int noRouteBehaviour= default(1); // experimental
        bool intermediateRREP= default(false); // experimental
        bool RREQattachPacket= default(false); // experimental
        volatile double broadCastDelay @unit("s") = default (uniform(0s,0.005s));
        volatile double uniCastDelay @unit("s") = default (0s);
        bool EqualDelay =default(true);
        bool UseIndex=default(false);

    gates:
        input from_ip;
        output to_ip;
}



 