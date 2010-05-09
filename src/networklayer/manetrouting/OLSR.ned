package inet.networklayer.manetrouting;

simple OLSR
{
    parameters:
        int Willingness = default(3); //ms
        int Hello_ival = default(2); // sec
        int Tc_ival = default(5); // sec
        int Mid_ival = default(5); // sec
        int use_mac = default(0);
        bool UseIndex = default(false); // EXPERIMENTAL
    gates:
        input from_ip;
        output to_ip;
}



