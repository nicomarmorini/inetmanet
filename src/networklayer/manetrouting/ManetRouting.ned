
package inet.networklayer.manetrouting;


module ManetRouting
{
    parameters:
        @display("bgb=160,88");
    gates:
        input from_ip;
        output to_ip;
    submodules:
        // the name of manet routing manager module must be "manetmanager", the routing protocols is ManetRouting.ned
        manetmanager: ManetManager {
            parameters:
                // Manager parameters
                // eliminate in the routing table all the initial reference to wlan interfaces
                manetActive = true;
                //		manetPurgeRoutingTables=true,
                manetPurgeRoutingTables = true;
                @display("p=90,48;b=40,24");
        }
    connections allowunconnected:
        manetmanager.from_ip <-- from_ip;
        manetmanager.to_ip --> to_ip;
}






