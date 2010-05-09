package inet.networklayer.manetrouting;


simple ManetManagerStatic
{
    parameters:
        // Manager parameters
        string routingProtocol;
        bool manetActive;
        bool manetPurgeRoutingTables;

    gates:
        input from_ip;
        output to_ip;
        output to_aodv;
        input from_aodv;
        output to_dsr;
        input from_dsr;
        output to_dymo;
        input from_dymo;
        output to_olsr;
        input from_olsr;
        output to_dsdv;
        input from_dsdv;
        output to_geo;
        input from_geo;
}

