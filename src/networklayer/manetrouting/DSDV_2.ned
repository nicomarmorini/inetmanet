
package inet.networklayer.manetrouting;

simple DSDV_2
{
    parameters:
      double hellomsgperiod_DSDV @unit("s")=default(5);
      string netmask = default("255.255.0.0");
      int RNGseed_DSDV = default(1);
      double MaxVariance_DSDV=default(1);
      double timetolive_routing_entry @unit("s")=default(5);
    gates:
      input ip_toDSDV;
      output DSDV_toip;
}
