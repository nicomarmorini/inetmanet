
package inet.networklayer.manetrouting;

simple GEO
{
    parameters:
      double routeRefreshInterval @unit("s")=default(1s);
      double ftableRefreshInterval @unit("s")=default(1s);
      double GTmapsRefreshInterval @unit("s")=default(1s);
      int maxDistance @unit("m") = default(100m);
      int nodeClass = default(1); // it defines the class of a node that follows the policy STATIC, it must be a number between 0 and numInterfaces 
      string policy = default("RANDOM"); // RANDOM, RAMEN, STATIC or GT
      string ifChoice = default("PREALLOC"); // RANDOM, PREALLOC, QUEUE
      string fitFunction = default("LINEAR"); // LINEAR, EXPONENTIAL, EXPONENTIAL_RELATIVE, POLYNOMIAL
    gates:
      input from_ip;
      output to_ip;
}
