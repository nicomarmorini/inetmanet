package inet.networklayer.manetrouting;

simple OLSR_ETX
{
    parameters:
        int Willingness = default(3); //ms
        int Hello_ival = default(2); // sec
        int Tc_ival = default(5); // sec
        int Mid_ival = default(5); // sec
        int use_mac = default(0);
        int Mpr_algorithm = default(1);
        int routing_algorithm = default(1);
        int Link_quality = default(2);
        bool Fish_eye = default(false);
        int Tc_redundancy = default(3);
        bool Link_delay = default(true);
        double C_alpha = default(0.2);

    gates:
        input from_ip;
        output to_ip;
}

