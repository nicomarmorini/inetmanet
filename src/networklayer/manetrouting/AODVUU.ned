package inet.networklayer.manetrouting;

simple AODVUU
{
    parameters:
        bool log_to_file = default(false);
        bool hello_jittering = default(true);
        bool optimized_hellos = default(true);
        bool expanding_ring_search = default(true);
        bool local_repair = default(true);
        bool rreq_gratuitous = default(true);
        bool debug = default(false);
        int rt_log_interval = default(0);
        int unidir_hack = default(0);
        int internet_gw_mode = default(0);  // not work, to do
        int receive_n_hellos = default(1);
        int wait_on_reboot = default(0);
        int ratelimit= default(1000); // msecond
        string internet_gw_address = default("0.0.0.0"); // not work, to do
        int active_timeout= default(6000); // msecon
        bool llfeedback= default(false); // Use hello
        volatile double broadCastDelay @unit("s") = default (uniform(0s,0.005s));
        volatile double uniCastDelay @unit("s") = default (0s);
        bool EqualDelay =default(true);
        bool UseIndex=default(false);
    gates:
        input from_ip;
        output to_ip;
}

